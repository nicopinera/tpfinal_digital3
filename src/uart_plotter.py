#!/usr/bin/env python3
"""
uart_plotter.py

Ventana interactiva que recibe muestras por UART y las muestra en tiempo real.
Basado en la lógica de uart_receiver/main.py.
"""
import argparse
import os
import sys
import time
import threading
from collections import deque
from math import ceil
import importlib.util
import matplotlib
matplotlib.use('TkAgg')  # o 'Qt5Agg' si tenés PyQt5

import serial
import matplotlib

# selección robusta de backend GUI (solo si hay DISPLAY y bindings)
def _binding_available(pkg: str) -> bool:
    return importlib.util.find_spec(pkg) is not None

if os.environ.get("DISPLAY"):
    # preferir Qt, luego Tk, luego GTK; si fallan usar Agg
    if any(_binding_available(pkg) for pkg in ("PyQt5", "PySide2", "PySide6")):
        try:
            matplotlib.use("Qt5Agg", force=True)
        except Exception:
            pass
    elif _binding_available("tkinter"):
        try:
            matplotlib.use("TkAgg", force=True)
        except Exception:
            pass
    elif _binding_available("gi"):
        try:
            matplotlib.use("GTK3Agg", force=True)
        except Exception:
            pass
# si no se cambió a backend interactivo, matplotlib conservará su backend (posible Agg)

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.animation import FuncAnimation

def parse_args():
    p = argparse.ArgumentParser(description="Plot UART ADC data in an interactive window.")
    p.add_argument("-p", "--port", default=os.environ.get("SERIAL_PORT", "/dev/ttyUSB0"))
    p.add_argument("-b", "--baud", type=int, default=115200)
    p.add_argument("--received-bits", type=int, default=8)
    p.add_argument("--adc-bits", type=int, default=None,
                   help="Si no se pasa, se asume igual a --received-bits (evita reescalar).")
    p.add_argument("--no-rescale", action="store_true", help="No reescalar la muestra recibida.")
    p.add_argument("-s", "--samples", type=int, default=500, help="Tamaño del buffer mostrado")
    p.add_argument("-i", "--interval", type=float, default=50.0, help="Intervalo de actualización (ms)")
    p.add_argument("--smooth", type=int, default=1,
                   help="Window size para media móvil aplicada al plot (1 = sin suavizado).")
    p.add_argument("--drawstyle", choices=("default", "steps-pre", "steps-mid", "steps-post"),
                   default="steps-mid",
                   help="Estilo de dibujo de la línea (usar steps-* mejora señales digitales).")
    return p.parse_args()

def reader_thread(ser, buf: deque, stop_event: threading.Event, read_bytes: int,
                  received_bits: int, adc_bits: int, no_rescale: bool):
    shift = adc_bits - received_bits
    mask = (1 << received_bits) - 1
    while not stop_event.is_set():
        try:
            data = ser.read(read_bytes)
        except (serial.SerialException, OSError) as e:
            print(f"Error de lectura serie: {e}", file=sys.stderr)
            stop_event.set()
            break
        if not data:
            time.sleep(0.001)
            continue
        if len(data) != read_bytes:
            time.sleep(0.001)
            continue
        # ensamblar little-endian (parte baja primero) y limitar a received_bits
        v_raw = int.from_bytes(data, byteorder="little", signed=False) & mask
        if no_rescale:
            v = v_raw
        else:
            if shift > 0:
                v = v_raw << shift
            elif shift < 0:
                v = v_raw >> (-shift)
            else:
                v = v_raw
        buf.append(int(v))

def main():
    args = parse_args()
    if args.adc_bits is None:
        args.adc_bits = args.received_bits
    read_bytes = (args.received_bits + 7) // 8

    backend = matplotlib.get_backend().lower()
    # si backend no es interactivo, sugerir cómo proceder
    if backend == "agg" or not os.environ.get("DISPLAY"):
        print("No se detectó backend GUI interactivo (backend='{}').".format(matplotlib.get_backend()), file=sys.stderr)
        print("Para abrir una ventana interactiva instala un binding (PyQt5/PySide2 o python3-tk) y exporta DISPLAY o usa X forwarding.", file=sys.stderr)
        return 1

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.1)
    except Exception as e:
        print(f"Error abriendo puerto {args.port}: {e}", file=sys.stderr)
        return 1

    buf = deque(maxlen=args.samples)
    stop_event = threading.Event()
    th = threading.Thread(target=reader_thread,
                          args=(ser, buf, stop_event, read_bytes, args.received_bits, args.adc_bits, args.no_rescale),
                          daemon=True)
    th.start()

    max_adc = (1 << args.adc_bits) - 1
    fig, ax = plt.subplots()
    line, = ax.plot([], [], lw=1)
    try:
        line.set_drawstyle(args.drawstyle)
    except Exception:
        pass
    ax.set_xlim(0, args.samples)
    ax.set_ylim(0, max_adc)
    ax.set_xlabel("Muestras (últimas)")
    ax.set_ylabel(f"Valor ADC (0..{max_adc})")
    ax.set_title(f"UART -> ADC (puerto: {args.port} @ {args.baud})")
    ax.grid(True)

    def update(frame):
        ylist = list(buf)
        if not ylist:
            line.set_data([], [])
            return line,
        y = np.asarray(ylist, dtype=float)
        if args.smooth and args.smooth > 1 and y.size > 1:
            k = args.smooth
            kernel = np.ones(k, dtype=float) / k
            y = np.convolve(y, kernel, mode="same")
        x = np.arange(len(y))
        line.set_data(x, y)
        ax.set_xlim(0, max(len(y), args.samples))
        return line,

    ani = FuncAnimation(fig, update, interval=args.interval, blit=False, cache_frame_data=False)

    try:
        plt.show(block=True)
    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        th.join(timeout=1.0)
        try:
            ser.close()
        except Exception:
            pass
    return 0

if __name__ == "__main__":
    sys.exit(main())
