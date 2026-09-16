$log = "e:\work\SVN_2022\Unity_PrefabForMaya\Unity_PrefabForMaya\memwatch.log"
$threshold = 90
$lastAlert = Get-Date "2000-01-01"
while ($true) {
  Start-Sleep -Seconds 20
  try {
    $os = Get-CimInstance Win32_OperatingSystem
    $total = [double]$os.TotalVisibleMemorySize
    $free = [double]$os.FreePhysicalMemory
    $usedPct = [math]::Round((($total - $free) / $total) * 100, 1)
    $py = @(Get-Process python*,py* -ErrorAction SilentlyContinue)
    $pyHot = $py | Where-Object { $_.WorkingSet64 -gt 500MB }
    $cursorHot = @(Get-Process Cursor -ErrorAction SilentlyContinue | Where-Object { $_.WorkingSet64 -gt 2GB })
    $shouldAlert = ($usedPct -ge $threshold) -or ($pyHot.Count -gt 0)
    if ($shouldAlert -and ((Get-Date) - $lastAlert).TotalSeconds -gt 60) {
      $lastAlert = Get-Date
      $stamp = (Get-Date).ToString('s')
      Add-Content $log ("ALERT {0} mem={1}% free={2}MB" -f $stamp, $usedPct, [math]::Round($free/1024,0))
      Get-Process -ErrorAction SilentlyContinue | Sort-Object WorkingSet64 -Descending | Select-Object -First 15 | ForEach-Object {
        Add-Content $log ("  proc {0} pid={1} mem={2}MB cpu={3}" -f $_.Name, $_.Id, [math]::Round($_.WorkingSet64/1MB,0), [math]::Round($_.CPU,1))
      }
      if ($py.Count -gt 0) {
        $py | ForEach-Object { Add-Content $log ("  python {0} pid={1} mem={2}MB" -f $_.Name, $_.Id, [math]::Round($_.WorkingSet64/1MB,0)) }
      } else {
        Add-Content $log "  python: none"
      }
      Write-Output ('AGENT_LOOP_WAKE_memwatch {"prompt":"内存告警：读取 e:\\work\\SVN_2022\\Unity_PrefabForMaya\\Unity_PrefabForMaya\\memwatch.log 末尾，总结是谁占满内存，并告诉用户。","usedPct":' + $usedPct + '}')
    }
  } catch {
    Add-Content $log ("ERROR $(Get-Date -Format s) $($_.Exception.Message)")
  }
}
