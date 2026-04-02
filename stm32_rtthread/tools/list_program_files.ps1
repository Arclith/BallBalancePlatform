$out = @()
foreach ($root in @('C:\Program Files','C:\Program Files (x86)')) {
    if (-Not (Test-Path $root)) { continue }
    Write-Host "==== $root ===="
    $folders = Get-ChildItem $root -Directory -ErrorAction SilentlyContinue
    $report = @()
    foreach ($f in $folders) {
        $size = 0
        try {
            $size = (Get-ChildItem $f.FullName -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum
        } catch { }
        $report += [PSCustomObject]@{Folder=$f.FullName; SizeBytes = $size; SizeMB = '{0:N2}' -f ($size/1MB)}
    }
    $report | Sort-Object -Property SizeBytes -Descending | Select-Object -First 10 | ForEach-Object { Write-Host ("{0,8} MB  -  {1}" -f $_.SizeMB, $_.Folder) }
    Write-Host ""
}

Write-Host "Done."