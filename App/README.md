# STM32 Datalogger — PC Companion App

Python companion application for the STM32F107 USB/SD datalogger.
Provides a desktop GUI and a command-line interface (CLI) over the USB-CDC serial port.

## Requirements

- Python 3.9+
- `pyserial` (installed automatically below)
- `tkinter` — bundled with the standard CPython installer on Windows; on Linux install `python3-tk`

## Install

```
cd App
pip install -e .
```

This installs two entry-point commands: `datalogger` (CLI) and `datalogger-gui` (GUI).

---

## GUI

### Launch (three equivalent ways)

```
# Preferred — works from any directory after pip install -e .
datalogger-gui

# Module form — run from the App/ directory
python -m datalogger.gui

# Direct script — run from the App/ directory or App/datalogger/
python datalogger/gui.py
```

### Tabs

| Tab | Purpose |
|---|---|
| **Device** | Identity, status, RTC clock, Sync PC Time |
| **Logging** | Start/Stop, sample rate, Stream ON/OFF, MSC ON/OFF |
| **Files** | List, download, delete files on the SD card |
| **Config** | Read and write CONFIG.TXT key=value pairs |
| **Console** | Raw command log |

### Typical workflow

1. Plug the board via USB-CDC, wait for the COM port to appear.
2. Click **Scan**, select the port, click **Connect**.
3. *(Device tab)* Click **Sync PC Time** to set the RTC.
4. *(Logging tab)* Set **Rate** (ms) and click **Start**.
5. *(Files tab)* Click **Refresh** to list log files; select one and click **Download**.
6. *(Files tab)* Enter the downloaded `.bin` path in **Export BIN → CSV** and click the button.

### MSC mode (direct SD card access)

Click **MSC ON** in the Logging tab. The firmware closes the log file, unmounts FatFs, and
exposes the SD card as a standard USB mass-storage drive. Windows will detect the drive
within ~5 seconds (one UNIT_ATTENTION poll cycle).

Click **MSC OFF** to resume logging. Download via CDC is not available while MSC is active.

---

## CLI

```
datalogger [-p PORT] <command> [args]
```

If `-p PORT` is omitted the device is auto-detected by VID/PID `1209:0001`.

### Commands

| Command | Description |
|---|---|
| `info` | Print `*IDN?` (model, firmware, serial) |
| `status` | Print `STAT?` (logging, card, USB, free heap) |
| `time` | Read RTC epoch |
| `time --set` | Set RTC to current PC time |
| `start` | Start logging |
| `stop` | Stop logging |
| `rate <ms>` | Set ADC sample interval (100–60000 ms) |
| `thr <ch> <lo> <hi>` | Set ADC alarm threshold (ch=14, 0–4095) |
| `cfg` | Read CONFIG.TXT |
| `cfg <key=value>` | Write one config key (`format`, `rate`, `oversample`) |
| `ls` | List files on SD card |
| `get <name> -o <out>` | Download a file (framed binary protocol) |
| `del <name>` | Delete a file |
| `format` | Format SD card as FAT32 (slow — prefer PC-side format) |
| `stream on\|off` | Enable/disable live CSV stream over CDC |
| `msc on\|off` | Enable/disable USB mass-storage mode |
| `export <file.bin> <out.csv>` | Convert a downloaded binary log to CSV (offline) |

### Examples

```
# Auto-detect port and sync time
datalogger time --set

# Check status on a specific port
datalogger -p COM4 status

# Download the first log and export
datalogger get LOGS/L00001.BIN -o log.bin
datalogger export log.bin log.csv

# Enable MSC, copy files from the drive, then resume logging
datalogger msc on
# ... copy files in Explorer ...
datalogger msc off

# Set 500 ms sample rate and CSV format
datalogger rate 500
datalogger cfg format=csv
```

---

## CONFIG.TXT keys

| Key | Values | Default |
|---|---|---|
| `format` | `bin` / `csv` | `bin` |
| `rate` | 100–60000 (ms) | 1000 |
| `oversample` | 1–16 | 1 |

Thresholds (`thr`) are session-only and not persisted to CONFIG.TXT.

---

## SD card requirements

- **MBR partition table + FAT32** — FatFs R0.11 does not support GPT or exFAT.
- Format with SD Card Formatter, Rufus (FAT32 + MBR), or Windows `diskpart`:
  `clean` → `convert mbr` → `create partition primary` → `format fs=fat32 quick`.
