# build.ps1 - compila o projeto STM32 (gerado por /stm32_dev)
[CmdletBinding()]
param([ValidateSet("Debug","Release")][string]$Config = "Debug")
$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot
$ProjectName = "stm32_datalogger"
$Workspace   = "C:\Users\Usuario\STM32CubeIDE\workspace_1.14.1"

function Find-IdeRoot {
    $idec = Get-ChildItem "C:\ST\STM32CubeIDE_*\STM32CubeIDE\stm32cubeidec.exe" -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending | Select-Object -First 1
    # Prefer the version matching the workspace name
    $match = Get-ChildItem "C:\ST\STM32CubeIDE_1.14.1\STM32CubeIDE\stm32cubeidec.exe" -ErrorAction SilentlyContinue |
             Select-Object -First 1
    $chosen = if ($match) { $match } else { $idec }
    if ($chosen) { return @{ Exe = $chosen.FullName; Plugins = (Join-Path $chosen.Directory.FullName "plugins") } }
    return $null
}

$ide = Find-IdeRoot
$buildDir = Join-Path $ProjectRoot $Config
$makefile  = Join-Path $buildDir "makefile"

if ((Test-Path $makefile) -and $ide) {
    $mk  = Get-ChildItem "$($ide.Plugins)\*make*\tools\bin\make.exe" -EA SilentlyContinue | Select-Object -First 1
    $gcc = Get-ChildItem "$($ide.Plugins)\*gnu-tools*\tools\bin"     -Directory -EA SilentlyContinue | Select-Object -First 1
    if ($mk -and $gcc) {
        $env:PATH = "$($gcc.FullName);$env:PATH"
        $jobs = (Get-CimInstance Win32_Processor).NumberOfLogicalProcessors
        Write-Host "[build] make -j$jobs em $buildDir (Config=$Config)" -ForegroundColor Cyan
        & $mk.FullName -C $buildDir -j $jobs -k all
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[build] OK -> $buildDir\$ProjectName.elf" -ForegroundColor Green
        }
        exit $LASTEXITCODE
    }
}

if (-not $ide) { throw "STM32CubeIDE nao encontrada em C:\ST\STM32CubeIDE_*. Ajuste o caminho." }

# Headless build (requer a GUI fechada para este workspace)
Write-Host "[build] headless build $ProjectName/$Config" -ForegroundColor Cyan
Write-Host "        Atencao: feche a IDE se o workspace estiver aberto!" -ForegroundColor Yellow
& $ide.Exe -nosplash `
    -application org.eclipse.cdt.managedbuilder.core.headlessbuild `
    -data  $Workspace `
    -import $ProjectRoot `
    -cleanBuild "$ProjectName/$Config"
exit $LASTEXITCODE
