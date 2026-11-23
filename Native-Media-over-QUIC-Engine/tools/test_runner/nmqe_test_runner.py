#!/usr/bin/env python3
"""
NMQE Test Runner
A GUI for launching the native MoQ player (build/nmqe-player) or the Shaka MoQ
reference page in Chrome against a configured or ad-hoc relay endpoint, and for
summarizing each run.

Endpoints live in config/endpoints.json (not committed; see
config/endpoints.example.json). Run files are written to runs/.
"""

import datetime
import functools
import glob
import json
import os
import queue
import re
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time
import tkinter as tk
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from tkinter import messagebox, scrolledtext, simpledialog, ttk
from urllib.parse import urlencode

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PLAYER = os.path.join(ROOT, "build", "nmqe-player")
RUNS_DIR = os.path.join(ROOT, "runs")
CONFIG_PATH = os.path.join(ROOT, "config", "endpoints.json")
PLATFORM_DIR = os.path.join(ROOT, "config", "platform")
SHAKA_DIR = os.path.join(ROOT, "docs", "reference")
SHAKA_PAGE = "Shaka-MoQ.html"
SUMMARIZE = os.path.join(ROOT, "tools", "summarize_runs.py")
CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"

TRANSPORT_LOG_PRESETS = ["", "warn", "info", "info,moq_tokio=debug", "debug", "info,moq_quic=trace"]
STOP_GRACE_SEC = 10
PAUSE_BETWEEN_RUNS_SEC = 1
POLL_SEC = 0.25

# Furthest-stage diagnosis: the first stage not reached explains the run. Each
# stage lists the milestones (any of) that count as reaching it.
NATIVE_STAGES = [
    (("transport_connecting",), "The transport was never started."),
    (("setup_complete",),
     "The relay handshake (QUIC + WebTransport + MOQT SETUP) did not complete. "
     "Check the VPN and the UDP path to the relay."),
    (("broadcast_ready",),
     "Connected to the relay, but namespace '{ns}' was never announced. "
     "The publisher is probably offline or not routed to this relay."),
    (("catalog_received",), "The namespace was announced, but no catalog was received."),
    (("first_object_video", "first_object_audio"), "The catalog was received, but no media Objects arrived."),
    (("first_frame", "first_audio_at_sink"),
     "Media Objects arrived, but nothing reached a sink. Check the pipeline and decoders."),
    (("stable_playback",), "Playback started but never became stable."),
]


def load_endpoints():
    if not os.path.exists(CONFIG_PATH):
        return []
    with open(CONFIG_PATH) as f:
        return json.load(f).get("endpoints", [])


def save_endpoints(endpoints):
    with open(CONFIG_PATH, "w") as f:
        json.dump({"endpoints": endpoints}, f, indent=2)
        f.write("\n")


def shaka_version(moqt_version):
    """moq-transport-22 -> draft-22, the naming Shaka's MSF parser uses."""
    match = re.fullmatch(r"moq-transport-(\d+)", moqt_version)
    return f"draft-{match.group(1)}" if match else moqt_version


def run_stem():
    os.makedirs(RUNS_DIR, exist_ok=True)
    stem = os.path.join(RUNS_DIR, datetime.datetime.now().strftime("%Y%m%d-%H%M%S"))
    candidate, n = stem, 1
    while glob.glob(candidate + "*"):
        n += 1
        candidate = f"{stem}-{n}"
    return candidate


def read_native_summary(telemetry_path):
    if not os.path.exists(telemetry_path):
        return None
    with open(telemetry_path) as f:
        for line in f:
            try:
                event = json.loads(line)
            except json.JSONDecodeError:
                continue
            if event.get("cat") == "summary" and event.get("ev") == "run":
                return event
    return None


def ms(value):
    return "-" if value is None or value < 0 else f"{value:.0f} ms"


def native_report(summary, namespace, exit_code):
    """Returns (status, lines): status is ok|warn|fail."""
    if summary is None:
        return "fail", [f"No run summary in telemetry (exit code {exit_code}). See the output log."]
    milestones = summary.get("milestones_ms", {})
    end_state = summary.get("end_state", "?")
    reason = summary.get("end_reason", "")
    diagnosis, status = "Playback reached stable playback.", "ok"
    for names, message in NATIVE_STAGES:
        if not any(name in milestones for name in names):
            diagnosis = message.format(ns=namespace)
            status = "warn" if "first_frame" in milestones else "fail"
            break
    if end_state == "failed":
        status = "fail"
    furthest = max(milestones.items(), key=lambda item: item[1]) if milestones else None
    live_edge = summary.get("live_edge_ms") or {}
    lines = [
        f"Result:    {end_state.upper()} ({reason})",
        f"Diagnosis: {diagnosis}",
        f"Furthest:  {furthest[0]} +{furthest[1]:.0f} ms" if furthest else "Furthest:  -",
        "Tune:      first_frame={} stable_playback={} stable_live_edge={}".format(
            ms(summary.get("tune_to_first_frame_ms")), ms(summary.get("tune_to_stable_playback_ms")),
            ms(summary.get("tune_to_stable_live_edge_ms"))),
        "Playback:  stalls={} ({} total) live_edge p50={} run={}".format(
            summary.get("stalls", 0), ms(summary.get("stall_total_ms")),
            ms(live_edge.get("p50")) if live_edge.get("n") else "-", ms(summary.get("run_ms"))),
    ]
    return status, lines


class ShakaRun:
    """Collects the events the Shaka page reports and turns them into a summary."""

    MARKERS = [
        ("connected", "Connection established"),
        ("setup", "Received peer SETUP"),
        ("loaded", "MoQ stream loaded."),
    ]

    def __init__(self, namespace):
        self.namespace = namespace
        self.milestones = {}
        self.errors = []
        self.first_time = None
        self.last_status = None

    def add(self, event):
        t = event.get("t_ms", 0)
        if event.get("type") == "log":
            text = event.get("text", "")
            for name, marker in self.MARKERS:
                if marker in text:
                    self.milestones.setdefault(name, t)
            if event.get("level") == "error" and text not in self.errors:
                self.errors.append(text)
        elif event.get("type") == "video" and event.get("name") == "playing":
            self.milestones.setdefault("playing", t)
        elif event.get("type") == "status":
            self.last_status = event
            if self.first_time is None and event.get("current_time", 0) > 0:
                self.first_time = (t, event["current_time"])
            elif self.first_time and event["current_time"] - self.first_time[1] >= 1:
                self.milestones.setdefault("progressing", t)

    def stage(self):
        for name in ("progressing", "playing", "loaded", "setup", "connected"):
            if name in self.milestones:
                return name
        return "page loaded" if self.last_status else "starting"

    def report(self):
        m, errors = self.milestones, " ".join(self.errors)
        status = "fail"
        if not self.last_status and not self.errors:
            diagnosis = "The page never reported back. Did Chrome open it?"
        elif "unroutable" in errors:
            diagnosis = (f"The relay has no route for namespace '{self.namespace}' (subscribe 'unroutable'). "
                         "The publisher is probably offline or not routed to this relay.")
        elif "connected" not in m:
            diagnosis = "The WebTransport session to the relay was not established. Check the VPN and relay URL."
        elif "loaded" not in m:
            diagnosis = "Connected, but the stream did not load. See the errors below."
        elif "playing" not in m:
            diagnosis = "The stream loaded, but the video element never started playing."
        elif "progressing" not in m:
            diagnosis, status = "Playback started, but the position did not advance.", "warn"
        else:
            diagnosis, status = "Playback is progressing.", "warn" if self.errors else "ok"
        s = self.last_status or {}
        lines = [
            f"Result:    {'OK' if status == 'ok' else 'ISSUES' if status == 'warn' else 'FAILED'}",
            f"Diagnosis: {diagnosis}",
            "Tune:      " + " ".join(f"{name}={m[name]} ms" for name in
                                     ("connected", "setup", "loaded", "playing", "progressing") if name in m),
            "Playback:  position={:.1f}s buffered_end={:.1f}s frames={} dropped={}".format(
                s.get("current_time", 0), s.get("buffered_end", 0), s.get("total_frames", 0),
                s.get("dropped_frames", 0)),
        ]
        lines += [f"Error:     {error}" for error in self.errors[:5]]
        return status, lines


class ReportHandler(SimpleHTTPRequestHandler):
    """Serves docs/reference and accepts the page's POST /report events."""

    def do_POST(self):
        if self.path != "/report":
            self.send_error(404)
            return
        body = self.rfile.read(int(self.headers.get("Content-Length", 0)))
        try:
            self.server.on_event(json.loads(body))
        except json.JSONDecodeError:
            pass
        self.send_response(204)
        self.end_headers()

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, *args):
        pass


class TestRunner:
    def __init__(self, root):
        self.root = root
        self.root.title("NMQE Test Runner")
        self.root.geometry("980x860")
        self.ui_queue = queue.Queue()
        self.endpoints = load_endpoints()
        self.stop_event = threading.Event()
        self.process = None
        self.worker = None
        self.session_telemetry = []
        self.setup_gui()
        self.process_ui_queue()
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)
        if self.endpoints:
            self.endpoint_combo.current(0)
            self.select_endpoint()
        else:
            self.log(f"No endpoints in {CONFIG_PATH}. Enter a relay below and click Save "
                     "(see config/endpoints.example.json).")

    # ---- layout -----------------------------------------------------------

    def setup_gui(self):
        main = ttk.Frame(self.root, padding=10)
        main.pack(fill=tk.BOTH, expand=True)

        endpoint = ttk.LabelFrame(main, text="Endpoint", padding=6)
        endpoint.pack(fill=tk.X, pady=4)
        row = ttk.Frame(endpoint)
        row.pack(fill=tk.X)
        ttk.Label(row, text="Saved:").pack(side=tk.LEFT)
        self.endpoint_combo = ttk.Combobox(row, state="readonly", width=50,
                                           values=[e["name"] for e in self.endpoints])
        self.endpoint_combo.pack(side=tk.LEFT, padx=5)
        self.endpoint_combo.bind("<<ComboboxSelected>>", lambda _: self.select_endpoint())
        ttk.Button(row, text="Save", command=self.save_endpoint).pack(side=tk.LEFT, padx=2)
        ttk.Button(row, text="Delete", command=self.delete_endpoint).pack(side=tk.LEFT, padx=2)

        self.relay_var = tk.StringVar()
        self.namespace_var = tk.StringVar(value="mediakind-test")
        self.version_var = tk.StringVar(value="moq-transport-22")
        for label, var, width in (("Relay URL:", self.relay_var, 70), ("Namespace:", self.namespace_var, 30),
                                  ("MOQT version:", self.version_var, 30)):
            line = ttk.Frame(endpoint)
            line.pack(fill=tk.X, pady=2)
            ttk.Label(line, text=label, width=14).pack(side=tk.LEFT)
            ttk.Entry(line, textvariable=var, width=width).pack(side=tk.LEFT, fill=tk.X, expand=width > 50)

        run = ttk.LabelFrame(main, text="Run", padding=6)
        run.pack(fill=tk.X, pady=4)
        line = ttk.Frame(run)
        line.pack(fill=tk.X, pady=2)
        self.player_var = tk.StringVar(value="native")
        ttk.Label(line, text="Player:", width=14).pack(side=tk.LEFT)
        ttk.Radiobutton(line, text="Native (nmqe-player)", value="native", variable=self.player_var).pack(side=tk.LEFT)
        ttk.Radiobutton(line, text="Shaka (Chrome)", value="shaka", variable=self.player_var).pack(side=tk.LEFT, padx=10)
        ttk.Label(line, text="Duration (s, 0 = until Stop):").pack(side=tk.LEFT, padx=(20, 2))
        self.duration_var = tk.StringVar(value="30")
        ttk.Spinbox(line, from_=0, to=3600, textvariable=self.duration_var, width=6).pack(side=tk.LEFT)
        ttk.Label(line, text="Repeat:").pack(side=tk.LEFT, padx=(15, 2))
        self.repeat_var = tk.StringVar(value="1")
        ttk.Spinbox(line, from_=1, to=100, textvariable=self.repeat_var, width=4).pack(side=tk.LEFT)

        native = ttk.LabelFrame(main, text="Native options", padding=6)
        native.pack(fill=tk.X, pady=4)
        line = ttk.Frame(native)
        line.pack(fill=tk.X, pady=2)
        ttk.Label(line, text="Pipeline:").pack(side=tk.LEFT)
        self.pipeline_var = tk.StringVar(value="gst")
        ttk.Combobox(line, textvariable=self.pipeline_var, values=["gst", "null"], state="readonly",
                     width=6).pack(side=tk.LEFT, padx=(2, 12))
        ttk.Label(line, text="Platform:").pack(side=tk.LEFT)
        platforms = sorted(os.path.basename(p) for p in glob.glob(os.path.join(PLATFORM_DIR, "*.json")))
        self.platform_var = tk.StringVar(value="macos.json" if "macos.json" in platforms else "")
        ttk.Combobox(line, textvariable=self.platform_var, values=[""] + platforms, state="readonly",
                     width=16).pack(side=tk.LEFT, padx=(2, 12))
        ttk.Label(line, text="Log:").pack(side=tk.LEFT)
        self.log_level_var = tk.StringVar(value="info")
        ttk.Combobox(line, textvariable=self.log_level_var, values=["trace", "debug", "info", "warn", "error"],
                     state="readonly", width=7).pack(side=tk.LEFT, padx=(2, 12))
        ttk.Label(line, text="--transport-log:").pack(side=tk.LEFT)
        self.transport_log_var = tk.StringVar(value="warn")
        ttk.Combobox(line, textvariable=self.transport_log_var, values=TRANSPORT_LOG_PRESETS,
                     width=22).pack(side=tk.LEFT, padx=2)
        line = ttk.Frame(native)
        line.pack(fill=tk.X, pady=2)
        self.no_audio_var, self.no_video_var, self.trace_var = tk.BooleanVar(), tk.BooleanVar(), tk.BooleanVar()
        ttk.Checkbutton(line, text="--no-audio", variable=self.no_audio_var).pack(side=tk.LEFT)
        ttk.Checkbutton(line, text="--no-video", variable=self.no_video_var).pack(side=tk.LEFT, padx=8)
        ttk.Checkbutton(line, text="--trace", variable=self.trace_var).pack(side=tk.LEFT)
        ttk.Label(line, text="Extra args:").pack(side=tk.LEFT, padx=(20, 2))
        self.extra_args_var = tk.StringVar()
        ttk.Entry(line, textvariable=self.extra_args_var).pack(side=tk.LEFT, fill=tk.X, expand=True)

        buttons = ttk.Frame(main)
        buttons.pack(fill=tk.X, pady=8)
        self.run_button = ttk.Button(buttons, text="Run", command=self.start)
        self.run_button.pack(side=tk.LEFT, padx=3)
        self.stop_button = ttk.Button(buttons, text="Stop", command=self.stop, state=tk.DISABLED)
        self.stop_button.pack(side=tk.LEFT, padx=3)
        self.build_button = ttk.Button(buttons, text="Build", command=self.build)
        self.build_button.pack(side=tk.LEFT, padx=3)
        ttk.Button(buttons, text="Summarize Session Runs", command=self.summarize).pack(side=tk.LEFT, padx=3)
        ttk.Button(buttons, text="Open runs/", command=lambda: subprocess.Popen(["open", RUNS_DIR])).pack(side=tk.LEFT, padx=3)
        ttk.Button(buttons, text="Clear", command=self.clear).pack(side=tk.LEFT, padx=3)

        status = ttk.Frame(main)
        status.pack(fill=tk.X, pady=2)
        ttk.Label(status, text="Stage:").pack(side=tk.LEFT)
        self.stage_var = tk.StringVar(value="idle")
        ttk.Label(status, textvariable=self.stage_var, width=60).pack(side=tk.LEFT, padx=5)
        self.progress = ttk.Progressbar(status, mode="indeterminate", length=200)
        self.progress.pack(side=tk.RIGHT)

        panes = ttk.PanedWindow(main, orient=tk.VERTICAL)
        panes.pack(fill=tk.BOTH, expand=True, pady=4)
        log_frame = ttk.LabelFrame(panes, text="Output Log")
        self.log_text = scrolledtext.ScrolledText(log_frame, height=16, wrap=tk.NONE, font=("Menlo", 11))
        self.log_text.pack(fill=tk.BOTH, expand=True)
        results_frame = ttk.LabelFrame(panes, text="Results Summary")
        self.results_text = scrolledtext.ScrolledText(results_frame, height=12, wrap=tk.WORD, font=("Menlo", 11))
        self.results_text.pack(fill=tk.BOTH, expand=True)
        self.results_text.tag_configure("ok", foreground="#1a7f37")
        self.results_text.tag_configure("warn", foreground="#b58100")
        self.results_text.tag_configure("fail", foreground="#cf222e")
        self.results_text.tag_configure("head", font=("Menlo", 11, "bold"))
        panes.add(log_frame, weight=3)
        panes.add(results_frame, weight=2)

    # ---- thread-safe UI helpers ---------------------------------------------

    def process_ui_queue(self):
        while True:
            try:
                func, args = self.ui_queue.get_nowait()
            except queue.Empty:
                break
            func(*args)
        self.root.after(100, self.process_ui_queue)

    def ui(self, func, *args):
        self.ui_queue.put((func, args))

    def log(self, message):
        self.ui(self._append, self.log_text, message + "\n", None)

    def result(self, text, tag=None):
        self.ui(self._append, self.results_text, text + "\n", tag)

    @staticmethod
    def _append(widget, text, tag):
        widget.insert(tk.END, text, tag)
        widget.see(tk.END)

    def set_stage(self, text):
        self.ui(self.stage_var.set, text)

    def set_running(self, running):
        def apply():
            self.run_button.config(state=tk.DISABLED if running else tk.NORMAL)
            self.build_button.config(state=tk.DISABLED if running else tk.NORMAL)
            self.stop_button.config(state=tk.NORMAL if running else tk.DISABLED)
            (self.progress.start if running else self.progress.stop)()
            if not running:
                self.stage_var.set("idle")
        self.ui(apply)

    def clear(self):
        self.log_text.delete(1.0, tk.END)
        self.results_text.delete(1.0, tk.END)

    # ---- endpoints -----------------------------------------------------------

    def select_endpoint(self):
        endpoint = self.endpoints[self.endpoint_combo.current()]
        self.relay_var.set(endpoint.get("relay", ""))
        self.namespace_var.set(endpoint.get("namespace", "mediakind-test"))
        self.version_var.set(endpoint.get("moqt_version", "moq-transport-22"))

    def save_endpoint(self):
        if not self.relay_var.get().strip():
            messagebox.showerror("Save endpoint", "Enter a relay URL first.")
            return
        name = simpledialog.askstring("Save endpoint", "Endpoint name:", parent=self.root,
                                      initialvalue=self.endpoint_combo.get() or self.namespace_var.get())
        if not name:
            return
        entry = {"name": name, "relay": self.relay_var.get().strip(), "namespace": self.namespace_var.get().strip(),
                 "moqt_version": self.version_var.get().strip()}
        names = [e["name"] for e in self.endpoints]
        if name in names:
            self.endpoints[names.index(name)] = entry
        else:
            self.endpoints.append(entry)
        save_endpoints(self.endpoints)
        self.refresh_endpoints(name)
        self.log(f"Saved endpoint '{name}' to {CONFIG_PATH}")

    def delete_endpoint(self):
        index = self.endpoint_combo.current()
        if index < 0:
            return
        name = self.endpoints[index]["name"]
        if not messagebox.askyesno("Delete endpoint", f"Delete '{name}' from {CONFIG_PATH}?"):
            return
        del self.endpoints[index]
        save_endpoints(self.endpoints)
        self.refresh_endpoints(None)

    def refresh_endpoints(self, selected):
        names = [e["name"] for e in self.endpoints]
        self.endpoint_combo.config(values=names)
        if selected in names:
            self.endpoint_combo.current(names.index(selected))
        else:
            self.endpoint_combo.set("")

    # ---- running -------------------------------------------------------------

    def start(self):
        try:
            duration, repeat = int(self.duration_var.get()), int(self.repeat_var.get())
        except ValueError:
            messagebox.showerror("Run", "Duration and repeat must be numbers.")
            return
        settings = {
            "player": self.player_var.get(),
            "endpoint": self.endpoint_combo.get() or "ad-hoc",
            "relay": self.relay_var.get().strip(),
            "namespace": self.namespace_var.get().strip(),
            "version": self.version_var.get().strip(),
            "duration": duration,
            "repeat": repeat,
        }
        if not settings["relay"]:
            messagebox.showerror("Run", "Enter a relay URL.")
            return
        if repeat > 1 and duration <= 0:
            messagebox.showerror("Run", "Repeated runs need a duration.")
            return
        if settings["player"] == "native":
            if not os.path.exists(PLAYER):
                messagebox.showerror("Run", f"{PLAYER} not found. Click Build first.")
                return
            try:
                settings["command"] = self.native_command(settings)
            except ValueError as err:
                messagebox.showerror("Run", f"Extra args: {err}")
                return
        elif not os.path.exists(CHROME):
            messagebox.showerror("Run", f"Google Chrome not found at {CHROME}.")
            return
        self.stop_event.clear()
        self.set_running(True)
        self.worker = threading.Thread(target=self.run_all, args=(settings,), daemon=True)
        self.worker.start()

    def native_command(self, s):
        cmd = [PLAYER, "--relay", s["relay"], "--namespace", s["namespace"], "--moqt-version", s["version"],
               "--pipeline", self.pipeline_var.get(), "--log", self.log_level_var.get()]
        if self.platform_var.get():
            cmd += ["--platform", os.path.join(PLATFORM_DIR, self.platform_var.get())]
        if s["duration"] > 0:
            cmd += ["--duration", str(s["duration"])]
        if self.transport_log_var.get().strip():
            cmd += ["--transport-log", self.transport_log_var.get().strip()]
        for flag, var in (("--no-audio", self.no_audio_var), ("--no-video", self.no_video_var),
                          ("--trace", self.trace_var)):
            if var.get():
                cmd.append(flag)
        return cmd + shlex.split(self.extra_args_var.get())

    def stop(self):
        self.stop_event.set()
        process = self.process
        if process and process.poll() is None:
            self.log("Stopping...")
            process.send_signal(signal.SIGINT)

    def run_all(self, settings):
        try:
            for attempt in range(1, settings["repeat"] + 1):
                if self.stop_event.is_set():
                    break
                label = f"Run {attempt}/{settings['repeat']}" if settings["repeat"] > 1 else "Run"
                started = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                head = f"=== {label} · {settings['player']} · {settings['endpoint']} · {started} ==="
                self.log(head)
                run = self.run_native if settings["player"] == "native" else self.run_shaka
                status, lines, files = run(settings)
                self.result(head, "head")
                self.result(lines[0], status)
                for line in lines[1:]:
                    self.result(line)
                self.result("Files:     " + ", ".join(os.path.relpath(f, ROOT) for f in files) + "\n")
                if attempt < settings["repeat"]:
                    self.stop_event.wait(PAUSE_BETWEEN_RUNS_SEC)
        except Exception as err:
            self.log(f"Runner error: {err!r}")
        finally:
            self.process = None
            self.set_running(False)

    def wait_or_kill(self, process):
        try:
            process.wait(timeout=STOP_GRACE_SEC)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()

    def run_native(self, settings):
        stem = run_stem()
        telemetry, log_path = stem + ".jsonl", stem + ".log"
        cmd = settings["command"] + ["--telemetry", telemetry]
        self.log("$ " + shlex.join(cmd))
        milestone = re.compile(r"\[player\] milestone (\S+)\s+\+\s*([\d.]+) ms")
        status_line = re.compile(r"\[status\] (.*)")
        last_milestone = "starting"
        with open(log_path, "w") as log_file:
            self.process = subprocess.Popen(cmd, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                            text=True, bufsize=1)
            for line in self.process.stdout:
                log_file.write(line)
                line = line.rstrip()
                self.log(line)
                if m := milestone.search(line):
                    last_milestone = f"{m.group(1)} +{float(m.group(2)):.0f} ms"
                    self.set_stage(last_milestone)
                elif m := status_line.search(line):
                    self.set_stage(f"{last_milestone} | {m.group(1)}")
            self.wait_or_kill(self.process)
        exit_code = self.process.returncode
        self.log(f"nmqe-player exited with code {exit_code}")
        self.session_telemetry.append(telemetry)
        status, lines = native_report(read_native_summary(telemetry), settings["namespace"], exit_code)
        return status, lines, [telemetry, log_path]

    def run_shaka(self, settings):
        events_path = run_stem() + "-shaka.jsonl"
        shaka = ShakaRun(settings["namespace"])
        lock = threading.Lock()
        events_file = open(events_path, "w")

        def on_event(event):
            event["wall_ms"] = int(datetime.datetime.now().timestamp() * 1000)
            with lock:
                events_file.write(json.dumps(event) + "\n")
                shaka.add(event)
                stage = shaka.stage()
            if event.get("type") == "log":
                self.log(f"[shaka +{event.get('t_ms', 0)} ms] {event.get('level')}: {event.get('text')}")
            elif event.get("type") == "video":
                self.log(f"[shaka +{event.get('t_ms', 0)} ms] video {event.get('name')} at {event.get('current_time', 0):.2f}s")
            elif event.get("type") == "status":
                stage += " | pos={:.1f}s buffered={:.1f}s".format(event.get("current_time", 0),
                                                                  event.get("buffered_end", 0))
            self.set_stage(stage)

        server = ThreadingHTTPServer(("127.0.0.1", 0), functools.partial(ReportHandler, directory=SHAKA_DIR))
        server.on_event = on_event
        threading.Thread(target=server.serve_forever, daemon=True).start()
        query = urlencode({"relay": settings["relay"], "ns": settings["namespace"],
                           "version": shaka_version(settings["version"]), "report": 1})
        url = f"http://127.0.0.1:{server.server_address[1]}/{SHAKA_PAGE}?{query}"
        # A throwaway profile makes this a separate Chrome instance we can stop, with a cold cache.
        profile = tempfile.mkdtemp(prefix="nmqe-chrome-")
        cmd = [CHROME, f"--user-data-dir={profile}", "--no-first-run", "--no-default-browser-check",
               "--new-window", url]
        self.log("$ " + shlex.join(cmd))
        try:
            self.process = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            deadline = time.monotonic() + settings["duration"] if settings["duration"] > 0 else None
            while not self.stop_event.wait(POLL_SEC):
                if self.process.poll() is not None:
                    self.log("Chrome exited")
                    break
                if deadline and time.monotonic() >= deadline:
                    self.log(f"Duration {settings['duration']}s reached")
                    break
            if self.process.poll() is None:
                self.process.terminate()
            self.wait_or_kill(self.process)
        finally:
            server.shutdown()
            server.server_close()
            shutil.rmtree(profile, ignore_errors=True)
            with lock:
                status, lines = shaka.report()
                events_file.write(json.dumps({"cat": "summary", "ev": "shaka", "status": status,
                                              "milestones_ms": shaka.milestones, "errors": shaka.errors,
                                              "last_status": shaka.last_status}) + "\n")
                events_file.close()
        return status, lines, [events_path]

    # ---- build / summarize ---------------------------------------------------

    def build(self):
        self.set_running(True)
        threading.Thread(target=self.run_build, daemon=True).start()

    def run_build(self):
        try:
            env = dict(os.environ)
            gst = subprocess.run(["brew", "--prefix", "gstreamer"], capture_output=True, text=True).stdout.strip()
            env["PKG_CONFIG_PATH"] = f"{gst or '/opt/homebrew/opt/gstreamer'}/lib/pkgconfig:/opt/homebrew/lib/pkgconfig"
            env["PATH"] = os.path.expanduser("~/.cargo/bin") + os.pathsep + env.get("PATH", "")
            steps = [["ninja", "-C", "build"]]
            if not os.path.exists(os.path.join(ROOT, "build", "build.ninja")):
                steps.insert(0, ["cmake", "-S", ".", "-B", "build", "-G", "Ninja"])
            for cmd in steps:
                self.log("$ " + shlex.join(cmd))
                self.process = subprocess.Popen(cmd, cwd=ROOT, env=env, stdout=subprocess.PIPE,
                                                stderr=subprocess.STDOUT, text=True, bufsize=1)
                for line in self.process.stdout:
                    self.log(line.rstrip())
                if self.process.wait() != 0:
                    self.result(f"Build failed: {shlex.join(cmd)} (exit {self.process.returncode})\n", "fail")
                    return
            self.result("Build succeeded\n", "ok")
        except OSError as err:
            self.result(f"Build failed: {err}\n", "fail")
        finally:
            self.process = None
            self.set_running(False)

    def summarize(self):
        files = [f for f in self.session_telemetry if os.path.exists(f)]
        if not files:
            messagebox.showinfo("Summarize", "No native runs in this session yet.")
            return
        out = subprocess.run([sys.executable, SUMMARIZE] + files, cwd=ROOT, capture_output=True, text=True)
        self.result(f"=== Summary of {len(files)} native session run(s) ===", "head")
        self.result((out.stdout + out.stderr).rstrip() + "\n")

    def on_close(self):
        self.stop()
        if self.worker and self.worker.is_alive():
            self.worker.join(timeout=STOP_GRACE_SEC + 2)
        self.root.destroy()


def main():
    root = tk.Tk()
    TestRunner(root)
    root.mainloop()


if __name__ == "__main__":
    main()
