# flash.ps1 - grava o firmware (gerado por /stm32_dev)
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

if ($Probe -eq "auto") {
    $p = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
         Where-Object { $_.FriendlyName -match 'ST-?Link|J-?Link|SEGGER' } | Select-Object -First 1
    if     ($p.FriendlyName -match 'ST-?Link')        { $Probe = "stlink" }
    elseif ($p.FriendlyName -match 'J-?Link|SEGGER')  { $Probe = "jlink"  }
    else { throw "Nenhum debugger ST-Link/J-Link detectado. Conecte o probe ou use -Probe stlink|jlink." }
    Write-Host "[flash] probe detectado: $($p.FriendlyName) -> $Probe" -ForegroundColor Cyan
}

if ($Probe -eq "stlink") {
    $candidates = @(
        "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
        "C:\ST\STM32CubeIDE_1.14.1\STM32CubeIDE\plugins\*cubeprogrammer*\tools\bin\STM32_Programmer_CLI.exe"
    )
    $cli = $null
    foreach ($c in $candidates) {
        $cli = Get-ChildItem $c -EA SilentlyContinue | Select-Object -First 1
        if ($cli) { break }
    }
    if (-not $cli) { throw "STM32_Programmer_CLI.exe nao encontrado. Instale o STM32CubeProgrammer." }
    Write-Host "[flash] ST-Link -> $Elf" -ForegroundColor Cyan
    & $cli.FullName -c port=SWD -w "$Elf" -v -rst
    if ($LASTEXITCODE -eq 0) { Write-Host "[flash] OK" -ForegroundColor Green }
    exit $LASTEXITCODE
}
else {
    $jlink = Get-ChildItem "C:\Program Files*\SEGGER\JLink*\JLink.exe" -EA SilentlyContinue |
             Sort-Object FullName -Descending | Select-Object -First 1
    if (-not $jlink) { throw "JLink.exe nao encontrado. Instale o J-Link Software Pack." }

    # J-Link Commander (especially older V8 probes) handles Intel HEX better than ELF
    $Hex = $Elf -replace '\.elf$','.hex'
    $gcc = Get-ChildItem "C:\ST\STM32CubeIDE_1.14.1\STM32CubeIDE\plugins\*gnu-tools*\tools\bin" -Directory -EA SilentlyContinue | Select-Object -First 1
    $objcopy = Join-Path $gcc.FullName "arm-none-eabi-objcopy.exe"
    if (-not (Test-Path $objcopy)) { throw "arm-none-eabi-objcopy.exe nao encontrado." }
    & $objcopy -O ihex $Elf $Hex | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Falha ao converter ELF para HEX." }

    $script = Join-Path $env:TEMP "flash_$([guid]::NewGuid().ToString('N')).jlink"
    @(
        "si SWD",
        "speed 4000",
        "device $JLinkDevice",
        "connect",
        "loadfile `"$Hex`"",
        "r",
        "g",
        "exit"
    ) | Set-Content -Encoding ascii $script
    Write-Host "[flash] J-Link -> $Hex (device=$JLinkDevice)" -ForegroundColor Cyan
    try {
        & $jlink.FullName -device $JLinkDevice -if SWD -speed 4000 -autoconnect 1 -ExitOnError 1 -CommandFile $script
        if ($LASTEXITCODE -eq 0) { Write-Host "[flash] OK" -ForegroundColor Green }
    }
    finally { Remove-Item $script -EA SilentlyContinue }
    exit $LASTEXITCODE
}
