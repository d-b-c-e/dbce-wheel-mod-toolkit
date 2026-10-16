#Requires -Version 7.0
param([Parameter(Mandatory)][string]$RetainedLog)
# Pure text checks against the retained consumer record; no producer or process.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Stage-RunnerLifecycle.ps1')
$checks=0
function Check([bool]$Ok,[string]$Message){if(!$Ok){throw $Message};$script:checks++}
function Refuses([string]$Text){
    $failed=$false;try{Read-StageJsonLogReply $Text $marker $request $step|Out-Null}catch{$failed=$true}
    Check $failed 'Completed malformed reply was masked as pending'
}
$marker='Dev bindings: '
$text=[IO.File]::ReadAllText($RetainedLog)
$lines=@($text.Split([char]10)|Where-Object {$_.Contains($marker)})
if($lines.Count -ne 1){throw 'Expected the original single DRIVE02 reply'}
$line=$lines[0].TrimEnd([char]13)
$offset=$line.IndexOf($marker,[StringComparison]::Ordinal)+$marker.Length
$json=$line.Substring($offset)
$original=$json|ConvertFrom-Json -ErrorAction Stop
$request=$original.requestId;$step=$original.stepId
Check ($original.rows.Count -eq 27 -and $original.rows[24].Action -ceq 'gear6') 'Retained consumer regression changed'
# Original failure reported rows[24].NextStart/position7650. Reconstruct a
# transport snapshot ending there without changing or resending the command.
$partial=$line.Substring(0,$offset+7650)
$oldFailed=$false;try{$partial.Substring($offset)|ConvertFrom-Json -ErrorAction Stop|Out-Null}catch{$oldFailed=$true}
Check $oldFailed 'Old consumer did not reproduce the incomplete JSON failure'
Check ($null -eq (Read-StageJsonLogReply $partial $marker $request $step)) 'Incomplete first record was parsed'
Check ($null -eq (Read-StageJsonLogReply ('earlier complete line'+[char]10+$partial) $marker $request $step)) 'Incomplete final record after prior lines was parsed'
Check ($null -eq (Read-StageJsonLogReply $line $marker $request $step)) 'Complete JSON without publication LF was accepted early'
$reply=Read-StageJsonLogReply ($line+[char]10) $marker $request $step
Check ($reply.accepted -and $reply.requestId -ceq $request -and $reply.stepId -ceq $step -and $reply.rows.Count -eq 27) 'Completed exact reply was not returned'
$reply=Read-StageJsonLogReply ($line+"`r`n") $marker $request $step
Check ($reply.rows[24].NextStart -eq $null) 'CRLF reply changed its original value'
Check ($null -eq (Read-StageJsonLogReply ($line+[char]10) $marker $request ('f'*32))) 'Other step was accepted'
Check ($null -eq (Read-StageJsonLogReply ($line+[char]10) $marker ('f'*32) $step)) 'Other request was accepted'
$reply=Read-StageJsonLogReply ($line+[char]10+$marker+'{"unfinished":') $marker $request $step
Check ($reply.stepId -ceq $step) 'Unpublished later tail hid an already published exact reply'
Refuses ($partial+[char]10)
Refuses ($marker+'{"requestId":'+[char]10)
[pscustomobject]@{schema='dbce.stage-json-log-reply-check@1';status='PASS';checks=$checks;
 retainedLogSha256=(Get-FileHash -LiteralPath $RetainedLog).Hash;requestId=$request;stepId=$step;
 oldPartialParseRefused=$oldFailed;finalReplyRows=$original.rows.Count;commandsSent=0;producerInvoked=$false}|ConvertTo-Json
