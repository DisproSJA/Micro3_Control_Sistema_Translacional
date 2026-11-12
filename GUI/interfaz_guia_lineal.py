#!/usr/bin/env python3
"""
Interfaz de Usuario — Control Sistema Lineal
Proyecto de Diseño Mecatrónica
Integrantes: Juan Andrés Sánchez, Sofía Vega, Andrés Felipe Trujillo

Habla con el PIC16F18446 a 115200 8N1.
Cada muestra llega como: consigna_mm,posicion_mm,error_mm,salida_V
Comandos: C, P, S, Z, D, A, Kp:, Ki:, Kd:
"""

import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import csv
import threading
import time
import queue
from collections import deque
import re
import ctypes


# Sin esto Windows escala la ventana y el texto queda borroso.
try:
    ctypes.windll.shcore.SetProcessDpiAwareness(1)
except Exception:
    try:
        ctypes.windll.user32.SetProcessDPIAware()
    except Exception:
        pass

try:
    import serial
    import serial.tools.list_ports
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False
    print("[AVISO] pyserial no instalado  →  pip install pyserial")

try:
    from matplotlib.figure import Figure
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
    HAS_MPL = True
except ImportError:
    HAS_MPL = False
    print("[AVISO] matplotlib no instalado →  pip install matplotlib")


BG      = "#0D1117"
SURFACE = "#161B22"
CARD    = "#161B22"
CARD2   = "#1C2128"
BORDER  = "#30363D"
ACCENT  = "#1F6FEB"
ACCENT2 = "#238636"
CYAN    = "#00D4FF"
WARN    = "#D29922"
DANGER  = "#FF6B35"
PURPLE  = "#BC8CFF"
ORANGE  = "#F0883E"
TEXT    = "#E6EDF3"
TEXTDIM = "#8B949E"
TEXTLOG = "#ADBAC7"

FF  = "Segoe UI"
FS  = 10
FSL = 13
FSH = 22

BAUD_RATE  = 115200
FS_HZ      = 150
VENTANA_S  = 10
MAX_PUNTOS = FS_HZ * 30
LEFT_W     = 400
# Escalón: ts va del 10 % al 90 %. te es la entrada a la banda del 2 %.
# La ventana se corta en 3*te para que el escalón quede quieto y con zoom.
BANDA_TE   = 0.02
VENTANA_TE = 3.0
PASO_MIN   = 1.0
POS_MIN    = -120.0
POS_MAX    = 120.0

UI_SCALE = 1.0

# Cada muestra: consigna, posicion, error (1 decimal), voltios (2) y el ADC.
TELEMETRIA_RE = re.compile(
    r"^([+-]?\d+\.\d),([+-]?\d+\.\d),([+-]?\d+\.\d),([+-]?\d+\.\d{2}),(\d+)$")
# C responde el conteo crudo y, en otra linea, A y B como los ve el PIC.
ENC_RE = re.compile(r"^ENC:(-?\d+)$")
AB_RE = re.compile(r"^AB:([01])([01])$")
NUMERO_RE = re.compile(r"^[+-]?\d+(?:\.\d+)?$")


def px(n):
    return int(round(n * UI_SCALE))


def sep_h(parent, pady=4):
    f = tk.Frame(parent, bg=BORDER, height=1)
    f.pack(fill="x", padx=0, pady=pady)
    return f


def card_frame(parent, title, title_color=ACCENT):
    wrapper = tk.Frame(parent, bg=BORDER, bd=0)
    title_bar = tk.Frame(wrapper, bg=CARD2)
    title_bar.pack(fill="x")
    tk.Label(title_bar, text=title,
             font=(FF, FS, "bold"), bg=CARD2,
             fg=title_color).pack(side="left", padx=10, pady=5)
    body = tk.Frame(wrapper, bg=CARD2)
    body.pack(fill="both", expand=True)
    return wrapper, body


class FlatButton(tk.Frame):
    """Botón de un solo color. El texto y el fondo reciben el clic."""

    def __init__(self, parent, text, command=None,
                 bg_color=ACCENT, fg_color=TEXT,
                 width=160, height=34,
                 font_size=FS, bold=True, **kwargs):
        super().__init__(parent, bg=bg_color,
                         width=px(width), height=px(height),
                         cursor="hand2")
        self.pack_propagate(False)
        self.grid_propagate(False)

        self._bg  = bg_color
        self._hbg = self._lighten(bg_color)
        self._fg  = fg_color
        self._cmd = command

        self._lbl = tk.Label(
            self, text=text, bg=bg_color, fg=fg_color,
            font=(FF, font_size, "bold" if bold else "normal"),
            cursor="hand2")
        self._lbl.pack(expand=True, fill="both")

        for w in (self, self._lbl):
            w.bind("<Enter>",    lambda _e: self._hover(True))
            w.bind("<Leave>",    lambda _e: self._hover(False))
            w.bind("<Button-1>", lambda _e: self._click())

    @staticmethod
    def _lighten(hex_color):
        h = hex_color.lstrip("#")
        r, g, b = int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)
        return "#{:02x}{:02x}{:02x}".format(
            min(255, r + 30), min(255, g + 30), min(255, b + 30))

    def _hover(self, on):
        c = self._hbg if on else self._bg
        self.config(bg=c)
        self._lbl.config(bg=c)

    def _click(self):
        if self._cmd:
            self._cmd()

    def configure_text(self, text):
        self._lbl.config(text=text)

    def set_color(self, bg_color, fg_color=None):
        self._bg  = bg_color
        self._hbg = self._lighten(bg_color)
        if fg_color:
            self._fg = fg_color
            self._lbl.config(fg=self._fg)
        self.config(bg=self._bg)
        self._lbl.config(bg=self._bg)


def entry_row(parent, label_text, var, cmd, hint_color=ACCENT, w_label=12):
    row = tk.Frame(parent, bg=CARD2)
    row.pack(fill="x", expand=True, padx=14, pady=3)

    tk.Label(row, text=label_text, font=(FF, FS),
             bg=CARD2, fg=TEXTDIM, width=w_label,
             anchor="w").pack(side="left")

    FlatButton(row, "▶", command=cmd,
               bg_color=hint_color, fg_color=BG,
               width=36, height=30, font_size=FS).pack(side="right")

    e = tk.Entry(row, textvariable=var, width=8,
                 font=(FF, FS + 1), bg=SURFACE, fg=TEXT,
                 insertbackground=ACCENT, relief="flat",
                 bd=0, highlightthickness=1,
                 highlightcolor=hint_color,
                 highlightbackground=BORDER)
    e.pack(side="left", fill="x", expand=True, padx=6, ipady=4)
    e.bind("<Return>", lambda _: cmd())
    return e


class Riel(tk.Canvas):
    """Regla de -120 a 120 mm. Naranja es la referencia, cian es el carro."""

    def __init__(self, parent):
        super().__init__(parent, bg=BG, height=px(78),
                         highlightthickness=0, bd=0)
        self._pos = None
        self._ref = None
        self.bind("<Configure>", lambda _e: self._dibujar())

    def set_valores(self, pos, ref):
        self._pos = pos
        self._ref = ref
        self._dibujar()

    def _x(self, mm, x0, ancho):
        t = (mm - POS_MIN) / (POS_MAX - POS_MIN)
        return x0 + t * ancho

    def _dibujar(self):
        self.delete("all")
        w = self.winfo_width()
        h = self.winfo_height()
        if w < 40 or h < 20:
            return

        x0 = 28
        x1 = w - 18
        ancho = max(10, x1 - x0)
        y = 28

        self.create_line(x0, y, x1, y, fill=BORDER, width=3)
        for mm, txt in ((-120, "-120"), (-60, "-60"), (0, "0"),
                        (60, "60"), (120, "120")):
            x = self._x(mm, x0, ancho)
            self.create_line(x, y - 6, x, y + 6, fill=TEXTDIM)
            self.create_text(x, y + 16, text=txt, fill=TEXTDIM,
                             font=(FF, 8))

        if self._ref is not None:
            x = self._x(max(POS_MIN, min(POS_MAX, self._ref)), x0, ancho)
            self.create_line(x, 8, x, y + 4, fill=ORANGE, width=2)
            self.create_polygon(x - 5, 8, x + 5, 8, x, 16,
                                fill=ORANGE, outline=ORANGE)

        if self._pos is not None:
            x = self._x(max(POS_MIN, min(POS_MAX, self._pos)), x0, ancho)
            self.create_rectangle(x - 9, y - 11, x + 9, y + 7,
                                  fill=CYAN, outline=CYAN)
            self.create_text(w / 2, h - 8,
                             text=f"{self._pos:.1f} mm",
                             fill=CYAN, font=(FF, FS, "bold"))


class SerialWorker(threading.Thread):
    """Lee el puerto en otro hilo. A 150 Hz no se puede hacer desde la ventana."""
    def __init__(self, port, baud, rx_queue):
        super().__init__(daemon=True)
        self.port      = port
        self.baud      = baud
        self.rx_queue  = rx_queue
        self._tx_queue = queue.Queue()
        self._ser      = None
        self._running  = False

    def run(self):
        try:
            self._ser = serial.Serial(self.port, self.baud, timeout=0.05)
            self._running = True
            self.rx_queue.put(("CONNECTED", None))
            buf = ""
            while self._running:
                try:
                    msg = self._tx_queue.get_nowait()
                    self._ser.write((msg + "\n").encode("ascii"))
                except queue.Empty:
                    pass
                n = self._ser.in_waiting
                raw = self._ser.read(n if n else 1)
                if not raw:
                    continue
                buf += raw.decode("ascii", errors="replace")
                while "\n" in buf:
                    line, buf = buf.split("\n", 1)
                    line = line.strip().replace("\r", "")
                    if line:
                        self.rx_queue.put(("MSG", line))
        except Exception as exc:
            self.rx_queue.put(("ERROR", str(exc)))
        finally:
            if self._ser and self._ser.is_open:
                self._ser.close()
            self._running = False

    def send(self, texto):
        self._tx_queue.put(texto)

    def stop(self):
        self._running = False


class SplashFrame(tk.Frame):
    BAR_W = 360

    def __init__(self, parent, on_done):
        super().__init__(parent, bg=BG)
        self._on_done  = on_done
        self._progress = 0
        self.rowconfigure(0, weight=1)
        self.columnconfigure(0, weight=1)
        self._build()
        self._animate()

    def _build(self):
        outer = tk.Frame(self, bg=BORDER)
        outer.grid(row=0, column=0)
        inner = tk.Frame(outer, bg=CARD2)
        inner.pack(padx=1, pady=1)
        pad = dict(padx=60)

        tk.Frame(inner, bg=CARD2, height=40).pack()
        tk.Label(inner, text="↔", font=(FF, 48),
                 bg=CARD2, fg=ACCENT).pack(**pad)
        tk.Label(inner, text="Control Sistema Lineal",
                 font=(FF, FSH, "bold"), bg=CARD2,
                 fg=TEXT).pack(pady=(8, 0), **pad)
        tk.Label(inner, text="G U Í A   L I N E A L",
                 font=(FF, FSL, "bold"), bg=CARD2,
                 fg=ACCENT).pack(**pad)

        tk.Frame(inner, bg=BORDER, height=1, width=px(380)).pack(pady=22)
        tk.Label(inner, text="Proyecto de Diseño Mecatrónica",
                 font=(FF, FS + 1, "italic"), bg=CARD2,
                 fg=TEXTDIM).pack()
        tk.Frame(inner, bg=CARD2, height=14).pack()
        tk.Label(inner, text="Integrantes",
                 font=(FF, FS, "bold"), bg=CARD2,
                 fg=TEXTDIM).pack()
        for nombre in ("Juan Andrés Sánchez",
                       "Sofía Vega",
                       "Andrés Felipe Trujillo"):
            tk.Label(inner, text=f"  •  {nombre}",
                     font=(FF, FSL), bg=CARD2, fg=TEXT).pack(pady=2)

        tk.Frame(inner, bg=CARD2, height=28).pack()
        bar_outer = tk.Frame(inner, bg=BORDER, height=3, width=px(self.BAR_W))
        bar_outer.pack()
        bar_outer.pack_propagate(False)
        self._bar = tk.Frame(bar_outer, bg=ACCENT, height=3, width=0)
        self._bar.place(x=0, y=0, height=3)

        self._lbl_cargando = tk.Label(
            inner, text="Iniciando...",
            font=(FF, FS - 1), bg=CARD2, fg=TEXTDIM)
        self._lbl_cargando.pack(pady=6)

        self._btn_cont = FlatButton(
            inner, "  Continuar  →",
            command=self._on_done,
            bg_color=ACCENT, fg_color=TEXT,
            width=280, height=56, font_size=FSL + 1)
        self._bottom_space = tk.Frame(inner, bg=CARD2, height=px(36))
        self._bottom_space.pack()

    def _animate(self):
        if self._progress > 100:
            self._lbl_cargando.config(text="Listo")
            self._btn_cont.pack(pady=(10, 0), before=self._bottom_space)
            return
        self._progress += 2
        self._bar.place(x=0, y=0, height=3,
                        width=int(px(self.BAR_W) * self._progress / 100))
        msgs = {0: "Iniciando...", 30: "Cargando módulos...",
                60: "Buscando puertos COM...", 85: "Preparando interfaz..."}
        for k, v in msgs.items():
            if self._progress >= k:
                self._lbl_cargando.config(text=v)
        self.after(25, self._animate)


class MainFrame(tk.Frame):

    def __init__(self, parent):
        super().__init__(parent, bg=BG)
        self._worker   = None
        self._rx_q     = queue.Queue()
        self._t        = deque(maxlen=MAX_PUNTOS)
        self._y_ref    = deque(maxlen=MAX_PUNTOS)
        self._y_pos    = deque(maxlen=MAX_PUNTOS)
        self._t0       = None
        self._muestra  = None
        self._graficar = False
        self._buscando_home = False
        self._en_fallo = False
        self._paso_t = []
        self._paso_y = []
        self._paso_t0 = None
        self._paso_y0 = 0.0
        self._paso_yf = 0.0
        self._paso_activo = False
        self._paso_fijo = False
        self._paso_dibujar = False
        self._marca_ts = None
        self._marca_te = None
        self._ts_s = None
        self._mp_pct = None
        # Ganancias con las que arranca el PIC. Se actualizan al enviarlas.
        self._kp = 1.5
        self._ki = 0.0
        self._kd = 0.07
        self._pruebas = []
        self._ventana_pruebas = None
        self._arbol_pruebas = None
        self._lbls_prom = []

        self._build()
        self._refresh_ports()
        self._poll()

    def _build(self):
        self._build_header()
        body = tk.Frame(self, bg=BG)
        body.pack(fill="both", expand=True, padx=10, pady=(6, 10))
        body.columnconfigure(0, weight=0, minsize=px(LEFT_W))
        body.columnconfigure(1, weight=1)
        body.rowconfigure(0, weight=1)
        self._build_left(body)
        self._build_right(body)

    def _build_header(self):
        hdr = tk.Frame(self, bg=SURFACE, height=px(52))
        hdr.pack(fill="x")
        hdr.pack_propagate(False)

        tk.Label(hdr, text="↔   CONTROL GUÍA LINEAL",
                 font=(FF, FSL, "bold"),
                 bg=SURFACE, fg=ACCENT).pack(side="left", padx=16)
        FlatButton(hdr, "PRUEBAS", command=self._abrir_pruebas,
                   bg_color=BORDER, fg_color=TEXT,
                   width=110, height=30).pack(side="left", pady=11)

        self._lbl_status = tk.Label(
            hdr, text="●  DESCONECTADO",
            font=(FF, FS, "bold"), bg=SURFACE, fg=DANGER)
        self._lbl_status.pack(side="right", padx=16)

        tk.Label(hdr, text="115200 8N1",
                 font=(FF, FS - 1), bg=SURFACE,
                 fg=TEXTDIM).pack(side="right", padx=4)

        self._btn_conn = FlatButton(
            hdr, "CONECTAR", command=self._toggle_connect,
            bg_color=ACCENT2, fg_color=TEXT,
            width=120, height=30)
        self._btn_conn.pack(side="right", padx=6, pady=11)

        self._port_var = tk.StringVar()
        self._port_cb = ttk.Combobox(
            hdr, textvariable=self._port_var,
            width=10, state="readonly", font=(FF, FS))
        self._port_cb.pack(side="right", padx=4)

        tk.Label(hdr, text="Puerto:", font=(FF, FS),
                 bg=SURFACE, fg=TEXTDIM).pack(side="right", padx=(8, 2))

        FlatButton(hdr, "↺", command=self._refresh_ports,
                   bg_color=BORDER, fg_color=TEXT,
                   width=28, height=28, bold=False).pack(
                       side="right", padx=0, pady=11)

    def _build_left(self, parent):
        left = tk.Frame(parent, bg=BG, width=px(LEFT_W))
        left.grid(row=0, column=0, sticky="nsew", padx=(0, 10))
        left.grid_propagate(False)
        left.columnconfigure(0, weight=1)
        left.rowconfigure(0, weight=5)
        left.rowconfigure(1, weight=3)
        left.rowconfigure(2, weight=3, minsize=px(160))
        self._card_control(left)
        self._card_lectura(left)
        self._build_log(left)

    def _big_button(self, parent, text, cmd, color):
        FlatButton(parent, text, command=cmd,
                   bg_color=color, fg_color=TEXT,
                   width=1, height=36, font_size=FS).pack(
                       fill="x", expand=True, padx=14, pady=3)

    def _card_control(self, parent):
        wrap, card = card_frame(parent, "Control", ACCENT)
        wrap.grid(row=0, column=0, sticky="nsew", pady=(0, 8))

        self._big_button(card, "LEER ENCODER   (C)",
                         lambda: self._enviar_simple("C", None), BORDER)
        self._big_button(card, "IR AL HOME   (Z)",
                         self._send_z, ACCENT)
        self._big_button(card, "PARAR   (S)",
                         self._parar, DANGER)

        fila = tk.Frame(card, bg=CARD2)
        fila.pack(fill="x", padx=14, pady=3)
        fila.columnconfigure(0, weight=1, uniform="m")
        fila.columnconfigure(1, weight=1, uniform="m")
        pot = tk.Frame(fila, bg=SURFACE, height=px(34))
        pot.grid(row=0, column=0, sticky="nsew", padx=(0, 4))
        pot.pack_propagate(False)
        tk.Label(pot, text="ADC", font=(FF, FS - 2, "bold"),
                 bg=SURFACE, fg=TEXTDIM).pack(side="left", padx=(8, 4))
        self._lbl_pot_adc = tk.Label(
            pot, text="—", font=(FF, FS, "bold"), bg=SURFACE, fg=WARN)
        self._lbl_pot_adc.pack(side="left")
        self._lbl_pot_mm = tk.Label(
            pot, text="—", font=(FF, FS, "bold"), bg=SURFACE, fg=ORANGE)
        self._lbl_pot_mm.pack(side="right", padx=(4, 8))
        FlatButton(fila, "ANALÓGICA  (A)",
                   command=self._send_analog,
                   bg_color=BORDER, fg_color=TEXT,
                   width=1, height=34, font_size=FS).grid(
                       row=0, column=1, sticky="ew", padx=(4, 0))

        sep_h(card, pady=6)

        self._pos_var = tk.StringVar(value="0.0")
        self._kp_var  = tk.StringVar(value="1.5")
        self._ki_var  = tk.StringVar(value="0")
        self._kd_var  = tk.StringVar(value="0.07")

        entry_row(card, "Pos (mm):", self._pos_var, self._send_pos)
        entry_row(card, "Kp:", self._kp_var,
                  lambda: self._send_ganancia("Kp", self._kp_var, 20.0),
                  hint_color=PURPLE)
        entry_row(card, "Ki:", self._ki_var,
                  lambda: self._send_ganancia("Ki", self._ki_var, 20.0),
                  hint_color=PURPLE)
        entry_row(card, "Kd:", self._kd_var,
                  lambda: self._send_ganancia("Kd", self._kd_var, 2.0),
                  hint_color=PURPLE)

        self._lim_min_var = tk.StringVar(value="-120.0")
        self._lim_max_var = tk.StringVar(value="120.0")
        self._lim_min = -120.0
        self._lim_max = 120.0
        entry_row(card, "Mín (mm):", self._lim_min_var, self._send_limites,
                  hint_color=WARN)
        entry_row(card, "Máx (mm):", self._lim_max_var, self._send_limites,
                  hint_color=WARN)

        self._reset_var = tk.BooleanVar(value=True)
        tk.Checkbutton(
            card, text="La gráfica arranca en 0 al mandar P",
            variable=self._reset_var,
            bg=CARD2, fg=TEXTDIM, selectcolor=SURFACE,
            activebackground=CARD2, activeforeground=TEXT,
            font=(FF, FS - 1), anchor="w"
        ).pack(fill="x", padx=12, pady=(2, 8))

    def _card_lectura(self, parent):
        wrap, card = card_frame(parent, "Lectura", CYAN)
        wrap.grid(row=1, column=0, sticky="nsew", pady=(0, 8))

        card.columnconfigure(0, weight=1)
        card.columnconfigure(1, weight=1)
        card.rowconfigure(0, weight=1)
        card.rowconfigure(1, weight=1)

        celdas = (
            (0, 0, "POSICIÓN",  "_lbl_pos", CYAN),
            (0, 1, "REFERENCIA", "_lbl_ref", ORANGE),
            (1, 0, "ERROR",     "_lbl_err", PURPLE),
            (1, 1, "SALIDA",    "_lbl_u",   ACCENT2),
        )
        for fila, col, titulo, attr, color in celdas:
            cell = tk.Frame(card, bg=CARD2)
            cell.grid(row=fila, column=col, sticky="nsew", pady=2)
            tk.Label(cell, text=titulo, font=(FF, FS - 2, "bold"),
                     bg=CARD2, fg=TEXTDIM).pack(expand=True, anchor="s")
            lbl = tk.Label(cell, text="—", font=(FF, FSL, "bold"),
                           bg=CARD2, fg=color)
            lbl.pack(expand=True, anchor="n")
            setattr(self, attr, lbl)

        fila = tk.Frame(card, bg=CARD2)
        fila.grid(row=2, column=0, columnspan=2, sticky="nsew", pady=(4, 0))
        for col in range(3):
            fila.columnconfigure(col, weight=1)
        for col, titulo, attr, color in (
            (0, "ts", "_lbl_ts", WARN),
            (1, "te", "_lbl_te", TEXT),
            (2, "Mp", "_lbl_mp", DANGER),
        ):
            cell = tk.Frame(fila, bg=CARD2)
            cell.grid(row=0, column=col, sticky="nsew")
            tk.Label(cell, text=titulo, font=(FF, FS - 2, "bold"),
                     bg=CARD2, fg=TEXTDIM).pack(anchor="s")
            lbl = tk.Label(cell, text="—", font=(FF, FS, "bold"),
                           bg=CARD2, fg=color)
            lbl.pack(anchor="n")
            setattr(self, attr, lbl)

        self._lbl_modo = tk.Label(
            card, text="Modo: en el home",
            font=(FF, FS - 1), bg=CARD2, fg=TEXTDIM)
        self._lbl_modo.grid(row=3, column=0, columnspan=2, pady=(4, 0))
        self._lbl_enc = tk.Label(
            card, text="Encoder: —",
            font=(FF, FS, "bold"), bg=CARD2, fg=ORANGE)
        self._lbl_enc.grid(row=4, column=0, columnspan=2, pady=(0, 6))

    def _build_right(self, parent):
        right = tk.Frame(parent, bg=BG)
        right.grid(row=0, column=1, sticky="nsew")
        right.rowconfigure(0, weight=3)
        right.rowconfigure(1, weight=3)
        right.columnconfigure(0, weight=1)
        self._build_graph(right)
        self._build_escalon(right)

    def _build_graph(self, parent):
        wrap, body = card_frame(parent, "Posición en el tiempo", CYAN)
        wrap.grid(row=0, column=0, sticky="nsew", pady=(0, 6))

        barra = tk.Frame(body, bg=CARD2)
        barra.pack(fill="x", padx=8, pady=(4, 0))
        tk.Label(barra,
                 text="Naranja: referencia     Cian: posición medida",
                 font=(FF, FS - 1), bg=CARD2, fg=TEXTDIM).pack(side="left")
        FlatButton(barra, "Limpiar", command=self._limpiar_grafica,
                   bg_color=BORDER, fg_color=TEXT,
                   width=80, height=26, font_size=FS - 1).pack(side="right")

        self._riel = Riel(body)
        self._riel.pack(side="bottom", fill="x", padx=6, pady=(0, 4))

        if not HAS_MPL:
            tk.Label(body, text="matplotlib no instalado\npip install matplotlib",
                     bg=CARD2, fg=DANGER, font=(FF, FS)).pack(expand=True)
            return

        self._fig = Figure(figsize=(6, 3.2), dpi=96, facecolor=BG)
        self._ax = self._fig.add_subplot(111)
        self._ax.set_facecolor(BG)
        self._ax.tick_params(colors=TEXTDIM, labelsize=8)
        for sp in self._ax.spines.values():
            sp.set_color(BORDER)
        self._ax.set_xlabel("Tiempo (s)", color=TEXTDIM, fontsize=8)
        self._ax.set_ylabel("Posición (mm)", color=TEXTDIM, fontsize=8)
        self._ax.set_xlim(0, VENTANA_S)
        self._ax.set_ylim(-130, 130)
        self._ax.grid(True, color=BORDER, linewidth=0.6)
        self._line_ref, = self._ax.plot(
            [], [], color=ORANGE, linewidth=1.6, linestyle="--",
            label="Referencia")
        self._line_pos, = self._ax.plot(
            [], [], color=CYAN, linewidth=1.8, label="Posición")
        leg = self._ax.legend(loc="upper right", fontsize=8, frameon=False)
        for txt in leg.get_texts():
            txt.set_color(TEXTDIM)
        self._fig.tight_layout(pad=0.8)

        self._mpl = FigureCanvasTkAgg(self._fig, master=body)
        self._mpl.get_tk_widget().pack(fill="both", expand=True,
                                       padx=4, pady=4)

    def _build_escalon(self, parent):
        wrap, body = card_frame(parent, "Escalón", WARN)
        wrap.grid(row=1, column=0, sticky="nsew")

        tk.Label(body,
                 text="ts: 10 % a 90 %      te: se queda dentro del 2 %      Mp: sobrepaso",
                 font=(FF, FS - 1), bg=CARD2, fg=TEXTDIM).pack(
                     anchor="w", padx=10, pady=(4, 0))

        if not HAS_MPL:
            tk.Label(body, text="matplotlib no instalado",
                     bg=CARD2, fg=DANGER, font=(FF, FS)).pack(expand=True)
            self._ax2 = None
            return

        self._fig2 = Figure(figsize=(6, 2.6), dpi=96, facecolor=BG)
        self._ax2 = self._fig2.add_subplot(111)
        self._mpl2 = FigureCanvasTkAgg(self._fig2, master=body)
        self._mpl2.get_tk_widget().pack(fill="both", expand=True,
                                        padx=4, pady=4)
        self._pintar_escalon()

    def _build_log(self, parent):
        wrap, body = card_frame(parent, "Terminal", ACCENT)
        wrap.grid(row=2, column=0, sticky="nsew")

        inner = tk.Frame(body, bg=CARD2)
        inner.pack(fill="both", expand=True, padx=4, pady=(2, 6))

        self._log = tk.Text(
            inner, bg=BG, fg=TEXTLOG,
            font=("Consolas", 9), relief="flat",
            height=9, wrap="word", state="disabled",
            selectbackground=ACCENT, insertbackground=ACCENT)
        sb = tk.Scrollbar(inner, orient="vertical",
                          command=self._log.yview,
                          bg=SURFACE, troughcolor=BG,
                          activebackground=ACCENT)
        self._log.config(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y")
        self._log.pack(side="left", fill="both", expand=True)

        self._log.tag_config("ts",    foreground="#455a64")
        self._log.tag_config("ok",    foreground=ACCENT2)
        self._log.tag_config("err",   foreground=DANGER)
        self._log.tag_config("send",  foreground=PURPLE)
        self._log.tag_config("sys",   foreground=TEXTDIM)
        self._log.tag_config("plain", foreground=TEXTLOG)
        self._log.tag_config("adc",   foreground=ORANGE)

    def _refresh_ports(self):
        if HAS_SERIAL:
            ports = [p.device for p in serial.tools.list_ports.comports()]
        else:
            ports = []
        self._port_cb["values"] = ports
        if ports:
            self._port_cb.set(ports[-1])

    def _toggle_connect(self):
        if self._worker and self._worker._running:
            self._disconnect()
        else:
            self._connect()

    def _connect(self):
        port = self._port_var.get()
        if not port:
            messagebox.showerror("Sin puerto", "Selecciona un puerto COM.")
            return
        if not HAS_SERIAL:
            messagebox.showerror("pyserial faltante", "pip install pyserial")
            return
        self._rx_q = queue.Queue()
        self._worker = SerialWorker(port, BAUD_RATE, self._rx_q)
        self._t0 = time.time()
        self._worker.start()

    def _disconnect(self):
        if self._worker:
            self._worker.stop()
            self._worker = None
        self._lbl_status.config(text="●  DESCONECTADO", fg=DANGER)
        self._btn_conn.configure_text("CONECTAR")
        self._btn_conn.set_color(ACCENT2, TEXT)
        self._log_add("sys", "Puerto cerrado")

    def _send(self, cmd):
        if self._worker and self._worker._running:
            self._worker.send(cmd)
            self._log_add("send", f"→  {cmd}")
            return True
        self._log_add("err", "No hay puerto abierto")
        return False

    def _enviar_simple(self, cmd, modo):
        if self._send(cmd) and modo:
            self._lbl_modo.config(text=f"Modo: {modo}")

    def _parar(self):
        self._enviar_simple("S", "Parado")

    def _send_analog(self):
        self._paso_t = []
        self._paso_y = []
        self._paso_activo = False
        self._paso_fijo = False
        self._paso_dibujar = True
        self._marca_ts = None
        self._marca_te = None
        self._ts_s = None
        self._mp_pct = None
        self._lbl_ts.config(text="—")
        self._lbl_te.config(text="—")
        self._lbl_mp.config(text="—")
        self._enviar_simple("A", "analógica, tope 8 V")

    def _send_z(self):
        if self._reset_var.get():
            self._limpiar_grafica()
        self._nuevo_escalon(0.0)
        self._enviar_simple("Z", "home, tope 24 V")

    def _leer_numero(self, texto):
        t = texto.strip().replace(",", ".")
        if not NUMERO_RE.fullmatch(t):
            return None
        return t, float(t)

    def _send_pos(self):
        parsed = self._leer_numero(self._pos_var.get())
        if parsed is None:
            self._log_add("err", "La posición tiene que ser un número")
            return
        _, valor = parsed
        if valor < POS_MIN or valor > POS_MAX:
            self._log_add("err", "La posición va de -120.0 a 120.0 mm")
            return
        if valor < self._lim_min or valor > self._lim_max:
            self._log_add("err", "Esa posición está fuera de los finales de carrera")
            return
        if self._reset_var.get():
            self._limpiar_grafica()
        self._nuevo_escalon(valor)
        if self._send(f"P{valor:.1f}"):
            self._lbl_modo.config(text="Modo: digital, tope 24 V")

    def _send_limites(self):
        a = self._leer_numero(self._lim_min_var.get())
        b = self._leer_numero(self._lim_max_var.get())
        if a is None or b is None:
            self._log_add("err", "Los finales de carrera tienen que ser números")
            return
        lo = a[1]
        hi = b[1]
        if lo < POS_MIN or hi > POS_MAX or lo >= hi:
            self._log_add("err", "El mínimo va de -120 hasta antes del máximo, y el máximo hasta 120")
            return
        if self._send(f"F{lo:.1f},{hi:.1f}"):
            self._lim_min = lo
            self._lim_max = hi
            self._log_add("sys", f"Finales de carrera {lo:.1f} a {hi:.1f} mm")

    def _send_ganancia(self, nombre, var, tope):
        parsed = self._leer_numero(var.get())
        if parsed is None:
            self._log_add("err", f"{nombre} tiene que ser un número")
            return
        texto, valor = parsed
        if valor < 0 or valor > tope:
            self._log_add("err", f"{nombre} va de 0 a {tope:g}")
            return
        if self._send(f"{nombre}:{texto}"):
            if nombre == "Kp":
                self._kp = valor
            elif nombre == "Ki":
                self._ki = valor
            else:
                self._kd = valor

    def _limpiar_grafica(self):
        self._t.clear()
        self._y_ref.clear()
        self._y_pos.clear()
        self._t0 = time.time()
        self._graficar = True
        self._pintar()

    def _poll(self):
        # Se vacía la cola y se pinta una vez. Si se dibuja cada muestra, la ventana se traba.
        try:
            while True:
                kind, data = self._rx_q.get_nowait()
                if kind == "CONNECTED":
                    self._lbl_status.config(text="●  CONECTADO", fg=ACCENT2)
                    self._btn_conn.configure_text("DESCONECTAR")
                    self._btn_conn.set_color(DANGER, TEXT)
                    self._log_add("sys", "Puerto abierto, 115200 8N1")
                elif kind == "MSG":
                    self._handle_msg(data)
                elif kind == "ERROR":
                    self._lbl_status.config(text="●  ERROR", fg=DANGER)
                    self._btn_conn.configure_text("CONECTAR")
                    self._btn_conn.set_color(ACCENT2, TEXT)
                    self._worker = None
                    self._log_add("err", data)
        except queue.Empty:
            pass

        if self._graficar:
            self._aplicar_lectura()
            self._pintar()
            self._graficar = False
        if self._paso_dibujar:
            self._evaluar_escalon()
            self._pintar_escalon()
            self._paso_dibujar = False
        self.after(50, self._poll)

    def _handle_msg(self, msg):
        m = TELEMETRIA_RE.match(msg)
        if m:
            if self._t0 is None:
                self._t0 = time.time()
            ref = float(m.group(1))
            pos = float(m.group(2))
            err = float(m.group(3))
            vol = float(m.group(4))
            self._t.append(time.time() - self._t0)
            self._y_ref.append(ref)
            self._y_pos.append(pos)
            self._muestra = (ref, pos, err, vol)
            self._graficar = True
            adc = int(m.group(5))
            mm_pot = (adc * 36000 // 4095 - 18000) / 150.0
            self._lbl_pot_adc.config(text=str(adc))
            self._lbl_pot_mm.config(text=f"{mm_pot:+.1f} mm")
            if self._paso_activo and not self._paso_fijo and self._paso_t0 is not None:
                self._paso_t.append(time.time() - self._paso_t0)
                self._paso_y.append(pos)
                self._paso_dibujar = True
            return

        m = ENC_RE.match(msg)
        if m:
            self._lbl_enc.config(text=f"Encoder: {m.group(1)}")
            self._log_add("adc", f"ENC {m.group(1)}")
            return
        m = AB_RE.match(msg)
        if m:
            self._log_add("adc", f"A={m.group(1)}  B={m.group(2)}")
            return
        if msg == "RDY":
            self._log_add("sys", "PIC listo")
            return
        if msg == "OK HOME":
            self._buscando_home = False
            self._lbl_modo.config(text="Modo: en el home, tope 24 V")
            self._en_fallo = False
            self._log_add("ok", "Yendo a la posición de encendido")
            return
        if msg == "ERR HOME":
            self._lbl_modo.config(text="Modo: sin home")
            self._log_add("err", "El home se fija al encender")
            self._buscando_home = False
            return
        if msg == "ERR RANGO":
            self._log_add("err", "Fuera de rango")
            return
        if msg == "ERR FALLO":
            self._buscando_home = False
            self._en_fallo = True
            self._lbl_modo.config(text="Modo: falla, motor frenado")
            self._log_add("err", "Protección: el encoder saltó o el motor no se movió. "
                                 "Pulsa PARAR para soltar el freno")
            return
        if msg == "ERR PARADO":
            self._buscando_home = False
            self._log_add("err", "Primero pulsa PARAR y deja el carro quieto")
            return
        if msg == "ERR QUIETO":
            self._buscando_home = False
            self._log_add("err", "El encoder se está moviendo. Espera y repite Z")
            return
        if msg == "ERR VALOR":
            self._log_add("err", "El PIC no entendió el comando")
            return
        if msg == "OK":
            self._en_fallo = False
            self._log_add("ok", "OK")
            return
        if msg.startswith("ERR"):
            self._log_add("err", msg)
            return
        self._log_add("plain", msg)

    def _aplicar_lectura(self):
        if not self._muestra:
            return
        ref, pos, err, vol = self._muestra
        self._lbl_pos.config(text=f"{pos:.1f}")
        self._lbl_ref.config(text=f"{ref:.1f}")
        self._lbl_err.config(text=f"{err:.1f}")
        self._lbl_u.config(text=f"{vol:.2f} V")
        self._riel.set_valores(pos, ref)

    def _pintar(self):
        if not HAS_MPL:
            return
        xs = list(self._t)
        if not xs:
            self._line_ref.set_data([], [])
            self._line_pos.set_data([], [])
            self._ax.set_xlim(0, VENTANA_S)
            self._mpl.draw_idle()
            return

        self._line_ref.set_data(xs, list(self._y_ref))
        self._line_pos.set_data(xs, list(self._y_pos))

        x_max = xs[-1]
        # Los primeros 10 s se quedan fijos para que el escalón no se corra.
        if x_max <= VENTANA_S:
            self._ax.set_xlim(0, VENTANA_S)
        else:
            self._ax.set_xlim(x_max - VENTANA_S, x_max + 0.2)

        ys = list(self._y_ref) + list(self._y_pos)
        y0 = min(ys)
        y1 = max(ys)
        if y1 - y0 < 10.0:
            medio = 0.5 * (y0 + y1)
            y0, y1 = medio - 5.0, medio + 5.0
        else:
            holgura = 0.12 * (y1 - y0)
            y0 -= holgura
            y1 += holgura
        self._ax.set_ylim(y0, y1)
        self._mpl.draw_idle()

    def _nuevo_escalon(self, yf):
        # Cada P borra el escalón anterior. y0 es donde está el carro ahora.
        if self._muestra:
            y0 = self._muestra[1]
        else:
            y0 = 0.0
        self._paso_y0 = y0
        self._paso_yf = yf
        self._paso_t = [0.0]
        self._paso_y = [y0]
        self._paso_t0 = time.time()
        self._paso_activo = abs(yf - y0) >= PASO_MIN
        self._paso_fijo = False
        self._paso_dibujar = True
        self._marca_ts = None
        self._marca_te = None
        self._ts_s = None
        self._mp_pct = None
        self._lbl_ts.config(text="—")
        self._lbl_te.config(text="—")
        self._lbl_mp.config(text="—")

    def _cruce(self, nivel, sube, ultimo=False):
        ys = self._paso_y
        ts = self._paso_t
        hallado = None
        for i in range(1, len(ys)):
            a, b = ys[i - 1], ys[i]
            if b == a:
                continue
            if sube and a < nivel <= b:
                pasa = True
            elif (not sube) and a > nivel >= b:
                pasa = True
            else:
                pasa = False
            if pasa:
                frac = (nivel - a) / (b - a)
                hallado = ts[i - 1] + frac * (ts[i] - ts[i - 1])
                if not ultimo:
                    return hallado
        return hallado

    def _evaluar_escalon(self):
        if not self._paso_activo or len(self._paso_y) < 2:
            return
        y0 = self._paso_y0
        yf = self._paso_yf
        amp = yf - y0
        if abs(amp) < PASO_MIN:
            return
        sube = amp > 0
        banda = BANDA_TE * abs(amp)

        t10 = self._cruce(y0 + 0.10 * amp, sube)
        t90 = self._cruce(y0 + 0.90 * amp, sube)
        if t10 is not None and t90 is not None and t90 >= t10:
            # La línea va en el 90 %: ahí ya se obtuvo ts y la señal está cerca.
            self._marca_ts = t90
            self._ts_s = t90 - t10
            self._lbl_ts.config(text=f"{self._ts_s:.3f} s")
            pico = yf - min(self._paso_y) if not sube else max(self._paso_y) - yf
            self._mp_pct = max(0.0, pico) / abs(amp) * 100.0
            self._lbl_mp.config(text=f"{self._mp_pct:.1f} %")

        # te es el último cruce del borde del 2 %. Si la cola sigue afuera, no hay te.
        if abs(self._paso_y[-1] - yf) > banda:
            self._marca_te = None
            self._lbl_te.config(text="—")
            return
        borde = (yf - banda) if sube else (yf + banda)
        te = self._cruce(borde, sube, ultimo=True)
        # El 2 % está después del 90 %. Si el cruce queda antes, todavía no llegó.
        if te is None or (t90 is not None and te < t90):
            self._marca_te = None
            self._lbl_te.config(text="—")
            return
        self._marca_te = te
        self._lbl_te.config(text=f"{te:.3f} s")
        if self._paso_t[-1] >= VENTANA_TE * te:
            self._paso_fijo = True
            self._paso_activo = False
            self._anotar_prueba()

    def _estilo_escalon(self):
        ax = self._ax2
        ax.set_facecolor(BG)
        ax.tick_params(colors=TEXTDIM, labelsize=8)
        for sp in ax.spines.values():
            sp.set_color(BORDER)
        ax.set_xlabel("Tiempo (s)", color=TEXTDIM, fontsize=8)
        ax.set_ylabel("Posición (mm)", color=TEXTDIM, fontsize=8)
        ax.grid(True, color=BORDER, linewidth=0.6)

    def _pintar_escalon(self):
        if not HAS_MPL or self._ax2 is None:
            return
        ax = self._ax2
        ax.cla()
        self._estilo_escalon()
        if len(self._paso_t) < 2:
            ax.text(0.5, 0.5, "El escalón aparece al mandar P",
                    transform=ax.transAxes, ha="center", va="center",
                    color=TEXTDIM, fontsize=9)
            ax.set_xticks([])
            ax.set_yticks([])
            self._fig2.tight_layout(pad=0.6)
            self._mpl2.draw_idle()
            return

        y0 = self._paso_y0
        yf = self._paso_yf
        amp = yf - y0
        banda = BANDA_TE * abs(amp)
        if self._marca_te is not None:
            t_fin = VENTANA_TE * self._marca_te
        else:
            t_fin = max(self._paso_t[-1], 0.4)
        t_pos = list(self._paso_t)
        y_pos = list(self._paso_y)
        if t_fin > t_pos[-1]:
            t_pos.append(t_fin)
            y_pos.append(y_pos[-1])
        ax.plot([0.0, 0.0, t_fin], [y0, yf, yf],
                color=ORANGE, linewidth=1.4, linestyle="--", label="Referencia")
        ax.plot(t_pos, y_pos,
                color=CYAN, linewidth=1.6, label="Posición")
        if abs(amp) >= PASO_MIN:
            ax.axhline(yf + banda, color=TEXTDIM, linewidth=0.7, linestyle=":")
            ax.axhline(yf - banda, color=TEXTDIM, linewidth=0.7, linestyle=":")
            ax.text(0.98, yf + banda, "±2 %", color=TEXTDIM,
                    fontsize=7, va="bottom", ha="right",
                    transform=ax.get_yaxis_transform())
        if self._marca_ts is not None:
            ax.axvline(self._marca_ts, color=WARN, linewidth=1.1,
                       linestyle="--")
            ax.text(self._marca_ts, 1.0, " ts", color=WARN, fontsize=8,
                    rotation=90, va="top", ha="right",
                    transform=ax.get_xaxis_transform())
        if self._marca_te is not None:
            ax.axvline(self._marca_te, color=TEXT, linewidth=1.1,
                       linestyle=":")
            ax.text(self._marca_te, 1.0, " te", color=TEXT, fontsize=8,
                    rotation=90, va="top", ha="left",
                    transform=ax.get_xaxis_transform())

        ys = self._paso_y + [y0, yf]
        bajo = min(ys) - banda
        alto = max(ys) + banda
        if alto - bajo < 4.0:
            medio = 0.5 * (bajo + alto)
            bajo, alto = medio - 2.0, medio + 2.0
        else:
            holgura = 0.12 * (alto - bajo)
            bajo -= holgura
            alto += holgura
        ax.set_ylim(bajo, alto)
        ax.set_xlim(0.0, t_fin)
        leg = ax.legend(loc="upper right", fontsize=8, frameon=False)
        for txt in leg.get_texts():
            txt.set_color(TEXTDIM)
        self._fig2.tight_layout(pad=0.6)
        self._mpl2.draw_idle()

    def _anotar_prueba(self):
        # El escalón ya se quedó quieto. El error es referencia menos posición.
        pos_fin = self._paso_y[-1]
        self._pruebas.append({
            "kp": self._kp,
            "ki": self._ki,
            "kd": self._kd,
            "pos_in": self._paso_y0,
            "ref_fin": self._paso_yf,
            "pos_fin": pos_fin,
            "error": self._paso_yf - pos_fin,
            "ts": self._ts_s,
            "te": self._marca_te,
            "mp": self._mp_pct,
            "en_promedio": True,
        })
        self._log_add("ok", f"Prueba {len(self._pruebas)} anotada")
        self._refrescar_tabla(ir_al_final=True)

    def _texto_prueba(self, i, p):
        def num(valor, dec):
            if valor is None:
                return "—"
            return f"{valor:.{dec}f}"

        def ganancia(valor):
            if valor is None:
                return "—"
            texto = f"{valor:.4f}".rstrip("0").rstrip(".")
            return texto if texto else "0"

        return (
            str(i),
            ganancia(p["kp"]),
            ganancia(p["ki"]),
            ganancia(p["kd"]),
            num(p["pos_in"], 1),
            num(p["ref_fin"], 1),
            num(p["pos_fin"], 1),
            num(p["error"], 1),
            num(p["ts"], 3),
            num(p["te"], 3),
            num(p["mp"], 1),
        )

    def _abrir_pruebas(self):
        win = self._ventana_pruebas
        if win is not None and win.winfo_exists():
            win.lift()
            win.focus_force()
            return

        win = tk.Toplevel(self)
        win.title("Pruebas")
        win.configure(bg=BG)
        win.transient(self.winfo_toplevel())
        sw = win.winfo_screenwidth()
        sh = win.winfo_screenheight()
        w = min(sw - 40, max(1280, int(sw * 0.92)))
        h = min(sh - 80, max(680, int(sh * 0.78)))
        win.minsize(1100, 560)
        win.geometry(f"{w}x{h}+{(sw - w) // 2}+{(sh - h) // 2}")
        self._ventana_pruebas = win

        tk.Label(win, text="Tabla de pruebas",
                 font=(FF, FSL, "bold"), bg=BG, fg=TEXT).pack(
                     anchor="w", padx=14, pady=(12, 0))
        tk.Label(win,
                 text="Se anota sola cuando el escalón se queda quieto. "
                      "Clic en Incluir para meter o sacar esa prueba del promedio.",
                 font=(FF, FS - 1), bg=BG, fg=TEXTDIM).pack(
                     anchor="w", padx=14, pady=(0, 8))

        columnas = (
            ("uso", "Incluir", 84),
            ("n", "N°", 48),
            ("kp", "Kp", 70),
            ("ki", "Ki", 70),
            ("kd", "Kd", 70),
            ("pos_in", "Pos. inicial (mm)", 124),
            ("ref_fin", "Ref. final (mm)", 118),
            ("pos_fin", "Pos. final (mm)", 124),
            ("error", "Error (mm)", 96),
            ("ts", "ts (s)", 78),
            ("te", "te (s)", 78),
            ("mp", "Mp (%)", 76),
        )
        estilo = ttk.Style(win)
        estilo.configure("Pruebas.Treeview",
                         background=BG, fieldbackground=BG, foreground=TEXT,
                         bordercolor=BORDER, rowheight=px(32),
                         font=(FF, FS + 1))
        estilo.configure("Pruebas.Treeview.Heading",
                         background=CARD2, foreground=TEXT,
                         relief="flat", font=(FF, FS, "bold"))
        estilo.map("Pruebas.Treeview",
                   background=[("selected", ACCENT)],
                   foreground=[("selected", TEXT)])
        estilo.map("Pruebas.Treeview.Heading",
                   background=[("active", SURFACE)])

        marco = tk.Frame(win, bg=BORDER, bd=0)
        marco.pack(fill="both", expand=True, padx=14, pady=(0, 8))
        ids = [c[0] for c in columnas]
        arbol = ttk.Treeview(marco, columns=ids, show="headings",
                             style="Pruebas.Treeview", selectmode="browse")
        for clave, titulo, ancho in columnas:
            arbol.heading(clave, text=titulo, anchor="center")
            arbol.column(clave, width=ancho, minwidth=48, anchor="center",
                         stretch=True)
        arbol.tag_configure("par", background=SURFACE, foreground=TEXT)
        arbol.tag_configure("impar", background=BG, foreground=TEXT)
        arbol.tag_configure("fuera", background=BG, foreground=TEXTDIM)
        arbol.bind("<ButtonRelease-1>", self._clic_incluir)
        sy = tk.Scrollbar(marco, orient="vertical", command=arbol.yview,
                          bg=SURFACE, troughcolor=BG)
        sx = tk.Scrollbar(marco, orient="horizontal", command=arbol.xview,
                          bg=SURFACE, troughcolor=BG)
        arbol.configure(yscrollcommand=sy.set, xscrollcommand=sx.set)
        sy.pack(side="right", fill="y")
        sx.pack(side="bottom", fill="x")
        arbol.pack(side="left", fill="both", expand=True)
        self._arbol_pruebas = arbol

        caja = tk.Frame(win, bg=CARD2)
        caja.pack(fill="x", padx=14, pady=(0, 8))
        self._lbl_titulo_prom = tk.Label(
            caja, text="Promedio", font=(FF, FS, "bold"),
            bg=CARD2, fg=WARN)
        self._lbl_titulo_prom.grid(row=0, column=0, columnspan=10,
                                   sticky="w", padx=10, pady=(6, 0))
        titulos_prom = (
            "Kp", "Ki", "Kd", "Pos. inicial", "Ref. final",
            "Pos. final", "Error", "ts", "te", "Mp",
        )
        self._lbls_prom = []
        for i, titulo in enumerate(titulos_prom):
            caja.columnconfigure(i, weight=1)
            tk.Label(caja, text=titulo, font=(FF, FS - 2),
                     bg=CARD2, fg=TEXTDIM).grid(row=1, column=i, pady=(2, 0))
            val = tk.Label(caja, text="—", font=(FF, FS, "bold"),
                           bg=CARD2, fg=WARN)
            val.grid(row=2, column=i, pady=(0, 8))
            self._lbls_prom.append(val)

        barra = tk.Frame(win, bg=BG)
        barra.pack(fill="x", padx=14, pady=(0, 12))
        self._lbl_n_pruebas = tk.Label(
            barra, text="", font=(FF, FS), bg=BG, fg=TEXTDIM)
        self._lbl_n_pruebas.pack(side="left")
        FlatButton(barra, "Exportar CSV", command=self._exportar_pruebas,
                   bg_color=ACCENT2, fg_color=TEXT,
                   width=140, height=32).pack(side="right", padx=(8, 0))
        FlatButton(barra, "Ninguna", command=lambda: self._marcar_promedio(False),
                   bg_color=BORDER, fg_color=TEXT,
                   width=100, height=32).pack(side="right", padx=(8, 0))
        FlatButton(barra, "Todas", command=lambda: self._marcar_promedio(True),
                   bg_color=BORDER, fg_color=TEXT,
                   width=100, height=32).pack(side="right", padx=(8, 0))
        FlatButton(barra, "Borrar última", command=self._borrar_ultima_prueba,
                   bg_color=BORDER, fg_color=TEXT,
                   width=130, height=32).pack(side="right", padx=(8, 0))
        FlatButton(barra, "Vaciar", command=self._vaciar_pruebas,
                   bg_color=DANGER, fg_color=TEXT,
                   width=100, height=32).pack(side="right")
        self._refrescar_tabla()

    def _elegidas(self):
        return [p for p in self._pruebas if p.get("en_promedio", True)]

    def _media_pruebas(self):
        elegidas = self._elegidas()
        if not elegidas:
            return None
        claves = ("kp", "ki", "kd", "pos_in", "ref_fin", "pos_fin",
                  "error", "ts", "te", "mp")
        media = {}
        for clave in claves:
            vals = [p[clave] for p in elegidas if p[clave] is not None]
            media[clave] = sum(vals) / len(vals) if vals else None
        return media

    def _valores_fila(self, i, p):
        texto = self._texto_prueba(i, p)
        marca = "Sí" if p.get("en_promedio", True) else "No"
        return (marca,) + texto

    def _clic_incluir(self, event):
        arbol = self._arbol_pruebas
        if arbol.identify_region(event.x, event.y) != "cell":
            return
        if arbol.identify_column(event.x) != "#1":
            return
        fila = arbol.identify_row(event.y)
        if not fila:
            return
        idx = int(fila) - 1
        if idx < 0 or idx >= len(self._pruebas):
            return
        self._pruebas[idx]["en_promedio"] = not self._pruebas[idx].get(
            "en_promedio", True)
        self._refrescar_tabla()
        if arbol.exists(fila):
            arbol.selection_set(fila)
            arbol.focus(fila)

    def _marcar_promedio(self, incluir):
        if not self._pruebas:
            return
        for p in self._pruebas:
            p["en_promedio"] = incluir
        self._refrescar_tabla()

    def _refrescar_tabla(self, ir_al_final=False):
        win = self._ventana_pruebas
        arbol = self._arbol_pruebas
        if win is None or arbol is None or not win.winfo_exists():
            return
        vista = arbol.yview()
        arbol.delete(*arbol.get_children())
        for i, p in enumerate(self._pruebas, start=1):
            if p.get("en_promedio", True):
                tag = "par" if i % 2 == 0 else "impar"
            else:
                tag = "fuera"
            arbol.insert("", "end", iid=str(i),
                         values=self._valores_fila(i, p), tags=(tag,))
        hijos = arbol.get_children()
        if hijos and ir_al_final:
            arbol.see(hijos[-1])
        elif hijos:
            arbol.yview_moveto(vista[0])
        n_sel = len(self._elegidas())
        self._lbl_n_pruebas.config(
            text=f"{n_sel} de {len(self._pruebas)} en el promedio")
        self._lbl_titulo_prom.config(
            text=f"Promedio ({n_sel} de {len(self._pruebas)})")
        media = self._media_pruebas()
        if media is None:
            textos = ("—",) * 10
        else:
            textos = self._texto_prueba("Prom.", media)[1:]
        for lbl, texto in zip(self._lbls_prom, textos):
            lbl.config(text=texto)

    def _borrar_ultima_prueba(self):
        if not self._pruebas:
            return
        self._pruebas.pop()
        self._refrescar_tabla()

    def _vaciar_pruebas(self):
        if not self._pruebas:
            return
        if not messagebox.askyesno(
                "Vaciar tabla", "¿Borrar todas las pruebas anotadas?",
                parent=self._ventana_pruebas):
            return
        self._pruebas.clear()
        self._refrescar_tabla()

    def _exportar_pruebas(self):
        if not self._pruebas:
            messagebox.showinfo(
                "Sin pruebas", "Todavía no hay una prueba anotada.",
                parent=self._ventana_pruebas)
            return
        ruta = filedialog.asksaveasfilename(
            parent=self._ventana_pruebas,
            title="Exportar pruebas",
            defaultextension=".csv",
            filetypes=(("CSV", "*.csv"),),
            initialfile="pruebas_guia_lineal.csv")
        if not ruta:
            return
        titulos = (
            "Incluir", "N°", "Kp", "Ki", "Kd",
            "Pos inicial (mm)", "Ref final (mm)", "Pos final (mm)",
            "Error (mm)", "ts (s)", "te (s)", "Mp (%)",
        )

        def celdas_csv(valores):
            return ["" if c == "—" else c.replace(".", ",") for c in valores]

        try:
            with open(ruta, "w", newline="", encoding="utf-8-sig") as f:
                out = csv.writer(f, delimiter=";")
                out.writerow(titulos)
                for i, p in enumerate(self._pruebas, start=1):
                    out.writerow(celdas_csv(self._valores_fila(i, p)))
                media = self._media_pruebas()
                if media is not None:
                    nums = self._texto_prueba("Promedio", media)
                    out.writerow(celdas_csv(("",) + nums))
        except OSError as exc:
            messagebox.showerror(
                "No se pudo guardar", str(exc), parent=self._ventana_pruebas)
            return
        self._log_add("ok", f"CSV guardado: {ruta}")

    def _log_add(self, tag, msg):
        ts = time.strftime("%H:%M:%S")
        self._log.config(state="normal")
        self._log.insert("end", f"[{ts}]", "ts")
        self._log.insert("end", f"  {msg}\n", tag)
        self._log.see("end")
        self._log.config(state="disabled")


class App(tk.Tk):

    def __init__(self):
        super().__init__()
        self.title("Control Sistema Lineal")
        self.configure(bg=BG)

        global UI_SCALE
        # tkinter mide en puntos. Esto lo pasa a los pixeles reales de la pantalla.
        try:
            dpi = self.winfo_fpixels("1i")
            self.tk.call("tk", "scaling", dpi / 72.0)
            UI_SCALE = dpi / 96.0
        except Exception:
            pass

        sw = self.winfo_screenwidth()
        sh = self.winfo_screenheight()
        w = max(1100, int(sw * 0.82))
        h = max(700, int(sh * 0.82))
        x = (sw - w) // 2
        y = (sh - h) // 2
        self.geometry(f"{w}x{h}+{x}+{y}")
        self.minsize(1060, 680)
        self._apply_style()
        self._show_splash()

    def _apply_style(self):
        s = ttk.Style(self)
        s.theme_use("default")
        s.configure("TCombobox",
                    fieldbackground=SURFACE,
                    background=SURFACE,
                    foreground=TEXT,
                    arrowcolor=ACCENT,
                    bordercolor=BORDER,
                    insertcolor=TEXT)
        s.map("TCombobox",
              fieldbackground=[("readonly", SURFACE)],
              selectbackground=[("readonly", CARD)],
              selectforeground=[("readonly", TEXT)])

    def _show_splash(self):
        self._splash = SplashFrame(self, self._show_main)
        self._splash.pack(fill="both", expand=True)

    def _show_main(self):
        self._splash.destroy()
        MainFrame(self).pack(fill="both", expand=True)


if __name__ == "__main__":
    app = App()
    app.mainloop()
