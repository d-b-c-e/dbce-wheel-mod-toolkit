#requires -Version 7.2
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PlanPath,
    [string]$RepositoryRoot = (Join-Path $PSScriptRoot '../..'),
    [string]$OutputDirectory,
    [string]$Dotnet = 'dotnet',
    [string]$Python = 'python',
    [string]$Gxx = 'g++',
    [string]$Git = 'git'
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $root ('build/local-checks/' + [guid]::NewGuid().ToString('N')) }
$output = [IO.Path]::GetFullPath($OutputDirectory)
# A receipt never replaces an existing run or owner's artifact.
if (Test-Path -LiteralPath $output) { throw 'Output directory already exists; choose a new run directory' }
New-Item -ItemType Directory -Path $output -ErrorAction Stop | Out-Null
$pwsh = Join-Path $PSHOME $(if ($IsWindows) {'pwsh.exe'} else {'pwsh'})
$bindings = @{root=$root; output=$output; pwsh=$pwsh; dotnet=$Dotnet; python=$Python; gxx=$Gxx; git=$Git; cmd='cmd.exe'}
$receipt = [ordered]@{
    schema='dbce.local-check-receipt'; version=1; startedUtc=[DateTime]::UtcNow.ToString('o');
    endedUtc=$null; status='failed'; error=$null; plan=$null; sourceBefore=$null; sourceAfter=$null;
    sourceStableAndClean=$false; platform=[Runtime.InteropServices.RuntimeInformation]::OSDescription;
    architecture=[Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString();
    powershell=$PSVersionTable.PSVersion.ToString();
    runnerSha256=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant();
    steps=@(); artifacts=@(); releaseQualified=$false
}
function Run-Process([string]$Program, [string[]]$Arguments, [int]$Seconds=900) {
    $start=[Diagnostics.ProcessStartInfo]::new()
    $start.FileName=$Program; $start.WorkingDirectory=$root; $start.UseShellExecute=$false
    $start.CreateNoWindow=$true; $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
    foreach($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    $process=[Diagnostics.Process]::new(); $process.StartInfo=$start
    try {
        if(-not $process.Start()) { throw 'Process did not start' }
        $stdout=$process.StandardOutput.ReadToEndAsync(); $stderr=$process.StandardError.ReadToEndAsync()
        if(-not $process.WaitForExit($Seconds*1000)) {
            $process.Kill($true); $process.WaitForExit()
            return [pscustomobject]@{exitCode=$null; timedOut=$true; stdout=$stdout.GetAwaiter().GetResult(); stderr=$stderr.GetAwaiter().GetResult()}
        }
        return [pscustomobject]@{exitCode=$process.ExitCode; timedOut=$false; stdout=$stdout.GetAwaiter().GetResult(); stderr=$stderr.GetAwaiter().GetResult()}
    } finally { $process.Dispose() }
}
function Resolve-Program([string]$Value) {
    if(Test-Path -LiteralPath $Value -PathType Leaf) { return (Resolve-Path -LiteralPath $Value).Path }
    return (Get-Command -Name $Value -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
}
function Source-Identity {
    $values=@{}
    foreach($item in @(@('commit','rev-parse','HEAD'), @('tree','rev-parse','HEAD^{tree}'), @('status','status','--porcelain=v1','--untracked-files=all'))) {
        $result=Run-Process $script:gitProgram $item[1..($item.Count-1)] 30
        if($result.timedOut -or $result.exitCode -ne 0) { throw ('Git provenance refused: ' + $result.stderr) }
        $values[$item[0]]=$result.stdout.TrimEnd()
    }
    return [ordered]@{commit=$values.commit; tree=$values.tree; clean=($values.status.Length -eq 0); status=$values.status}
}
function Expand-Argument([string]$Value) {
    return [regex]::Replace($Value,'\{([a-z]+)\}', {
        param($match)
        if(-not $bindings.ContainsKey($match.Groups[1].Value)) { throw 'Unknown plan token' }
        return $bindings[$match.Groups[1].Value]
    })
}
try {
    $absolutePlan=(Resolve-Path -LiteralPath $PlanPath).Path
    $plan=Get-Content -LiteralPath $absolutePlan -Raw | ConvertFrom-Json
    if($plan.schema -cne 'dbce.local-check-plan' -or $plan.version -isnot [long] -or $plan.version -ne 1 -or
       $plan.lane -isnot [string] -or [string]::IsNullOrWhiteSpace($plan.lane) -or
       $plan.steps -isnot [array] -or $plan.steps.Count -eq 0 -or $plan.steps.Count -gt 100) { throw 'Unsupported or empty check plan' }
    $ids=@{}
    foreach($step in $plan.steps) {
        if($step.id -isnot [string] -or $step.id -cnotmatch '^[a-z0-9][a-z0-9-]{0,63}$' -or $ids.ContainsKey($step.id) -or
           $step.program -isnot [string] -or -not $step.program -or $step.arguments -isnot [array] -or
           @($step.arguments | Where-Object { $_ -isnot [string] }).Count -ne 0) { throw 'Invalid or duplicate check step' }
        $ids[$step.id]=$true
        $receipt.steps += [ordered]@{id=$step.id; status='not-run'; program=$step.program; arguments=$step.arguments; exitCode=$null; timedOut=$false; durationSeconds=$null; executableSha256=$null; stdout=$null; stderr=$null}
    }
    foreach($artifact in $plan.artifacts) {
        if($artifact -isnot [string] -or -not $artifact -or $artifact.Contains('\') -or $artifact.Contains(':') -or
           @($artifact.Split('/') | Where-Object { $_ -in @('','..','.') }).Count) { throw 'Unsafe artifact path in plan' }
    }
    $receipt.plan=[ordered]@{path=$absolutePlan; sha256=(Get-FileHash -LiteralPath $absolutePlan -Algorithm SHA256).Hash.ToLowerInvariant(); lane=$plan.lane}
    $script:gitProgram=Resolve-Program $Git
    $receipt.sourceBefore=Source-Identity
    for($i=0;$i -lt $plan.steps.Count;$i++) {
        $step=$plan.steps[$i]; $record=$receipt.steps[$i]
        $record.status='running'
        $program=Resolve-Program (Expand-Argument $step.program)
        $arguments=@($step.arguments | ForEach-Object { Expand-Argument $_ })
        $record.program=$program; $record.arguments=$arguments
        $record.executableSha256=(Get-FileHash -LiteralPath $program -Algorithm SHA256).Hash.ToLowerInvariant()
        Write-Host ('Running ' + $step.id)
        $watch=[Diagnostics.Stopwatch]::StartNew()
        $result=Run-Process $program $arguments
        $watch.Stop(); $record.durationSeconds=$watch.Elapsed.TotalSeconds
        $record.exitCode=$result.exitCode; $record.timedOut=$result.timedOut
        foreach($stream in @('stdout','stderr')) {
            $path=Join-Path $output ($step.id+'.'+$stream+'.log')
            [IO.File]::WriteAllText($path,$result.$stream,[Text.UTF8Encoding]::new($false))
            $record[$stream]=[ordered]@{file=[IO.Path]::GetFileName($path); sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
        }
        if($result.timedOut -or $result.exitCode -ne 0) { $record.status='failed'; throw ('Check failed: '+$step.id+'; see its logs') }
        $record.status='passed'
    }
    $receipt.status='passed'
} catch {
    foreach($record in $receipt.steps) { if($record.status -eq 'running') { $record.status='failed' } }
    $receipt.error=$_.Exception.Message
    Write-Host ('Local checks stopped: ' + $receipt.error)
} finally {
    if($receipt.sourceBefore) {
        try {
            $receipt.sourceAfter=Source-Identity
            $receipt.sourceStableAndClean=($receipt.sourceBefore.clean -and $receipt.sourceAfter.clean -and
                $receipt.sourceBefore.commit -ceq $receipt.sourceAfter.commit -and $receipt.sourceBefore.tree -ceq $receipt.sourceAfter.tree)
            if(-not $receipt.sourceStableAndClean) { $receipt.status='failed'; $receipt.error=(@($receipt.error,'Source was dirty or changed; results are not qualified for this commit') | Where-Object { $_ }) -join '; ' }
        } catch { $receipt.status='failed'; $receipt.error=$_.Exception.Message }
    }
    if($receipt.sourceBefore) {
        foreach($artifact in $plan.artifacts) {
            try {
                $path=Join-Path $root $artifact
                $receipt.artifacts += [ordered]@{path=$artifact; sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256 -ErrorAction Stop).Hash.ToLowerInvariant(); bytes=(Get-Item -LiteralPath $path -ErrorAction Stop).Length}
            } catch { $receipt.status='failed'; $receipt.error=(@($receipt.error,$_.Exception.Message) | Where-Object { $_ }) -join '; ' }
        }
    }
    $receipt.endedUtc=[DateTime]::UtcNow.ToString('o')
    $receipt | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $output 'receipt.json') -Encoding utf8
    Write-Host ('Receipt: ' + (Join-Path $output 'receipt.json'))
}
if($receipt.status -ne 'passed') { exit 1 }
exit 0
