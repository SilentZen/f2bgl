#!/usr/bin/env python3
"""Small, dependency-free GUI launcher for Fade To Black / f2bgl."""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
import threading
import tkinter as tk
from collections import deque
from pathlib import Path
from tkinter import filedialog, messagebox, ttk


BASE_DIR = Path(__file__).resolve().parent
FROZEN = bool(getattr(sys, "frozen", False))
LANGUAGES = ("EN", "FR", "GR", "SP", "IT")
VOICES = ("EN", "FR", "GR")
TEXTURE_FILTERS = ("linear", "nearest")
TEXTURE_SCALERS = ("point1x", "point2x", "scale2x", "point3x", "scale3x")
SHADERS = ("none", "crt-easymode", "crt-geom", "lcd-grid")
RESOLUTIONS = ("640x480", "800x600", "1024x768", "1280x720", "1280x960", "1600x900", "1920x1080")
CLI_HELP = """Fade To Black Launcher - command-line options
Usage: f2bgl.exe [OPTIONS]

  /? or --help                 Show this list of options
  --datapath=PATH              Path to PC data files (default '.')
  --language=EN|FR|GR|SP|IT    Language files to use (default 'EN')
  --playdemo                   Use inputs from .DEM files
  --level=NUM                  Start at level NUM
  --voice=EN|FR|GR             Voice files (default 'EN')
  --subtitles                  Display cutscene subtitles
  --savepath=PATH              Path to save files (default '.')
  --fullscreen                 Start fullscreen (Alt+Enter toggles)
  --resolution=WIDTHxHEIGHT    Window size (default 640x480)
  --fov=DEG                    Field of vision in degrees (75-130)
  --soundfont=FILE             SoundFont (.sf2) file for music
  --texturefilter=FILTER       Texture filter (default 'linear')
  --texturescaler=NAME         Texture scaler (default 'scale2x')
  --shader=NAME                Shader: none, crt-easymode, crt-geom, lcd-grid
  --mouse                      Enable mouse controls
  --no-fog                     Disable fog rendering
  --no-gouraud                 Disable gouraud shading
  --psxpath=PATH               Path to PlayStation data files
"""
CONTROL_ACTIONS = (
    ("move_left", "Move left"), ("move_right", "Move right"),
    ("move_up", "Move forward"), ("move_down", "Move backward"),
    ("gun_mode", "Toggle gun mode"), ("shoot", "Shoot"), ("walk", "Walk"),
    ("activate", "Activate"), ("reload_gun", "Reload gun"),
    ("change_inventory", "Change inventory category"), ("escape", "Menu / back"),
    ("inventory", "Inventory"), ("jump", "Jump"), ("use", "Use"),
    ("item1", "Select item 1"), ("item2", "Select item 2"),
    ("item3", "Select item 3"), ("item4", "Select item 4"),
    ("item5", "Select item 5"), ("foot_step", "Step forward"),
    ("back_step", "Step backward"),
)
SCANCODE_NAMES = {
    **{chr(ord("a") + i): (4 + i, chr(ord("A") + i)) for i in range(26)},
    **{str(i): (39 if i == 0 else 29 + i, str(i)) for i in range(10)},
    "Return": (40, "Enter"), "Escape": (41, "Escape"), "BackSpace": (42, "Backspace"),
    "Tab": (43, "Tab"), "space": (44, "Space"), "minus": (45, "-"),
    "equal": (46, "="), "bracketleft": (47, "["), "bracketright": (48, "]"),
    "backslash": (49, "\\"), "semicolon": (51, ";"), "apostrophe": (52, "'"),
    "grave": (53, "`"), "comma": (54, ","), "period": (55, "."), "slash": (56, "/"),
    "Caps_Lock": (57, "Caps Lock"), **{f"F{i}": (57 + i, f"F{i}") for i in range(1, 13)},
    "Print": (70, "Print Screen"), "Scroll_Lock": (71, "Scroll Lock"), "Pause": (72, "Pause"),
    "Insert": (73, "Insert"), "Home": (74, "Home"), "Prior": (75, "Page Up"),
    "Delete": (76, "Delete"), "End": (77, "End"), "Next": (78, "Page Down"),
    "Right": (79, "Right"), "Left": (80, "Left"), "Down": (81, "Down"), "Up": (82, "Up"),
    "Num_Lock": (83, "Num Lock"), "KP_Divide": (84, "Keypad /"), "KP_Multiply": (85, "Keypad *"),
    "KP_Subtract": (86, "Keypad -"), "KP_Add": (87, "Keypad +"), "KP_Enter": (88, "Keypad Enter"),
    **{f"KP_{i}": (98 if i == 0 else 88 + i, f"Keypad {i}") for i in range(10)},
    "KP_Decimal": (99, "Keypad ."), "Control_L": (224, "Left Ctrl"), "Shift_L": (225, "Left Shift"),
    "Alt_L": (226, "Left Alt"), "Control_R": (228, "Right Ctrl"), "Shift_R": (229, "Right Shift"),
    "Alt_R": (230, "Right Alt"),
}

DEFAULTS: dict[str, str | bool] = {
    "executable": "",
    "datapath": "",
    "psxpath": "",
    "savepath": "",
    "language": "EN",
    "playdemo": False,
    "level": "",
    "voice": "EN",
    "subtitles": False,
    "fullscreen": False,
    "fov": "",
    "soundfont": "",
    "texturefilter": "linear",
    "texturescaler": "scale2x",
    "shader": "none",
    "resolution": "640x480",
    "mouse": False,
    "no_fog": False,
    "no_gouraud": False,
}


def settings_file() -> Path:
    """Use the usual per-user config folder without adding a Python dependency."""
    if os.name == "nt":
        root = Path(os.environ.get("APPDATA", Path.home() / "AppData" / "Roaming"))
    else:
        root = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
    return root / "f2bgl" / "launcher.json"


def user_data_dir() -> Path:
    return settings_file().parent


def controls_file(executable: Path) -> Path:
    if FROZEN:
        config = user_data_dir() / "controls.cfg"
        if not config.exists():
            bundled = BASE_DIR / "controls.cfg"
            if bundled.is_file():
                config.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(bundled, config)
        return config
    return executable.parent / "controls.cfg"


def normalized_path(value: str) -> str:
    """Normalize user-entered paths to the current platform's separators."""
    cleaned = value.strip().strip('"')
    return os.path.normpath(os.path.expandvars(os.path.expanduser(cleaned))) if cleaned else ""


def load_settings() -> dict[str, str | bool]:
    values = DEFAULTS.copy()
    try:
        saved = json.loads(settings_file().read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        saved = {}
    if isinstance(saved, dict):
        for key, default in DEFAULTS.items():
            value = saved.get(key, default)
            if isinstance(default, bool):
                if isinstance(value, bool):
                    values[key] = value
            elif isinstance(value, str):
                values[key] = value
    if FROZEN:
        values["executable"] = str(Path(getattr(sys, "_MEIPASS", BASE_DIR)) / "f2bgl-engine.exe")
        if not values["datapath"]:
            values["datapath"] = str(Path(sys.executable).resolve().parent)
        if not values["savepath"]:
            values["savepath"] = str(user_data_dir() / "saves")
    elif not values["executable"]:
        candidates = (
            BASE_DIR / ("f2bgl.exe" if os.name == "nt" else "f2bgl"),
            BASE_DIR / "f2bgl.exe",
        )
        found = next((path for path in candidates if path.is_file()), None)
        if found is None:
            command = shutil.which("f2bgl.exe") or shutil.which("f2bgl")
            found = Path(command) if command else None
        if found:
            values["executable"] = str(found)
    return values


def save_settings(values: dict[str, str | bool]) -> None:
    path = settings_file()
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps(values, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


class ControlsDialog:
    def __init__(self, parent: tk.Tk, config_path: Path) -> None:
        self.config_path = config_path
        self.dialog = tk.Toplevel(parent)
        self.dialog.withdraw()
        self.dialog.overrideredirect(True)
        self.dialog.configure(background="#111318", highlightthickness=1, highlightbackground="#343943")
        self.dialog.transient(parent)
        self.dialog.resizable(False, False)
        self.dialog.geometry("520x740")
        self.bind_id: str | None = None
        self.active_action: str | None = None
        self.scancodes: dict[str, int] = {}
        self.key_labels: dict[str, tk.StringVar] = {}
        self._load()
        titlebar = ttk.Frame(self.dialog, padding=(12, 8))
        titlebar.pack(fill="x")
        title = ttk.Label(titlebar, text="KEYBOARD CONTROLS", style="PopupTitle.TLabel")
        title.pack(side="left")
        close_button = ttk.Button(titlebar, text="×", width=3, style="Close.TButton", command=self._close)
        close_button.pack(side="right")
        for widget in (titlebar, title):
            widget.bind("<ButtonPress-1>", self._start_move)
            widget.bind("<B1-Motion>", self._move_window)
        ttk.Label(
            self.dialog,
            text="Select a control, then press the key you want to assign.",
            padding=(12, 4, 12, 10),
            style="Muted.TLabel",
        ).pack(anchor="w")
        body = ttk.Frame(self.dialog, padding=(12, 0, 12, 8))
        body.pack(fill="both", expand=True)
        for action, label in CONTROL_ACTIONS:
            row = ttk.Frame(body)
            row.pack(fill="x", pady=2)
            ttk.Label(row, text=label, width=28).pack(side="left")
            value = tk.StringVar(value=self._key_label(self.scancodes.get(action)))
            self.key_labels[action] = value
            ttk.Label(row, textvariable=value, width=14).pack(side="left", padx=4)
            ttk.Button(row, text="Scan key…", command=lambda name=action: self._scan(name)).pack(side="right")
        footer = ttk.Frame(self.dialog, padding=12)
        footer.pack(fill="x")
        ttk.Button(footer, text="Cancel", command=self._close).pack(side="right")
        ttk.Button(footer, text="Save controls", command=self._save).pack(side="right", padx=(0, 8))
        self.dialog.protocol("WM_DELETE_WINDOW", self._close)
        self.dialog.bind("<Escape>", lambda _event: self._close())
        self.dialog.update_idletasks()
        width, height = 520, 740
        x = parent.winfo_rootx() + (parent.winfo_width() - width) // 2
        y = parent.winfo_rooty() + (parent.winfo_height() - height) // 2
        x = max(0, min(x, parent.winfo_screenwidth() - width))
        y = max(0, min(y, parent.winfo_screenheight() - height))
        self.dialog.geometry(f"{width}x{height}+{x}+{y}")
        self.dialog.deiconify()
        self.dialog.lift(parent)
        self.dialog.focus_force()
        self.dialog.grab_set()

    def _start_move(self, event: tk.Event) -> None:
        self._move_x = event.x_root - self.dialog.winfo_x()
        self._move_y = event.y_root - self.dialog.winfo_y()

    def _move_window(self, event: tk.Event) -> None:
        self.dialog.geometry(f"+{event.x_root - self._move_x}+{event.y_root - self._move_y}")

    def _load(self) -> None:
        try:
            lines = self.config_path.read_text(encoding="utf-8").splitlines()
        except OSError:
            lines = []
        action_names = {name for name, _label in CONTROL_ACTIONS}
        for line in lines:
            match = re.match(r"\s*keyboard\.scancode\.(\d+)\s*=\s*([a-z0-9_]+)", line)
            if match and match.group(2) in action_names and match.group(2) not in self.scancodes:
                self.scancodes[match.group(2)] = int(match.group(1))
        for action, scancode in (("move_left", 80), ("move_right", 79), ("move_up", 82), ("move_down", 81)):
            self.scancodes.setdefault(action, scancode)

    @staticmethod
    def _key_label(scancode: int | None) -> str:
        if scancode is None:
            return "Unassigned"
        for name, (code, label) in SCANCODE_NAMES.items():
            if code == scancode:
                return label
        return f"Scancode {scancode}"

    def _scan(self, action: str) -> None:
        self.active_action = action
        self.key_labels[action].set("Press a key…")
        if self.bind_id:
            self.dialog._unbind(("bind", "all", "<KeyPress>"), self.bind_id)
        self.bind_id = self.dialog.bind_all("<KeyPress>", self._on_key)
        self.dialog.focus_force()

    def _on_key(self, event: tk.Event) -> str:
        if not self.active_action:
            return "break"
        key = event.keysym
        if len(key) == 1 and key.isalpha():
            key = key.lower()
        if key not in SCANCODE_NAMES:
            messagebox.showinfo("Key not supported", "Choose a standard keyboard key, such as a letter, number, arrow, or function key.", parent=self.dialog)
            return "break"
        scancode, label = SCANCODE_NAMES[key]
        for other_action, other_scancode in list(self.scancodes.items()):
            if other_action != self.active_action and other_scancode == scancode:
                self.scancodes.pop(other_action)
                self.key_labels[other_action].set("Unassigned")
        self.scancodes[self.active_action] = scancode
        self.key_labels[self.active_action].set(label)
        self.active_action = None
        self.dialog._unbind(("bind", "all", "<KeyPress>"), self.bind_id)
        self.bind_id = None
        return "break"

    def _save(self) -> None:
        try:
            lines = self.config_path.read_text(encoding="utf-8").splitlines()
        except OSError:
            lines = ["# f2bgl controls configuration file", ""]
        lines = [line for line in lines if not re.match(r"\s*keyboard\.scancode\.\d+\s*=", line)]
        lines.extend(("", "# Keyboard mappings configured in the launcher"))
        for action, _label in CONTROL_ACTIONS:
            if action in self.scancodes:
                lines.append(f"keyboard.scancode.{self.scancodes[action]} = {action}")
        try:
            self.config_path.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")
        except OSError as exc:
            messagebox.showerror("Could not save controls", str(exc), parent=self.dialog)
            return
        self._close()

    def _close(self) -> None:
        if self.bind_id:
            self.dialog._unbind(("bind", "all", "<KeyPress>"), self.bind_id)
        try:
            if self.dialog.grab_current() is self.dialog:
                self.dialog.grab_release()
        except tk.TclError:
            pass
        self.dialog.destroy()


class Launcher:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("Fade To Black Launcher")
        screen_width = root.winfo_screenwidth()
        screen_height = root.winfo_screenheight()
        window_width = min(700, max(540, screen_width - 60))
        window_height = min(600, max(440, screen_height - 100))
        window_x = max(10, (screen_width - window_width) // 2)
        window_y = max(10, (screen_height - window_height - 60) // 2)
        self.root.geometry(f"{window_width}x{window_height}+{window_x}+{window_y}")
        self.root.minsize(min(520, screen_width - 20), min(420, screen_height - 80))
        self.settings = load_settings()
        for key in ("executable", "datapath", "psxpath", "savepath", "soundfont"):
            self.settings[key] = normalized_path(str(self.settings[key]))
        self.vars: dict[str, tk.StringVar | tk.BooleanVar] = {}
        self.output_tail: deque[str] = deque(maxlen=60)
        self.process: subprocess.Popen[str] | None = None
        try:
            icon_source = tk.PhotoImage(file=str(BASE_DIR / "assets" / "launcher-icon.png"))
            self.icon_images = [icon_source.subsample(scale) for scale in (4, 8, 16, 32)]
            self.root.iconphoto(True, *self.icon_images)
        except tk.TclError:
            self.icon_images = []

        for key, value in self.settings.items():
            if isinstance(value, bool):
                self.vars[key] = tk.BooleanVar(value=value)
            else:
                self.vars[key] = tk.StringVar(value=value)

        self._build()
        self.root.protocol("WM_DELETE_WINDOW", self._close)

    def _build(self) -> None:
        page = ttk.Frame(self.root, padding=18)
        page.grid(row=0, column=0, sticky="nsew")
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        page.columnconfigure(0, weight=1)

        try:
            self.banner_source = tk.PhotoImage(file=str(BASE_DIR / "assets" / "launcher-banner.png"))
            self.banner_image = self.banner_source.zoom(1).subsample(3)
            ttk.Label(page, image=self.banner_image, anchor="center").grid(
                row=0, column=0, sticky="ew", pady=(0, 12)
            )
        except tk.TclError:
            ttk.Label(page, text="FADE TO BLACK", font=("Segoe UI", 25, "bold")).grid(
                row=0, column=0, sticky="w", pady=(0, 12)
            )
        ttk.Label(
            page,
            text="Choose your settings, configure controls, then launch the game.",
            style="Muted.TLabel",
        ).grid(row=1, column=0, sticky="w", pady=(0, 14))

        actions = ttk.LabelFrame(page, text="Launcher", padding=14)
        actions.grid(row=2, column=0, sticky="ew")
        actions.columnconfigure(0, weight=1)
        ttk.Button(
            actions,
            text="Configure keyboard controls…",
            command=self._configure_controls,
            style="Accent.TButton",
        ).grid(row=0, column=0, sticky="ew", pady=(0, 8))
        ttk.Button(
            actions,
            text="Options…",
            command=self._show_options,
        ).grid(row=1, column=0, sticky="ew")

        footer = ttk.Frame(page)
        footer.grid(row=3, column=0, sticky="ew", pady=(14, 0))
        footer.columnconfigure(0, weight=1)
        self.status = tk.StringVar(value="Settings are saved automatically.")
        ttk.Label(footer, textvariable=self.status).grid(row=0, column=0, sticky="w")
        self.launch_button = ttk.Button(
            footer, text="Launch game", command=self._launch, style="Accent.TButton"
        )
        self.launch_button.grid(row=0, column=1, sticky="e")

    def _show_options(self) -> None:
        existing = getattr(self, "options_dialog", None)
        if existing is not None and existing.winfo_exists():
            existing.lift()
            existing.focus_force()
            return

        dialog = tk.Toplevel(self.root)
        self.options_dialog = dialog
        dialog.withdraw()
        dialog.overrideredirect(True)
        dialog.configure(background="#111318", highlightthickness=1, highlightbackground="#343943")
        dialog.transient(self.root)
        dialog.resizable(False, False)
        dialog.bind("<Escape>", lambda _event: self._close_options())

        screen_width, screen_height = dialog.winfo_screenwidth(), dialog.winfo_screenheight()
        width, height = min(760, screen_width - 36), min(760, screen_height - 60)
        x = max(0, self.root.winfo_rootx() + (self.root.winfo_width() - width) // 2)
        y = max(0, self.root.winfo_rooty() + (self.root.winfo_height() - height) // 2)
        x = min(x, screen_width - width)
        y = min(y, screen_height - height)
        dialog.geometry(f"{width}x{height}+{x}+{y}")

        titlebar = ttk.Frame(dialog, padding=(12, 8))
        titlebar.pack(fill="x")
        title = ttk.Label(titlebar, text="GAME OPTIONS", style="PopupTitle.TLabel")
        title.pack(side="left")
        ttk.Button(titlebar, text="×", width=3, style="Close.TButton", command=self._close_options).pack(side="right")
        for widget in (titlebar, title):
            widget.bind("<ButtonPress-1>", self._options_start_move)
            widget.bind("<B1-Motion>", self._options_move)

        ttk.Label(
            dialog,
            text="Files, gameplay, display, and rendering settings.",
            padding=(12, 4, 12, 10),
            style="Muted.TLabel",
        ).pack(anchor="w")
        area = ttk.Frame(dialog, padding=(12, 0, 12, 8))
        area.pack(fill="both", expand=True)
        canvas = tk.Canvas(area, background="#111318", highlightthickness=0)
        scrollbar = ttk.Scrollbar(area, orient="vertical", command=canvas.yview)
        canvas.configure(yscrollcommand=scrollbar.set)
        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")
        content = ttk.Frame(canvas, padding=(0, 0, 8, 0))
        content_window = canvas.create_window((0, 0), window=content, anchor="nw")
        content.bind("<Configure>", lambda _event: canvas.configure(scrollregion=canvas.bbox("all")))
        canvas.bind("<Configure>", lambda event: canvas.itemconfigure(content_window, width=event.width))
        self._options_wheel_bind = dialog.bind_all(
            "<MouseWheel>", lambda event: canvas.yview_scroll(-int(event.delta / 120), "units")
        )
        content.columnconfigure(0, weight=1)

        files = ttk.LabelFrame(content, text="Files", padding=8)
        files.grid(row=0, column=0, sticky="ew", pady=(0, 10))
        files.columnconfigure(1, weight=1)
        path_row = 0
        if not FROZEN:
            self._path_row(files, path_row, "f2bgl executable", "executable", "file")
            path_row += 1
        self._path_row(files, path_row, "Game data folder", "datapath", "directory")
        self._path_row(files, path_row + 1, "PlayStation data folder (optional)", "psxpath", "directory")
        self._path_row(files, path_row + 2, "Save files folder", "savepath", "directory")
        self._path_row(files, path_row + 3, "SoundFont (.sf2)", "soundfont", "soundfont")
        ttk.Label(
            files,
            text="PC game data is required. PlayStation data supports cutscenes and supplemental files.",
            style="Muted.TLabel",
        ).grid(row=path_row + 4, column=1, sticky="w", pady=(1, 0))

        gameplay = ttk.LabelFrame(content, text="Gameplay", padding=8)
        gameplay.grid(row=1, column=0, sticky="ew", pady=(0, 10))
        gameplay.columnconfigure(1, weight=1)
        self._combo(gameplay, 0, "Language", "language", LANGUAGES)
        self._combo(gameplay, 1, "Voice", "voice", VOICES)
        self._entry(gameplay, 2, "Start level", "level", width=10)
        self._check(gameplay, 3, "playdemo", "Play demo input files")
        self._check(gameplay, 4, "subtitles", "Display cutscene subtitles")
        self._check(gameplay, 5, "mouse", "Enable mouse controls")

        display = ttk.LabelFrame(content, text="Display and rendering", padding=8)
        display.grid(row=2, column=0, sticky="ew")
        display.columnconfigure(1, weight=1)
        self._combo(display, 0, "Window resolution", "resolution", RESOLUTIONS)
        self._entry(display, 1, "Field of view", "fov", width=10)
        ttk.Label(display, text="75–130; blank uses the engine default", style="Muted.TLabel").grid(
            row=2, column=0, columnspan=2, sticky="w", pady=(0, 5)
        )
        self._combo(display, 3, "Texture filter", "texturefilter", TEXTURE_FILTERS)
        self._combo(display, 4, "Texture scaler", "texturescaler", TEXTURE_SCALERS)
        self._combo(display, 5, "Post-processing shader", "shader", SHADERS)
        self._check(display, 6, "fullscreen", "Start fullscreen (Alt+Enter toggles)")
        self._check(display, 7, "no_fog", "Disable fog")
        self._check(display, 8, "no_gouraud", "Disable Gouraud shading")

        footer = ttk.Frame(dialog, padding=12)
        footer.pack(fill="x")
        ttk.Button(footer, text="Done", style="Accent.TButton", command=self._close_options).pack(side="right")
        dialog.protocol("WM_DELETE_WINDOW", self._close_options)
        dialog.update_idletasks()
        dialog.deiconify()
        dialog.lift(self.root)
        dialog.focus_force()
        dialog.grab_set()

    def _options_start_move(self, event: tk.Event) -> None:
        dialog = self.options_dialog
        self._options_move_x = event.x_root - dialog.winfo_x()
        self._options_move_y = event.y_root - dialog.winfo_y()

    def _options_move(self, event: tk.Event) -> None:
        self.options_dialog.geometry(f"+{event.x_root - self._options_move_x}+{event.y_root - self._options_move_y}")

    def _close_options(self) -> None:
        dialog = getattr(self, "options_dialog", None)
        if dialog is None or not dialog.winfo_exists():
            return
        try:
            save_settings(self._current_settings())
        except OSError as exc:
            messagebox.showwarning("Settings not saved", str(exc), parent=dialog)
        try:
            if dialog.grab_current() is dialog:
                dialog.grab_release()
        except tk.TclError:
            pass
        if getattr(self, "_options_wheel_bind", None):
            dialog._unbind(("bind", "all", "<MouseWheel>"), self._options_wheel_bind)
            self._options_wheel_bind = None
        dialog.destroy()
        self.options_dialog = None

    def _path_row(self, parent: ttk.LabelFrame, row: int, label: str, key: str, kind: str) -> None:
        if FROZEN and key == "executable":
            return
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky="w", padx=(0, 8), pady=2)
        ttk.Entry(parent, textvariable=self.vars[key], state="readonly" if FROZEN and key == "executable" else "normal").grid(
            row=row, column=1, sticky="ew", pady=2
        )
        ttk.Button(parent, text="Browse…", command=lambda: self._browse(key, kind), state="disabled" if FROZEN and key == "executable" else "normal").grid(
            row=row, column=2, padx=(8, 0), pady=2
        )

    def _browse(self, key: str, kind: str) -> None:
        current = str(self.vars[key].get()).strip()
        initial = Path(current).expanduser() if current else BASE_DIR
        if initial.is_file():
            initial = initial.parent
        owner = getattr(self, "options_dialog", None)
        if owner is None or not owner.winfo_exists():
            owner = self.root
        if kind == "directory":
            chosen = filedialog.askdirectory(
                parent=owner, title="Choose folder", initialdir=str(initial)
            )
        else:
            if kind == "soundfont":
                filetypes = [("SoundFont files", "*.sf2"), ("All files", "*.*")]
                title = "Choose SoundFont"
            else:
                executable_pattern = "f2bgl.exe" if os.name == "nt" else "f2bgl"
                filetypes = [("f2bgl executable", executable_pattern), ("All files", "*.*")]
                title = "Choose f2bgl executable"
            chosen = filedialog.askopenfilename(
                parent=owner, title=title, initialdir=str(initial), filetypes=filetypes
            )
        if chosen:
            self.vars[key].set(chosen)

    def _entry(self, parent: ttk.LabelFrame, row: int, label: str, key: str, width: int = 28) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky="w", padx=(0, 8), pady=2)
        ttk.Entry(parent, textvariable=self.vars[key], width=width).grid(
            row=row, column=1, sticky="ew", pady=2
        )

    def _combo(
        self, parent: ttk.LabelFrame, row: int, label: str, key: str, values: tuple[str, ...]
    ) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky="w", padx=(0, 8), pady=2)
        ttk.Combobox(
            parent, textvariable=self.vars[key], values=values, state="readonly", width=16
        ).grid(row=row, column=1, sticky="ew", pady=2)

    def _check(self, parent: ttk.LabelFrame, row: int, key: str, label: str) -> None:
        ttk.Checkbutton(parent, text=label, variable=self.vars[key]).grid(
            row=row, column=0, columnspan=2, sticky="w", pady=1
        )

    def _current_settings(self) -> dict[str, str | bool]:
        values = {key: variable.get() for key, variable in self.vars.items()}
        for key in ("executable", "datapath", "psxpath", "savepath", "soundfont"):
            values[key] = normalized_path(str(values[key]))
            self.vars[key].set(values[key])
        return values

    def _arguments(self) -> tuple[Path, Path, Path, list[str]]:
        executable_text = normalized_path(str(self.vars["executable"].get()))
        if not executable_text:
            raise ValueError("Choose the f2bgl executable.")
        executable = Path(executable_text).expanduser().resolve()
        if not executable.is_file():
            raise ValueError(f"f2bgl executable not found:\n{executable}")
        if os.name != "nt" and not os.access(executable, os.X_OK):
            raise ValueError(f"The selected file is not executable:\n{executable}")

        data_text = normalized_path(str(self.vars["datapath"].get()))
        data_path = Path(data_text).expanduser().resolve() if data_text else executable.parent
        if not data_path.is_dir():
            raise ValueError(f"Game data folder not found:\n{data_path}")

        psx_text = normalized_path(str(self.vars["psxpath"].get()))
        psx_path = Path(psx_text).expanduser().resolve() if psx_text else None
        if psx_path is not None and not psx_path.is_dir():
            raise ValueError(f"PlayStation data folder not found:\n{psx_path}")

        save_text = normalized_path(str(self.vars["savepath"].get()))
        save_path = Path(save_text).expanduser().resolve() if save_text else executable.parent
        if save_path.exists() and not save_path.is_dir():
            raise ValueError(f"Save path is not a folder:\n{save_path}")
        save_path.mkdir(parents=True, exist_ok=True)

        soundfont_text = normalized_path(str(self.vars["soundfont"].get()))
        soundfont = Path(soundfont_text).expanduser().resolve() if soundfont_text else Path()
        if soundfont_text and not soundfont.is_file():
            raise ValueError(f"SoundFont file not found:\n{soundfont}")

        args = [
            str(executable),
            f"--datapath={data_path}",
            f"--savepath={save_path}",
            f"--language={self.vars['language'].get()}",
            f"--voice={self.vars['voice'].get()}",
            f"--texturefilter={self.vars['texturefilter'].get()}",
            f"--texturescaler={self.vars['texturescaler'].get()}",
        ]
        if psx_path is not None:
            args.append(f"--psxpath={psx_path}")
        resolution = str(self.vars["resolution"].get()).strip()
        if resolution not in RESOLUTIONS:
            raise ValueError("Choose a listed window resolution.")
        args.append(f"--resolution={resolution}")
        shader = str(self.vars["shader"].get()).strip()
        if shader and shader != "none":
            args.append(f"--shader={shader}")
        level = str(self.vars["level"].get()).strip()
        if level:
            try:
                int(level)
            except ValueError as exc:
                raise ValueError("Start level must be a whole number, or left blank.") from exc
            args.append(f"--level={level}")
        fov = str(self.vars["fov"].get()).strip()
        if fov:
            try:
                fov_value = int(fov)
            except ValueError as exc:
                raise ValueError("Field of view must be a whole number from 75 to 130.") from exc
            if not 75 <= fov_value <= 130:
                raise ValueError("Field of view must be between 75 and 130 degrees.")
            args.append(f"--fov={fov_value}")
        if soundfont_text:
            args.append(f"--soundfont={soundfont}")
        for key, option in (
            ("playdemo", "--playdemo"),
            ("subtitles", "--subtitles"),
            ("fullscreen", "--fullscreen"),
            ("mouse", "--mouse"),
            ("no_fog", "--no-fog"),
            ("no_gouraud", "--no-gouraud"),
        ):
            if self.vars[key].get():
                args.append(option)
        return executable, data_path, save_path, args

    def _launch(self) -> None:
        try:
            executable, _data_path, _save_path, args = self._arguments()
        except (OSError, ValueError) as exc:
            messagebox.showerror("Cannot launch f2bgl", str(exc), parent=self.root)
            return

        self.settings = self._current_settings()
        try:
            save_settings(self.settings)
        except OSError as exc:
            messagebox.showwarning(
                "Settings not saved",
                f"The game can still launch, but settings could not be saved:\n{exc}",
                parent=self.root,
            )

        self.output_tail.clear()
        creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
        try:
            self.process = subprocess.Popen(
                args,
                cwd=str(user_data_dir() if FROZEN else executable.parent),
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
                creationflags=creationflags,
            )
        except OSError as exc:
            messagebox.showerror("Could not start f2bgl", str(exc), parent=self.root)
            return

        self.launch_button.configure(state="disabled")
        self.status.set("Game is running.")
        threading.Thread(target=self._watch_process, args=(self.process,), daemon=True).start()

    def _configure_controls(self) -> None:
        executable = normalized_path(str(self.vars["executable"].get()))
        if not executable or not Path(executable).is_file():
            messagebox.showerror(
                "Choose f2bgl first",
                "Select the f2bgl executable before configuring controls.",
                parent=self.root,
            )
            return
        ControlsDialog(self.root, controls_file(Path(executable).resolve()))

    def _watch_process(self, process: subprocess.Popen[str]) -> None:
        if process.stdout:
            for line in process.stdout:
                self.output_tail.append(line.rstrip())
        code = process.wait()
        try:
            self.root.after(0, self._process_finished, code)
        except tk.TclError:
            pass

    def _process_finished(self, code: int) -> None:
        self.launch_button.configure(state="normal")
        self.process = None
        self.status.set("Game closed." if code == 0 else f"Game exited with code {code}.")
        diagnostics = [
            line for line in self.output_tail
            if any(word in line.lower() for word in ("shader", "framebuffer", "opengl"))
        ]
        if code != 0:
            details = "\n".join(self.output_tail).strip()
            message = f"f2bgl exited with code {code}."
            if details:
                message += f"\n\nLast output:\n{details}"
            messagebox.showerror("f2bgl stopped", message, parent=self.root)
        elif diagnostics and any("disabled" in line.lower() or "failed" in line.lower() or "unavailable" in line.lower() for line in diagnostics):
            messagebox.showwarning(
                "Post-processing shader unavailable",
                "The game ran, but could not enable its post-processing shader:\n\n"
                + "\n".join(diagnostics),
                parent=self.root,
            )

    def _close(self) -> None:
        try:
            save_settings(self._current_settings())
        except OSError:
            pass
        self.root.destroy()


def _show_cli_help() -> int:
    if sys.stdout is not None:
        print(CLI_HELP, end="", flush=True)
        return 0

    # A windowed app launched from Explorer has no console to print into.
    # Keep a small, copyable help window open instead of flashing a console.
    root = tk.Tk()
    root.title("Fade To Black Launcher - command-line options")
    root.configure(background="#111318")
    root.geometry("760x610")
    root.minsize(620, 440)
    frame = ttk.Frame(root, padding=12)
    frame.pack(fill="both", expand=True)
    ttk.Label(frame, text="Command-line options", style="PopupTitle.TLabel").pack(anchor="w", pady=(0, 8))
    content = ttk.Frame(frame)
    content.pack(fill="both", expand=True)
    text = tk.Text(content, wrap="none", background="#171a20", foreground="#f1eee9",
                   insertbackground="#f1eee9", relief="flat", font=("Consolas", 10), padx=10, pady=10)
    yscroll = ttk.Scrollbar(content, orient="vertical", command=text.yview)
    xscroll = ttk.Scrollbar(frame, orient="horizontal", command=text.xview)
    text.configure(yscrollcommand=yscroll.set, xscrollcommand=xscroll.set)
    text.pack(side="left", fill="both", expand=True)
    yscroll.pack(side="right", fill="y")
    xscroll.pack(fill="x")
    text.insert("1.0", CLI_HELP)
    text.configure(state="disabled")
    ttk.Button(frame, text="Close", command=root.destroy).pack(anchor="e", pady=(8, 0))
    root.mainloop()
    return 0


def main() -> int:
    if any(arg.lower() in ("--help", "/?") for arg in sys.argv[1:]):
        return _show_cli_help()
    if FROZEN and len(sys.argv) == 1 and os.name == "nt":
        # The packaged executable uses a console subsystem so command-line
        # output reaches the invoking prompt. Hide its console for normal GUI use.
        try:
            import ctypes

            console_window = ctypes.windll.kernel32.GetConsoleWindow()
            if console_window:
                ctypes.windll.user32.ShowWindow(console_window, 0)
        except (OSError, AttributeError):
            pass
    if FROZEN and len(sys.argv) > 1:
        try:
            import ctypes

            kernel32 = ctypes.windll.kernel32
            if not kernel32.AttachConsole(-1):
                kernel32.AllocConsole()
            sys.stdout = open("CONOUT$", "w", encoding="utf-8", errors="replace")
            sys.stderr = open("CONOUT$", "w", encoding="utf-8", errors="replace")
        except (OSError, AttributeError):
            pass
        engine = Path(getattr(sys, "_MEIPASS", BASE_DIR)) / "f2bgl-engine.exe"
        user_data_dir().mkdir(parents=True, exist_ok=True)
        return subprocess.call(
            [str(engine), *sys.argv[1:]],
            cwd=str(user_data_dir()),
        )
    root = tk.Tk()
    try:
        root.configure(background="#111318")
        style = ttk.Style(root)
        style.theme_use("clam")
        style.configure("TFrame", background="#111318")
        style.configure("TLabel", background="#111318", foreground="#f1eee9")
        style.configure("Muted.TLabel", background="#111318", foreground="#aaa7a2")
        style.configure(
            "TLabelframe", background="#171a20", foreground="#f1eee9",
            bordercolor="#343943", relief="solid", borderwidth=1,
        )
        style.configure(
            "TLabelframe.Label", background="#171a20", foreground="#ff8a42",
            font=("Segoe UI", 10, "bold"),
        )
        style.configure("TEntry", fieldbackground="#22262e", foreground="#f1eee9", insertcolor="#f1eee9")
        style.configure("TCombobox", fieldbackground="#22262e", background="#292d35", foreground="#f1eee9", arrowcolor="#ff8a42")
        style.map("TCombobox", fieldbackground=[("readonly", "#22262e")], foreground=[("readonly", "#f1eee9")])
        style.configure("TCheckbutton", background="#171a20", foreground="#e7e3de", focuscolor="#171a20")
        style.map("TCheckbutton", background=[("active", "#171a20")], foreground=[("active", "#ffffff")])
        style.configure("TButton", background="#2b3039", foreground="#f1eee9", padding=(10, 7), borderwidth=0)
        style.map("TButton", background=[("active", "#3a414d"), ("disabled", "#252830")])
        style.configure("Accent.TButton", background="#e85d28", foreground="#ffffff", padding=(12, 9), font=("Segoe UI", 10, "bold"))
        style.map("Accent.TButton", background=[("active", "#fa743c"), ("disabled", "#713c2b")])
        style.configure("PopupTitle.TLabel", background="#111318", foreground="#ff8a42", font=("Segoe UI", 10, "bold"))
        style.configure("Close.TButton", background="#252830", foreground="#f1eee9", padding=(4, 1), font=("Segoe UI", 13, "bold"))
        style.map("Close.TButton", background=[("active", "#e85d28")], foreground=[("active", "#ffffff")])
    except tk.TclError:
        pass
    Launcher(root)
    root.mainloop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
