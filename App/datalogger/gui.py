"""Desktop GUI for the STM32 datalogger."""

# Bootstrap for running directly: `python gui.py` or `python datalogger/gui.py`.
# Must come before the relative imports so Python can resolve them.
if __name__ == "__main__" and not __package__:
    import sys as _sys, os as _os
    _sys.path.insert(0, _os.path.dirname(_os.path.dirname(_os.path.abspath(__file__))))
    __package__ = "datalogger"

import tkinter as tk
from tkinter import ttk, messagebox
import threading
from typing import Optional

import serial.tools.list_ports

from .transport import Transport, TransportError, VID_PID
from .protocol import encode, decode, expect_ok
from .download import receive
from .timesync import pc_epoch
from .records import read_binary_file, export_csv

import io
import os


class _CmdThread(threading.Thread):
    def __init__(self, target, on_done=None):
        super().__init__(daemon=True)
        self._target_fn = target
        self._on_done = on_done
        self._result = None
        self._error = None

    def run(self):
        try:
            self._result = self._target_fn()
        except Exception as e:
            self._error = e
        finally:
            if self._on_done:
                self._on_done(self._result, self._error)


def _run_async(fn, on_done=None):
    _CmdThread(target=fn, on_done=on_done).start()


class DataloggerGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("STM32 Datalogger")
        self.root.geometry("720x680")
        self.root.minsize(600, 500)
        self.root.option_add("*tearOff", False)

        self.ser: Optional[Transport] = None
        self.connected = False
        self.port_var = tk.StringVar()

        style = ttk.Style()
        style.theme_use("clam")
        style.configure("Heading.TLabel", font=("Segoe UI", 10, "bold"))
        style.configure("Status.TLabel", font=("Consolas", 9))
        style.configure("Log.TLabel", font=("Consolas", 9))

        self._build_ui()
        self._scan_ports()

    # ─────────────── UI construction ───────────────

    def _build_ui(self):
        self._build_connection()
        self._build_notebook()

    def _build_connection(self):
        f = ttk.Frame(self.root, padding=6)
        f.pack(fill=tk.X)
        ttk.Label(f, text="Port:").pack(side=tk.LEFT, padx=(0, 4))
        self.port_cb = ttk.Combobox(f, textvariable=self.port_var, width=18, state="readonly")
        self.port_cb.pack(side=tk.LEFT, padx=(0, 4))
        ttk.Button(f, text="Scan", command=self._scan_ports, width=6).pack(side=tk.LEFT, padx=(0, 4))
        self.connect_btn = ttk.Button(f, text="Connect", command=self._toggle_connect, width=10)
        self.connect_btn.pack(side=tk.LEFT, padx=(0, 4))
        self.conn_indicator = tk.Canvas(f, width=16, height=16, highlightthickness=0)
        self.conn_indicator.pack(side=tk.LEFT)
        self._dot_off()

    def _build_notebook(self):
        nb = ttk.Notebook(self.root, padding=(6, 0, 6, 6))
        nb.pack(fill=tk.BOTH, expand=True)

        self._build_device_tab(nb)
        self._build_control_tab(nb)
        self._build_files_tab(nb)
        self._build_config_tab(nb)
        self._build_console_tab(nb)

    def _build_device_tab(self, nb):
        f = ttk.Frame(nb, padding=10)
        nb.add(f, text="Device")

        # Identity
        ttk.Label(f, text="Device", style="Heading.TLabel").pack(anchor=tk.W)
        self.idn_lbl = ttk.Label(f, text="(not connected)", style="Status.TLabel")
        self.idn_lbl.pack(fill=tk.X, pady=(2, 6))

        # Status
        ttk.Label(f, text="Status", style="Heading.TLabel").pack(anchor=tk.W)
        self.status_lbl = ttk.Label(f, text="", style="Status.TLabel")
        self.status_lbl.pack(fill=tk.X, pady=(2, 6))

        # Time
        ttf = ttk.Frame(f)
        ttf.pack(fill=tk.X, pady=(4, 0))
        ttk.Label(ttf, text="RTC Time:", style="Heading.TLabel").pack(side=tk.LEFT, padx=(0, 8))
        self.time_lbl = ttk.Label(ttf, text="--", style="Status.TLabel")
        self.time_lbl.pack(side=tk.LEFT, padx=(0, 8))
        ttk.Button(ttf, text="Sync PC Time", command=self._cmd_sync_time).pack(side=tk.LEFT)

        # Buttons
        bf = ttk.Frame(f)
        bf.pack(fill=tk.X, pady=(12, 0))
        ttk.Button(bf, text="Refresh Info", command=self._cmd_refresh_info).pack(side=tk.LEFT, padx=(0, 6))

    def _build_control_tab(self, nb):
        f = ttk.Frame(nb, padding=10)
        nb.add(f, text="Logging")

        # Start / Stop
        cf = ttk.Frame(f)
        cf.pack(fill=tk.X)
        ttk.Button(cf, text="Start", command=lambda: self._cmd("LOG", "START")).pack(side=tk.LEFT, padx=(0, 4))
        ttk.Button(cf, text="Stop", command=lambda: self._cmd("LOG", "STOP")).pack(side=tk.LEFT, padx=(0, 12))
        ttk.Label(cf, text="Rate:").pack(side=tk.LEFT, padx=(0, 4))
        self.rate_var = tk.StringVar(value="500")
        self.rate_spin = ttk.Spinbox(cf, from_=50, to=60000, increment=50,
                                     textvariable=self.rate_var, width=8)
        self.rate_spin.pack(side=tk.LEFT, padx=(0, 4))
        ttk.Label(cf, text="ms").pack(side=tk.LEFT)
        ttk.Button(cf, text="Set Rate", command=self._cmd_set_rate).pack(side=tk.LEFT, padx=(8, 4))

        # Stream & MSC
        sf = ttk.Frame(f)
        sf.pack(fill=tk.X, pady=(10, 0))
        self.stream_btn = ttk.Button(sf, text="Stream ON", width=12, command=self._toggle_stream)
        self.stream_btn.pack(side=tk.LEFT, padx=(0, 8))
        self.msc_btn = ttk.Button(sf, text="MSC ON", width=12, command=self._toggle_msc)
        self.msc_btn.pack(side=tk.LEFT)

        # Status labels
        self.log_status_lbl = ttk.Label(f, text="", style="Status.TLabel")
        self.log_status_lbl.pack(fill=tk.X, pady=(10, 0))

    def _build_files_tab(self, nb):
        f = ttk.Frame(nb, padding=10)
        nb.add(f, text="Files")

        bt = ttk.Frame(f)
        bt.pack(fill=tk.X)
        ttk.Button(bt, text="Refresh", command=self._cmd_ls).pack(side=tk.LEFT, padx=(0, 4))
        ttk.Button(bt, text="Download", command=self._cmd_download).pack(side=tk.LEFT, padx=(0, 4))
        ttk.Button(bt, text="Delete", command=self._cmd_delete).pack(side=tk.LEFT)
        ttk.Label(bt, text="  Name:").pack(side=tk.LEFT, padx=(8, 4))
        self.file_var = tk.StringVar()
        ttk.Entry(bt, textvariable=self.file_var, width=24).pack(side=tk.LEFT, fill=tk.X, expand=True)

        self.file_list = tk.Listbox(f, font=("Consolas", 9), height=12, exportselection=False)
        self.file_list.pack(fill=tk.BOTH, expand=True, pady=(6, 0))
        sb = ttk.Scrollbar(self.file_list, orient=tk.VERTICAL, command=self.file_list.yview)
        self.file_list.configure(yscrollcommand=sb.set)
        sb.pack(side=tk.RIGHT, fill=tk.Y)
        self.file_list.bind("<<ListboxSelect>>", self._on_file_select)

        tf = ttk.Frame(f)
        tf.pack(fill=tk.X, pady=(6, 0))
        ttk.Button(tf, text="Export BIN → CSV", command=self._cmd_export_csv).pack(side=tk.LEFT, padx=(0, 4))
        ttk.Label(tf, text="Source .bin:").pack(side=tk.LEFT, padx=(8, 4))
        self.export_var = tk.StringVar()
        ttk.Entry(tf, textvariable=self.export_var, width=20).pack(side=tk.LEFT, fill=tk.X, expand=True)

    def _build_config_tab(self, nb):
        f = ttk.Frame(nb, padding=10)
        nb.add(f, text="Config")

        ttk.Label(f, text="CONFIG.TXT", style="Heading.TLabel").pack(anchor=tk.W)
        self.config_text = tk.Text(f, font=("Consolas", 9), height=12, wrap=tk.NONE)
        self.config_text.pack(fill=tk.BOTH, expand=True, pady=(4, 6))

        hsc = ttk.Scrollbar(self.config_text, orient=tk.HORIZONTAL, command=self.config_text.xview)
        self.config_text.configure(xscrollcommand=hsc.set)
        hsc.pack(side=tk.BOTTOM, fill=tk.X)

        bf = ttk.Frame(f)
        bf.pack(fill=tk.X)
        ttk.Button(bf, text="Read Config", command=self._cmd_read_cfg).pack(side=tk.LEFT, padx=(0, 6))
        self.cfg_key_var = tk.StringVar()
        ttk.Entry(bf, textvariable=self.cfg_key_var, width=14).pack(side=tk.LEFT, padx=(0, 4))
        ttk.Label(bf, text="=").pack(side=tk.LEFT, padx=(0, 4))
        self.cfg_val_var = tk.StringVar()
        ttk.Entry(bf, textvariable=self.cfg_val_var, width=14).pack(side=tk.LEFT, padx=(0, 4))
        ttk.Button(bf, text="Set", command=self._cmd_set_cfg).pack(side=tk.LEFT)

    def _build_console_tab(self, nb):
        f = ttk.Frame(nb, padding=10)
        nb.add(f, text="Console")

        self.console = tk.Text(f, font=("Consolas", 9), height=16, wrap=tk.NONE,
                               state=tk.DISABLED, bg="#1e1e1e", fg="#d4d4d4",
                               insertbackground="#d4d4d4")
        self.console.pack(fill=tk.BOTH, expand=True)
        sb_y = ttk.Scrollbar(self.console, orient=tk.VERTICAL, command=self.console.yview)
        self.console.configure(yscrollcommand=sb_y.set)
        sb_y.pack(side=tk.RIGHT, fill=tk.Y)
        sb_x = ttk.Scrollbar(self.console, orient=tk.HORIZONTAL, command=self.console.xview)
        self.console.configure(xscrollcommand=sb_x.set)
        sb_x.pack(side=tk.BOTTOM, fill=tk.X)

        cf = ttk.Frame(f)
        cf.pack(fill=tk.X, pady=(6, 0))
        ttk.Button(cf, text="Clear Console", command=self._clear_console).pack(side=tk.LEFT)

    # ─────────────── Helpers ───────────────

    def _log(self, msg):
        self.console.configure(state=tk.NORMAL)
        self.console.insert(tk.END, msg + "\n")
        self.console.see(tk.END)
        self.console.configure(state=tk.DISABLED)

    def _clear_console(self):
        self.console.configure(state=tk.NORMAL)
        self.console.delete("1.0", tk.END)
        self.console.configure(state=tk.DISABLED)

    def _dot_on(self):
        self.conn_indicator.delete("all")
        self.conn_indicator.create_oval(2, 2, 14, 14, fill="#2ecc71", outline="")

    def _dot_off(self):
        self.conn_indicator.delete("all")
        self.conn_indicator.create_oval(2, 2, 14, 14, fill="#bbb", outline="")

    def _scan_ports(self):
        ports = []
        try:
            for p in serial.tools.list_ports.comports():
                label = f"{p.device}"
                if p.description:
                    label += f" ({p.description})"
                ports.append((p.device, label))
        except Exception:
            pass
        devices = [pl for d, pl in ports]
        self.port_cb["values"] = devices
        if devices:
            self.port_cb.current(0)
        else:
            self.port_cb.set("")

    def _toggle_connect(self):
        if self.connected:
            self._disconnect()
        else:
            self._connect()

    def _connect(self):
        raw = self.port_var.get()
        if not raw:
            messagebox.showwarning("No port", "Select a COM port first.")
            return
        port = raw.split()[0]
        try:
            self.ser = Transport(port)
            self.ser.open()
        except Exception as e:
            messagebox.showerror("Connection failed", str(e))
            self.ser = None
            return
        self.connected = True
        self.connect_btn.configure(text="Disconnect")
        self._dot_on()
        self._log(f"Connected to {port}")
        self._cmd_refresh_info()

    def _disconnect(self):
        if self.ser:
            try:
                self.ser.close()
            except Exception:
                pass
            self.ser = None
        self.connected = False
        self.connect_btn.configure(text="Connect")
        self._dot_off()
        self._log("Disconnected")
        self.idn_lbl.configure(text="(not connected)")
        self.status_lbl.configure(text="")

    # ─────────────── Async command helpers ───────────────

    def _cmd(self, cmd, arg="", cb=None):
        if not self._check_connected():
            return
        ser = self.ser

        def work():
            ser.send_line(encode(cmd, arg))
            return ser.read_line(timeout=5.0)

        def done(result, error):
            if error:
                self._log(f"Error: {error}")
                self._safe_status(f"Error: {error}")
                return
            result = result.strip()
            self._log(f">> {cmd} {arg}\n   {result}")
            if cb:
                cb(result)
            else:
                self._safe_status(result)

        _run_async(work, done)

    def _cmd_raw(self, raw_cmd, cb=None):
        if not self._check_connected():
            return
        ser = self.ser

        def work():
            ser.send_line(raw_cmd)
            lines = []
            while True:
                line = ser.read_line(timeout=3.0)
                if not line:
                    break
                status, body = decode(line)
                lines.append(line)
                if status == "OK":
                    break
            return "\n".join(lines)

        def done(result, error):
            if error:
                self._log(f"Error: {error}")
                return
            self._log(f">> {raw_cmd}\n{result}")
            if cb:
                cb(result)

        _run_async(work, done)

    def _cmd_ls(self):
        self.file_list.delete(0, tk.END)

        if not self._check_connected():
            return
        ser = self.ser

        def work():
            ser.send_line("LS")
            lines = []
            while True:
                line = ser.read_line(timeout=5.0)
                if not line:
                    break
                status, body = decode(line)
                if status == "OK":
                    break
                lines.append(line)
            return lines

        def done(result, error):
            if error:
                self._log(f"Error: {error}")
                return
            self._log(f">> LS\n" + "\n".join(result))
            for line in result:
                self.file_list.insert(tk.END, line)

        _run_async(work, done)

    def _cmd_download(self):
        name = self.file_var.get().strip()
        if not name:
            messagebox.showwarning("No file", "Enter or select a file name.")
            return
        if not self._check_connected():
            return
        ser = self.ser
        self._log(f"Downloading {name} ...")

        def work():
            buf = io.BytesIO()
            data = receive(ser, name, buf, timeout=15.0)
            return data

        def done(result, error):
            if error:
                self._log(f"Download failed: {error}")
                messagebox.showerror("Download failed", str(error))
                return
            safe = name.replace("/", "_").replace("\\", "_")
            path = os.path.join(os.getcwd(), safe)
            with open(path, "wb") as f:
                f.write(result)
            self._log(f"Downloaded {len(result)} bytes to {path}")
            messagebox.showinfo("Download OK", f"{len(result)} bytes → {path}")

        _run_async(work, done)

    def _cmd_delete(self):
        name = self.file_var.get().strip()
        if not name:
            return
        if not messagebox.askyesno("Confirm", f"Delete {name}?"):
            return
        self._cmd("DEL", name)

    def _cmd_set_rate(self):
        try:
            ms = int(self.rate_var.get())
        except ValueError:
            return
        self._cmd("RATE", str(ms))

    def _cmd_sync_time(self):
        now = pc_epoch()
        self._cmd("TIME", str(now), cb=lambda r: self._cmd_refresh_info())

    def _cmd_refresh_info(self):
        if not self._check_connected():
            return
        self._cmd("*IDN?", cb=lambda r: self.idn_lbl.configure(text=r))
        self._cmd("STAT?", cb=self._update_status)
        self._cmd("TIME?", cb=lambda r: self.time_lbl.configure(text=r))

    def _cmd_read_cfg(self):
        def cb(result):
            try:
                body = expect_ok(result)
            except Exception:
                self.config_text.delete("1.0", tk.END)
                self.config_text.insert("1.0", result)
                return
            self.config_text.delete("1.0", tk.END)
            self.config_text.insert("1.0", body)

        self._cmd("CFG?", cb=cb)

    def _cmd_set_cfg(self):
        kv = f"{self.cfg_key_var.get().strip()}={self.cfg_val_var.get().strip()}"
        if "=" not in kv or kv == "=":
            return
        self._cmd("CFG", kv, cb=lambda r: self._cmd_read_cfg())

    def _cmd_export_csv(self):
        path = self.export_var.get().strip()
        if not path:
            messagebox.showwarning("No file", "Enter a .bin file path.")
            return
        if not os.path.exists(path):
            messagebox.showerror("Not found", f"File not found:\n{path}")
            return
        try:
            _, records = read_binary_file(path)
            csv_path = os.path.splitext(path)[0] + ".csv"
            with open(csv_path, "w", newline="") as fp:
                export_csv(records, fp)
            self._log(f"Exported {len(records)} records → {csv_path}")
            messagebox.showinfo("Export OK", f"{len(records)} records → {csv_path}")
        except Exception as e:
            messagebox.showerror("Export failed", str(e))

    def _toggle_stream(self):
        current = self.stream_btn.cget("text")
        state = current.split()[1]          # "ON" or "OFF" from the label
        next_label = "Stream OFF" if state == "ON" else "Stream ON"
        self._cmd("STREAM", state, cb=lambda r: self.stream_btn.configure(text=next_label))

    def _toggle_msc(self):
        current = self.msc_btn.cget("text")
        state = current.split()[1]          # "ON" or "OFF" from the label
        next_label = "MSC OFF" if state == "ON" else "MSC ON"
        self._cmd("MSC", state, cb=lambda r: self.msc_btn.configure(text=next_label))

    def _update_status(self, result):
        try:
            body = expect_ok(result)
        except Exception:
            self.status_lbl.configure(text=result)
            return
        self.status_lbl.configure(text=body)
        parts = body.split()
        for p in parts:
            if "=" in p:
                k, v = p.split("=", 1)
                if k == "logging":
                    self.log_status_lbl.configure(
                        text=f"Logging: {'ACTIVE' if v == '1' else 'STOPPED'}")

    def _safe_status(self, msg):
        try:
            self.status_lbl.configure(text=msg[:120])
        except Exception:
            pass

    def _check_connected(self):
        if not self.connected or not self.ser:
            messagebox.showwarning("Not connected", "Connect to the device first.")
            return False
        return True

    def _on_file_select(self, _evt):
        sel = self.file_list.curselection()
        if sel:
            line = self.file_list.get(sel[0])
            parts = line.split()
            if len(parts) >= 2:
                self.file_var.set(parts[1])

    def on_close(self):
        self._disconnect()
        self.root.destroy()


def gui_main():
    root = tk.Tk()
    app = DataloggerGUI(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()


if __name__ == "__main__":
    gui_main()
