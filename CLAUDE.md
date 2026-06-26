# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

STM32F107VCTx (Cortex-M3, 72 MHz) datalogger running FreeRTOS v10.0.1 via CMSIS-RTOS V2. HAL library: STM32Cube FW_F1 V1.8.5. Toolchain: GNU Tools for STM32 (arm-none-eabi-gcc 11.3.rel1). IDE: STM32CubeIDE 1.14.1.

Memory: 256 KB FLASH, 64 KB RAM. Linker reserves 512 B heap (`_Min_Heap_Size`) and 1 KB stack (`_Min_Stack_Size`). FreeRTOS heap pool: 4096 bytes (Heap 4 allocator) — very limited, be conservative with task stack sizes.

## Build

Build is managed by STM32CubeIDE (Eclipse CDT). The `Debug/` directory contains an auto-generated makefile that drives `arm-none-eabi-gcc`. To build from the command line (requires arm-none-eabi toolchain on PATH):

```powershell
cd Debug
make -j4
```

Output artifacts: `Debug/stm32_datalogger.elf`, `stm32_datalogger.map`, `stm32_datalogger.list`.

Use `.\build.ps1` (see STM32 Dev section below). SWD pins: PA13 (SWDIO), PA14 (SWCLK), PB3 (SWO trace).

## Architecture

### Peripheral Configuration (CubeMX-generated, do not hand-edit init functions)

| Peripheral | Pin | Role |
|---|---|---|
| ADC1 CH14 | PC4 | Potentiometer analog input, single sw-triggered conversion, 12 MHz clock |
| RTC | — | Calendar (LSI clock, 1-second prescaler), initialized to 2026-06-25 12:00:00 |
| GPIO OUT | PE8–PE15 | 8 output pins (LED bar or similar) |
| GPIO IN | PD11–PD15 | BtnSelect, BtnUp, BtnRight, BtnDown, BtnLeft |
| GPIO IN | PB7 | BtnUser |
| TIM7 | — | HAL timebase (replaces SysTick to avoid conflict with FreeRTOS tick) |

Clock tree: 25 MHz HSE → PLL2 ×8 /5 → 40 MHz → /5 → PLL ×9 = **72 MHz SYSCLK**. APB1 = 36 MHz, APB2 = 72 MHz, ADC = 12 MHz.

### RTOS Structure

- `main.c` initializes peripherals, creates `defaultTask`, then calls `osKernelStart()` — code after that never runs.
- `Core/Src/freertos.c` is where additional RTOS objects (tasks, queues, semaphores, timers) should be created. Application task implementations also go here, or in dedicated files under `Core/Src/`.
- `defaultTask` (`StartDefaultTask` in `main.c`) is currently a stub with `osDelay(1)`. Application logic replaces this.

FreeRTOS interrupt priority rules:
- ISRs that call FreeRTOS `FromISR` APIs must have NVIC priority **5–15** (`configMAX_SYSCALL_INTERRUPT_PRIORITY = 5`).
- Never call non-`FromISR` FreeRTOS functions from an ISR.

### CubeMX Code Generation Rules

`stm32_datalogger.ioc` is the CubeMX project file. Regenerating from CubeMX overwrites everything **outside** `/* USER CODE BEGIN ... */` / `/* USER CODE END ... */` guard blocks. All application code must live inside these guards. The generated peripheral init functions (`MX_ADC1_Init`, `MX_RTC_Init`, `MX_GPIO_Init`, `SystemClock_Config`) must not be modified by hand — change the `.ioc` and regenerate instead.

New source files added manually under `Core/Src/` must be manually added to the STM32CubeIDE project (right-click → Refresh, or edit `.cproject`) since the makefile is auto-generated.

# STM32 Dev

_Gerado por `/stm32_dev` em 2026-06-25._

- **IDE:** STM32CubeIDE 1.14.1 (`C:\ST\STM32CubeIDE_1.14.1\STM32CubeIDE\stm32cubeidec.exe`)
- **MCU:** STM32F107VCTx — família STM32F1, núcleo Cortex-M3
- **HAL:** v1.1.9 (macros em `Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c`; firmware package FW_F1 V1.8.5)
- **Device J-Link:** `STM32F107VC`
- **Workspace:** `C:\Users\Usuario\STM32CubeIDE\workspace_1.14.1`
- **Configs:** Debug, Release  (saída em `<Config>/stm32_datalogger.elf`)

## Scripts (PowerShell)

| Script | Função | Uso |
|--------|--------|-----|
| `build.ps1` | Compila via make + toolchain embutido (quick-path, Debug/makefile existe) | `.\build.ps1 [-Config Debug\|Release]` |
| `flash.ps1` | Detecta o probe e grava o firmware | `.\flash.ps1 [-Config Debug\|Release] [-Probe auto\|stlink\|jlink]` |
| `debug.ps1` | Sobe o gdbserver + arm-none-eabi-gdb | `.\debug.ps1 [-Config Debug\|Release] [-Probe auto\|stlink\|jlink]` |

## Debugger

- Detecção automática via `Get-PnpDevice` (ST-Link → ST-LINK_gdbserver + STM32CubeProgrammer CLI; J-Link → JLinkGDBServerCL).
- Último probe detectado em 2026-06-25: **J-Link** (instalado em `C:\Program Files\SEGGER\JLink\`).
- ST-LINK gdbserver: `C:\ST\STM32CubeIDE_1.14.1\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.stlink-gdb-server.win32_2.1.100.202310302101\tools\bin\ST-LINK_gdbserver.exe`
- arm-none-eabi-gdb: dentro de `plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.11.3.rel1.win32_*\tools\bin\`

## Observações

- Headless build exige a GUI da IDE fechada para este workspace.
- Com múltiplos probes conectados, informe o serial (`sn=` para ST-Link; `-SelectEmuBySN` para J-Link).
