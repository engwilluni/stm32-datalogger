# STM32 Datalogger

STM32F107VCTx datalogger com FreeRTOS, TinyUSB (CDC + MSC) e SD card via SPI.

## Sistema

| Item | Especificação |
|---|---|
| **MCU** | STM32F107VCTx, Cortex-M3 @ 72 MHz |
| **Flash** | 256 KB |
| **RAM** | 64 KB (+ 4 KB heap FreeRTOS Heap4) |
| **RTOS** | FreeRTOS 10.0.1 via CMSIS-RTOS V2 |
| **HAL** | STM32Cube FW_F1 V1.8.5 |
| **USB** | TinyUSB, CDC + MSC composite device |
| **SD Card** | SPI mode, FatFs |
| **Toolchain** | GNU Tools for STM32 (arm-none-eabi-gcc 11.3.rel1) |
| **IDE** | STM32CubeIDE 1.14.1 |

## Arquitetura

```mermaid
graph TB
    subgraph "STM32F107VCTx"
        subgraph "Tasks (FreeRTOS)"
            T_default[defaultTask<br/>512 B stack<br/>IWDG refresh]
            T_sampler[sampler<br/>512 B stack<br/>ADC + RTC]
            T_gpio[gpio_scan<br/>512 B stack<br/>Button scan]
            T_storage[storage<br/>1536 B stack<br/>FatFs write]
            T_usb[usb_device<br/>1024 B stack<br/>TinyUSB stack]
            T_proto[proto<br/>1024 B stack<br/>CDC command parser]
        end

        subgraph "Perifericos"
            ADC1[ADC1 CH14<br/>PC4 - Pot]
            RTC[RTC<br/>LSI calendar]
            GPIO_OUT[GPIO OUT<br/>PE8-PE15 LED bar]
            GPIO_IN[GPIO IN<br/>PD11-PD15 buttons<br/>PB7 user btn]
            SPI1[SPI1<br/>SD card]
            USB_OTG[USB OTG FS<br/>CDC + MSC]
            IWDG[IWDG<br/>4s timeout]
            TIM7[TIM7<br/>HAL timebase]
        end

        T_default --> IWDG
        T_sampler --> ADC1
        T_sampler --> RTC
        T_gpio --> GPIO_IN
        T_storage --> SPI1
        T_usb --> USB_OTG
        T_proto --> USB_OTG
    end

    subgraph "PC"
        CLI[CLI / GUI]
    end

    USB_OTG <-->|CDC ACM| CLI
    SPI1 <-->|SPI mode| SD_CARD[SD Card]
```

## Fluxo de dados

```mermaid
sequenceDiagram
    participant ADC as ADC1 (PC4)
    participant Sampler as task_sampler
    participant Queue as q_events
    participant Storage as task_storage
    participant SD as SD Card (SPI)
    participant USB as task_usb
    participant PC as PC App

    loop Every sample interval (ex: 500ms)
        ADC->>Sampler: HAL_ADC_Start + PollForConversion
        Sampler->>Sampler: Read RTC time
        Sampler->>Sampler: Build LogRecord
        Sampler->>Queue: osMessageQueuePut(record)
    end

    loop Storage loop
        Storage->>Queue: osMessageQueueGet(record)
        Storage->>Storage: Buffer records to 512 B sector
        Storage->>SD: sd_spi_write(sector, buf)
        SD-->>Storage: OK
    end

    Note over USB,PC: USB CDC commands
    PC->>USB: *IDN? / STAT? / TIME? / LS / GET
    USB->>USB: tud_msc_capacity_cb → sd_spi_get_sector_count
    USB-->>PC: OK response

    Note over USB,PC: USB MSC access
    PC->>USB: MSC READ/WRITE
    USB->>SD: sd_spi_read/write
    SD-->>USB: data
    USB-->>PC: data
```

## Pinagem

| Peripheral | Pin | Função |
|---|---|---|
| ADC1 CH14 | PC4 | Potenciômetro |
| RTC | — | Calendar (LSI) |
| GPIO OUT | PE8–PE15 | LED bar |
| GPIO IN | PD11–PD15 | BtnSelect, BtnUp, BtnRight, BtnDown, BtnLeft |
| GPIO IN | PB7 | BtnUser |
| SPI1 SCK | PA5 | SD card clock |
| SPI1 MISO | PA6 | SD card data out |
| SPI1 MOSI | PA7 | SD card data in |
| SPI1 CS | PB6 | SD card chip select |
| SWDIO | PA13 | Debug |
| SWCLK | PA14 | Debug |
| SWO | PB3 | Trace |
| USB DM | PA11 | USB D- |
| USB DP | PA12 | USB D+ |

## Build & Flash

```powershell
# Build
.\build.ps1 -Config Debug

# Flash (auto-detects J-Link or ST-Link)
.\flash.ps1 -Config Debug

# Debug
.\debug.ps1 -Config Debug
```

## App (PC Companion)

```bash
cd App
pip install -e .

# CLI
datalogger info
datalogger status
datalogger time --set
datalogger ls

# GUI
datalogger-gui
```

## Debug

LEDs diagnósticos no boot (PE8–PE15):

| LED | Significado |
|---|---|
| PE8 | CPU start (6x toggle) |
| PE9 | SystemClock_Config |
| PE10 | LSE setup |
| PE11 | Board init OK |
| PE12 | MX_GPIO_Init |
| PE13 | MX_ADC1_Init |
| PE14 | MX_RTC_Init |
| PE15 | MX_SPI1_Init |
