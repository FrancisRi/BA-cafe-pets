param([int[]]$RootProcessIds,[int]$Seconds=6,[string]$Output='')
$ErrorActionPreference='Stop'
$inventory=@(Get-CimInstance Win32_Process | Select-Object ProcessId,ParentProcessId,Name)
$groups=@()
foreach($rootId in $RootProcessIds){
    $ids=[System.Collections.Generic.HashSet[int]]::new();[void]$ids.Add($rootId)
    do{$changed=$false;foreach($item in $inventory){if($ids.Contains([int]$item.ParentProcessId)-and $ids.Add([int]$item.ProcessId)){$changed=$true}}}while($changed)
    $processes=@(Get-Process -Id @($ids) -ErrorAction SilentlyContinue)
    if(!$processes.Count){throw "Process $rootId not running"}
    $cpu=($processes | Measure-Object CPU -Sum).Sum
    $groups+=@{rootPid=$rootId;name=(Get-Process -Id $rootId).ProcessName;ids=@($ids);cpuStart=$cpu}
}
$watch=[System.Diagnostics.Stopwatch]::StartNew()
Start-Sleep -Seconds $Seconds
foreach($group in $groups){
    $processes=@(Get-Process -Id $group.ids -ErrorAction SilentlyContinue)
    $cpu=($processes | Measure-Object CPU -Sum).Sum
    $group.cpuPercentMachine=[Math]::Round(($cpu-$group.cpuStart)/$watch.Elapsed.TotalSeconds/[Environment]::ProcessorCount*100,2)
    $group.cpuPercentOneCore=[Math]::Round(($cpu-$group.cpuStart)/$watch.Elapsed.TotalSeconds*100,1)
    $group.privateCommitMiB=[Math]::Round(($processes | Measure-Object PrivateMemorySize64 -Sum).Sum/1MB,1)
    $group.workingSetSumMiB=[Math]::Round(($processes | Measure-Object WorkingSet64 -Sum).Sum/1MB,1)
    $group.processCount=$processes.Count
    $group.Remove('cpuStart')
}
$cpuSampleSeconds=$watch.Elapsed.TotalSeconds
try{
    $samples=(Get-Counter '\GPU Engine(*)\Utilization Percentage' -ErrorAction Stop).CounterSamples
    foreach($group in $groups){
        $values=@($samples | Where-Object {$_.InstanceName -match 'pid_(\d+)_.*engtype_3D' -and $group.ids -contains [int]$Matches[1]} | ForEach-Object CookedValue)
        $group.max3DEnginePercent=if($values.Count){[Math]::Round(($values | Measure-Object -Maximum).Maximum,2)}else{$null}
    }
}catch{foreach($group in $groups){$group.gpuCounters='unavailable: '+$_.Exception.Message}}
$result=@{cpuSampleSeconds=[Math]::Round($cpuSampleSeconds,2);totalSeconds=[Math]::Round($watch.Elapsed.TotalSeconds,2);logicalProcessors=[Environment]::ProcessorCount;groups=$groups}
$json=$result | ConvertTo-Json -Depth 5
if($Output){$json | Set-Content -LiteralPath $Output -Encoding utf8}
$json
