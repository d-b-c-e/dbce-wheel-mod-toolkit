#requires -Version 7.2
$ErrorActionPreference='Stop'
$runner=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../local/Invoke-LocalChecks.ps1'))
$hostPath=Join-Path $PSHOME $(if($IsWindows){'pwsh.exe'}else{'pwsh'})
$fixtureRoot=Join-Path ([IO.Path]::GetTempPath()) ('local-check-fixture-'+[guid]::NewGuid().ToString('N'))
$repo=Join-Path $fixtureRoot 'space repo'
$script:assertions=0
function Assert($Passed,[string]$Message) { $script:assertions++;if(-not $Passed){throw $Message} }
function Run-Case([string]$Name,[array]$Steps,[string[]]$Extra=@()) {
    $planPath=Join-Path $fixtureRoot ($Name+'.json');$out=Join-Path $fixtureRoot ($Name+'-output')
    [ordered]@{schema='dbce.local-check-plan';version=1;lane='synthetic-fixture';artifacts=@('fixture.txt');steps=$Steps} | ConvertTo-Json -Depth 8 | Set-Content $planPath -Encoding utf8
    $text=@(& $hostPath -NoProfile -File $runner -RepositoryRoot $repo -PlanPath $planPath -OutputDirectory $out @Extra 2>&1)
    $code=$LASTEXITCODE
    $receipt=Get-Content (Join-Path $out 'receipt.json') -Raw | ConvertFrom-Json
    [pscustomobject]@{Code=$code;Receipt=$receipt;Directory=$out;Text=($text -join "`n")}
}
try {
    New-Item -ItemType Directory -Path $repo | Out-Null
    [IO.File]::WriteAllText((Join-Path $repo 'fixture.txt'),'original')
    [IO.File]::WriteAllText((Join-Path $repo 'throw.ps1'),"Write-Output 'before failure';throw 'intentional script failure'")
    [IO.File]::WriteAllText((Join-Path $repo 'failure child.cmd'),"@echo off`r`nexit /b 7`r`n")
    & git -C $repo init --quiet;if($LASTEXITCODE -ne 0){throw 'Fixture git initialization failed'}
    & git -C $repo add .;if($LASTEXITCODE -ne 0){throw 'Fixture staging failed'}
    & git -C $repo -c user.name=Fixture -c user.email=fixture@example.invalid commit --quiet -m fixture
    if($LASTEXITCODE -ne 0){throw 'Fixture commit failed'}
    $ok=@{id='success';program='{pwsh}';arguments=@('-NoProfile','-Command',"[Console]::Out.WriteLine('stdout fixture');[Console]::Error.WriteLine('stderr fixture');exit 0")}
    $result=Run-Case 'success' @($ok)
    Assert ($result.Code -eq 0 -and $result.Receipt.status -eq 'passed' -and $result.Receipt.sourceStableAndClean) 'Clean success failed'
    Assert (-not $result.Receipt.releaseQualified) 'Test success invented release qualification'
    Assert ($result.Receipt.artifacts[0].sha256 -ceq (Get-FileHash (Join-Path $repo 'fixture.txt') -Algorithm SHA256).Hash.ToLowerInvariant()) 'Artifact provenance hash differs'
    foreach($stream in @('stdout','stderr')) {
        $artifact=$result.Receipt.steps[0].$stream;$path=Join-Path $result.Directory $artifact.file
        Assert ((Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $artifact.sha256) 'Log hash mismatch'
    }
    $failure=@{id='native-failure';program='{pwsh}';arguments=@('-NoProfile','-Command','exit 7')}
    $result=Run-Case 'failure' @($failure,$ok)
    Assert ($result.Code -ne 0 -and $result.Receipt.steps[0].exitCode -eq 7 -and $result.Receipt.steps[0].status -eq 'failed') 'Native failure hidden'
    Assert ($result.Receipt.steps[1].status -eq 'not-run') 'Ran later step after failure'
    $result=Run-Case 'throw' @(@{id='script-throws';program='{pwsh}';arguments=@('-NoProfile','-File','{root}/throw.ps1')})
    Assert ($result.Code -ne 0 -and $result.Receipt.steps[0].exitCode -ne 0) 'Thrown script failure hidden'
    Assert ((Get-Content (Join-Path $result.Directory 'script-throws.stdout.log') -Raw) -match 'before failure') 'Lost pre-failure stdout'
    Assert ((Get-Content (Join-Path $result.Directory 'script-throws.stderr.log') -Raw) -match 'intentional script failure') 'Lost failure stderr'
    $result=Run-Case 'missing' @(@{id='missing-tool';program='{dotnet}';arguments=@('--version')}) @('-Dotnet',(Join-Path $fixtureRoot 'absent.exe'))
    Assert ($result.Code -ne 0 -and $result.Receipt.steps[0].status -eq 'failed') 'Missing tool hidden'
    $result=Run-Case 'empty' @()
    Assert ($result.Code -ne 0 -and $result.Receipt.status -eq 'failed') 'Empty plan passed'
    $result=Run-Case 'duplicate' @($ok,$ok)
    Assert ($result.Code -ne 0) 'Duplicate step passed'
    if($IsWindows) {
        $result=Run-Case 'batch-spaces' @(@{id='batch-spaces';program='{cmd}';arguments=@('/d','/c','{root}/failure child.cmd')})
        Assert ($result.Code -ne 0 -and $result.Receipt.steps[0].exitCode -eq 7) 'Batch path spaces or exit preservation failed'
    }
    $existing=Join-Path $fixtureRoot 'existing';New-Item -ItemType Directory $existing | Out-Null
    $owned=Join-Path $existing 'receipt.json';[IO.File]::WriteAllText($owned,'owner artifact')
    $text=@(& $hostPath -NoProfile -File $runner -RepositoryRoot $repo -PlanPath (Join-Path $fixtureRoot 'success.json') -OutputDirectory $existing 2>&1)
    Assert ($LASTEXITCODE -ne 0 -and [IO.File]::ReadAllText($owned) -ceq 'owner artifact') 'Existing run was overwritten'
    $result=Run-Case 'changed' @(@{id='mutates-source';program='{pwsh}';arguments=@('-NoProfile','-Command',"[IO.File]::WriteAllText((Join-Path (Get-Location) 'fixture.txt'),'changed')")})
    Assert ($result.Code -ne 0 -and -not $result.Receipt.sourceStableAndClean -and $result.Receipt.sourceBefore.clean -and -not $result.Receipt.sourceAfter.clean) 'Changed source qualified'
    [ordered]@{status='passed';assertions=$script:assertions;scope='synthetic local runner process/receipt boundaries only; no SDK/device/CI'} | ConvertTo-Json -Compress
} finally {
    $absolute=[IO.Path]::GetFullPath($fixtureRoot)
    $tempParent=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/')
    if((Split-Path -Parent $absolute) -ne $tempParent -or (Split-Path -Leaf $absolute) -notlike 'local-check-fixture-*'){throw 'Unsafe fixture cleanup path'}
    if(Test-Path -LiteralPath $absolute){Remove-Item -LiteralPath $absolute -Recurse -Force}
}
exit 0
