param(
    [string]$elf = "Debug\rtthread.elf"
)

if (-not (Test-Path $elf)) {
    Write-Host "ELF file not found: $elf" -ForegroundColor Yellow
    exit 0
}

$sizeOutput = & arm-none-eabi-size --format=berkeley $elf 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host $sizeOutput
    exit 0
}

Write-Host ""
Write-Host "arm-none-eabi-size `"$elf`""
Write-Host $sizeOutput.Trim()
Write-Host ""

$lines = $sizeOutput -split "\r?\n"
if ($lines.Count -lt 2) {
    Write-Host $sizeOutput
    exit 0
}

$cols = -split $lines[1]
if ($cols.Count -lt 3) {
    Write-Host $lines[1]
    exit 0
}

[int]$text = $cols[0]
[int]$data = $cols[1]
[int]$bss  = $cols[2]

$flash = $text + $data
$ram = $data + $bss

$flash_total = 128 * 1024
$ram_total = 20 * 1024

$flash_percent = ($flash / $flash_total) * 100.0
$ram_percent = ($ram / $ram_total) * 100.0

Write-Host "----------------------------------------------------------------"
Write-Host ("Flash: {0} bytes ({1:N2} KB) - {2:N2}%" -f $flash, ($flash/1024.0), $flash_percent)
Write-Host ("RAM:   {0} bytes ({1:N2} KB) - {2:N2}%" -f $ram, ($ram/1024.0), $ram_percent)
Write-Host "----------------------------------------------------------------"

exit 0
