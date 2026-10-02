# Verify the process boundary used by GitHub's PowerShell runner, including a
# genuine assertion failure. All fixtures are offline and confined to temp.
$ErrorActionPreference='Stop'
$hostPath=Join-Path $PSHOME $(if(Test-Path (Join-Path $PSHOME 'pwsh.exe')){'pwsh.exe'}else{'powershell.exe'})
$root=Join-Path ([IO.Path]::GetTempPath()) ('consumer-exit-test-'+[guid]::NewGuid().ToString('N'))
$testPath=Join-Path $root 'tools/tests/Test-ConsumerStatus.ps1'
function Invoke-Wrapper {
    $quoted=$testPath.Replace("'","''")
    $command="`$ErrorActionPreference='Stop'; . '$quoted'; if (Test-Path -LiteralPath variable:LASTEXITCODE) { exit `$LASTEXITCODE }"
    $ErrorActionPreference='Continue'
    $output=@(& $hostPath -NoProfile -Command $command 2>&1)
    [pscustomobject]@{Code=$LASTEXITCODE;Text=($output -join "`n")}
}
try {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $testPath) | Out-Null
    Copy-Item (Join-Path $PSScriptRoot '../Get-ConsumerStatus.ps1') (Join-Path $root 'tools/Get-ConsumerStatus.ps1')
    $original=Get-Content (Join-Path $PSScriptRoot 'Test-ConsumerStatus.ps1') -Raw
    # Exercise the same fixture without requesting any execution-policy override.
    $fixture=$original.Replace("'-ExecutionPolicy', 'Bypass', ",'')
    $beforeFix=[regex]::Replace($fixture,'\r?\nexit 0\s*\z','')
    if($beforeFix -ceq $fixture){throw 'Terminal success exit not found'}
    [IO.File]::WriteAllText($testPath,$beforeFix,[Text.UTF8Encoding]::new($false))
    $stale=Invoke-Wrapper
    if($stale.Code -ne 1 -or $stale.Text -notmatch '"status":"passed","assertions":76') { throw "Stale native exit did not reproduce: $($stale.Code) $($stale.Text)" }
    [IO.File]::WriteAllText($testPath,$fixture,[Text.UTF8Encoding]::new($false))
    $success=Invoke-Wrapper
    if($success.Code -ne 0 -or $success.Text -notmatch '"status":"passed","assertions":76') { throw "Success wrapper failed: $($success.Code) $($success.Text)" }
    $assertion="Assert (`$result.Code -eq 1 -and -not `$result.Data.TargetAvailable) 'Missing comparison target should require review'"
    if(-not $fixture.Contains($assertion)){throw 'Failure-injection assertion not found'}
    $failure=$fixture.Replace($assertion,"Assert `$false 'Intentional exit-contract assertion failure'")
    [IO.File]::WriteAllText($testPath,$failure,[Text.UTF8Encoding]::new($false))
    $failed=Invoke-Wrapper
    if($failed.Code -eq 0 -or $failed.Text -notmatch 'Intentional exit-contract assertion failure' -or $failed.Text -match '"status":"passed"') { throw "Assertion failure was hidden: $($failed.Code) $($failed.Text)" }
    [ordered]@{status='passed';beforeFixExit=$stale.Code;successExit=$success.Code;assertionFailureExit=$failed.Code;fixtureAssertions=76} | ConvertTo-Json -Compress
} finally {
    $absolute=[IO.Path]::GetFullPath($root)
    $tempParent=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/')
    if((Split-Path -Parent $absolute) -ne $tempParent -or (Split-Path -Leaf $absolute) -notlike 'consumer-exit-test-*'){throw 'Unsafe fixture cleanup path'}
    if(Test-Path -LiteralPath $absolute){Remove-Item -LiteralPath $absolute -Recurse -Force}
}
exit 0
