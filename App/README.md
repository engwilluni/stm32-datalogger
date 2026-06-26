# STM32 Datalogger — PC Companion App

Companion Python application for the STM32F107 USB/SD datalogger.

## Install

```bash
cd App
pip install -e .
```

## Usage

Auto-detect the datalogger by VID/PID (`1209:0001`):

```bash
datalogger info
datalogger status
datalogger time --set
datalogger start
datalogger ls
datalogger get LOGS/L00001.BIN --out log.bin
datalogger export log.bin log.csv
```

Specify a serial port if auto-detection fails:

```bash
datalogger -p COM3 info
```
