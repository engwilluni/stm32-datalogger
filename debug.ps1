# debug.ps1 - inicia sessao de debug (gerado por /stm32_dev)
[CmdletBinding()]
param(
    [ValidateSet("Debug","Release")][string]$Config = "Debug",
    [ValidateSet("auto","stlink","jlink")][string]$Probe = "auto"
)
$ErrorActionPreference = "Stop"

$ProjectRoot  = $PSScriptRoot
$Elf          = Join-Path $ProjectRoot "$Config\stm32_datalogger.elf"
$JLinkDevice  = "STM32F107VC"

if (-not (Test-Path $Elf)) { throw "ELF nao encontrado: $Elf`nRode .\build.ps1 primeiro." }

# Locate IDE plugins (prefer 1.14.1 to match workspace)
$idePlugins = $null
foreach ($v in @("1.14.1","1.19.0","1.10.1")) {
    $p = "C:\ST\STM32CubeIDE_$v\STM32CubeIDE\plugins"
    if (Test-Path $p) { $idePlugins = $p; break }
}

$gdb = $null
if ($idePlugins) {
    $gdb = Get-ChildItem "$idePlugins\*gnu-tools*\tools\bin\arm-none-eabi-gdb.exe" -EA SilentlyContinue | Select-Object -First 1
}
if (-not $gdb) {
    $gdb = Get-Command arm-none-eabi-gdb -EA SilentlyContinue
}
if (-not $gdb) { throw "arm-none-eabi-gdb nao encontrado. Adicione o toolchain ao PATH ou verifique a instalacao da IDE." }

# Detect probe
if ($Probe -eq "auto") {
    $p = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
         Where-Object { $_.FriendlyName -match 'ST-?Link|J-?Link|SEGGER' } | Select-Object -First 1
    if     ($p.FriendlyName -match 'ST-?Link')        { $Probe = "stlink" }
    elseif ($p.FriendlyName -match 'J-?Link|SEGGER')  { $Probe = "jlink"  }
    else { throw "Nenhum debugger ST-Link/J-Link detectado. Conecte o probe ou use -Probe stlink|jlink." }
    Write-Host "[debug] probe detectado: $($p.FriendlyName) -> $Probe" -ForegroundColor Cyan
}

$server = $null
$port   = 0

if ($Probe -eq "stlink") {
    $srv = $null
    if ($idePlugins) {
        $srv = Get-ChildItem "$idePlugins\*stlink-gdb-server*\tools\bin\ST-LINK_gdbserver.exe" -EA SilentlyContinue | Select-Object -First 1
    }
    if (-not $srv) { throw "ST-LINK_gdbserver.exe nao encontrado nos plugins da IDE." }
    $cp = $null
    if ($idePlugins) {
        $cp = (Get-ChildItem "$idePlugins\*cubeprogrammer*\tools\bin" -Directory -EA SilentlyContinue | Select-Object -First 1).FullName
    }
    if (-not $cp) { throw "Pasta bin do STM32CubeProgrammer nao encontrada nos plugins da IDE." }
    $port = 61234
    Write-Host "[debug] iniciando ST-LINK GDB server na porta $port..." -ForegroundColor Cyan
    $server = Start-Process $srv.FullName `
        -ArgumentList @("-p", $port, "-cp", $cp, "-e", "-r", "1", "-d", "--swd") `
        -PassThru
}
else {
    $srv = Get-ChildItem "C:\Program Files*\SEGGER\JLink*\JLinkGDBServerCL.exe" -EA SilentlyContinue |
           Sort-Object FullName -Descending | Select-Object -First 1
    if (-not $srv) { throw "JLinkGDBServerCL.exe nao encontrado. Instale o J-Link Software Pack." }
    $port = 2331
    Write-Host "[debug] iniciando J-Link GDB server (device=$JLinkDevice, porta=$port)..." -ForegroundColor Cyan
    $server = Start-Process $srv.FullName `
        -ArgumentList @("-device", $JLinkDevice, "-if", "SWD", "-speed", "4000", "-port", $port) `
        -PassThru
}

try {
    Start-Sleep -Seconds 2
    Write-Host "[debug] conectando GDB a localhost:$port ..." -ForegroundColor Cyan
    $gdbExe = if ($gdb -is [System.IO.FileSystemInfo]) { $gdb.FullName } else { $gdb.Source }
    & $gdbExe $Elf `
        -ex "set confirm off" `
        -ex "target extended-remote localhost:$port" `
        -ex "load" `
        -ex "break main" `
        -ex "monitor reset" `
        -ex "continue"
}
finally {
    if ($server -and -not $server.HasExited) {
        Write-Host "[debug] encerrando GDB server (PID $($server.Id))..." -ForegroundColor Yellow
        Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
    }
}
