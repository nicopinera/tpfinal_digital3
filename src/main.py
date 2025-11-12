#!/usr/bin/env python3
"""
main.py

Lectura por UART de muestras ADC y ploteo en tiempo real.

Mejoras incluidas en esta versión:
- Selección de backend de matplotlib antes de importar pyplot para intentar usar un backend
  interactivo (Qt5Agg, TkAgg, GTK3Agg...) si hay DISPLAY disponible.
- Si no hay DISPLAY, se detecta modo "headless" y se sugiere cómo proceder (xvfb-run,
  instalar python3-tk o usar X forwarding). En ese caso el script no intenta mostrar la figura.
- Se desactiva el cache ilimitado de frames en FuncAnimation (cache_frame_data=False)
  para evitar warnings.
- Mantiene la configuración previa (puerto por defecto, received-bits/adcbits, etc.)
"""
import argparse
import os
import struct
import threading
import time
from collections import deque
from math import ceil

import importlib.util
import matplotlib

# Selección robusta de backend antes de importar pyplot:
# - Si hay DISPLAY, intentamos backends interactivos solo si sus bindings están presentes.
# - Si no hay backend interactivo disponible, o no hay DISPLAY, usamos Agg.
def _binding_available(pkg_name: str) -> bool:
    return importlib.util.find_spec(pkg_name) is not None

def _try_use_backend(backend_name: str) -> bool:
    try:
        lower = backend_name.lower()
        if "qt" in lower:
            # Qt backends requieren PyQt5 / PySide2 / PySide6
            for pkg in ("PyQt5", "PySide2", "PySide6"):
                if _binding_available(pkg):
                    matplotlib.use(backend_name, force=True)
                    return True
            return False
        if backend_name == "TkAgg":
            if _binding_available("tkinter"):
                matplotlib.use(backend_name, force=True)
                return True
            return False
        if backend_name == "GTK3Agg":
            if _binding_available("gi"):
                matplotlib.use(backend_name, force=True)
                return True
            return False
        # Intento genérico
        matplotlib.use(backend_name, force=True)
        return True
    except Exception:
        return False

# Intentamos seleccionar un backend interactivo si hay DISPLAY
if os.environ.get("DISPLAY"):
    for backend in ("Qt5Agg", "TkAgg", "GTK3Agg", "Qt4Agg"):
        if _try_use_backend(backend):
            # backend seteado con éxito
            break
    else:
        # Ningún backend interactivo disponible -> usar Agg
        matplotlib.use("Agg", force=True)
else:
    # No hay DISPLAY -> modo headless
    matplotlib.use("Agg", force=True)

import matplotlib.pyplot as plt
import numpy as np
import serial
from matplotlib.animation import FuncAnimation

# Puerto por defecto (cámbialo aquí si quieres otro)
DEFAULT_SERIAL_PORT = "/dev/ttyUSB0"
DEFAULT_BAUD = 115200


def reader_thread(ser: serial.Serial, buffer: deque, stop_event: threading.Event,
                  read_bytes: int, received_bits: int, adc_bits: int):
    """Lee read_bytes del puerto en loop, convierte a int y lo reescala a adc_bits."""
    shift = adc_bits - received_bits
    mask = (1 << received_bits) - 1
    while not stop_event.is_set():
        try:
            data = ser.read(read_bytes)
            if len(data) == read_bytes:
                # interpretar en little-endian como entero sin signo
                # ensamblar little-endian (parte baja primero) y limitar a received_bits
                value_raw = int.from_bytes(data, byteorder="little", signed=False) & mask
                # reajuste de resolución entre received_bits y adc_bits
                if shift > 0:
                    # recibimos MSBs y LSBs fueron descartados -> rellenar LSB con ceros
                    value = value_raw << shift
                elif shift < 0:
                    # recibimos más bits de los que tiene el ADC real -> descartar LSB
                    value = value_raw >> (-shift)
                else:
                    value = value_raw
                buffer.append(int(value))
            else:
                # si no llegaron bytes completos, pequeña espera para no bloquear CPU
                time.sleep(0.001)
        except serial.SerialException:
            stop_event.set()
            break


def choose_style(preferred: str = "seaborn-darkgrid"):
    """Usa preferred style si está disponible, sino usa un fallback seguro."""
    try:
        available = plt.style.available
    except Exception:
        available = []
    if preferred in available:
        plt.style.use(preferred)
        return preferred
    for candidate in ("seaborn", "ggplot", "fivethirtyeight", "classic", "default"):
        if candidate in available:
            plt.style.use(candidate)
            return candidate
    return None


def main():
    parser = argparse.ArgumentParser(
        description="Plot UART ADC data in real time. You can set received-bits and adc-bits."
    )
    parser.add_argument("--port", "-p", required=False, default=None,
                        help=f"Puerto serie (ej: /dev/ttyUSB0 o COM3). Si no se pasa, se usa {DEFAULT_SERIAL_PORT}")
    parser.add_argument("--baud", "-b", type=int, default=DEFAULT_BAUD, help=f"Baud rate (por defecto: {DEFAULT_BAUD})")
    parser.add_argument("--samples", "-s", type=int, default=1000, help="Tamaño del buffer circular")
    parser.add_argument("--received-bits", type=int, default=8,
                        help="Bits por muestra transmitidos (ej: 8 si la MCU envía 1 byte por muestra)")
    parser.add_argument("--adc-bits", type=int, default=12,
                        help="Resolución real del ADC en la MCU (ej: 12 para ADC de 12 bits)")
    parser.add_argument("--smooth", type=int, default=1,
                        help="Window size para media móvil aplicada al plot (1 = sin suavizado).")
    parser.add_argument("--drawstyle", choices=("default", "steps-pre", "steps-mid", "steps-post"),
                        default="steps-mid",
                        help="Estilo de dibujo de la línea (usar steps-* mejora señales digitales).")
    parser.add_argument("--interval", "-i", type=float, default=30, help="Intervalo de actualización en ms")
    args = parser.parse_args()

    # Determinar puerto: CLI -> ENV -> DEFAULT
    port = args.port or os.environ.get("SERIAL_PORT") or DEFAULT_SERIAL_PORT

    # Calculamos cuantos bytes leer por muestra (ceil(received_bits/8))
    read_bytes = ceil(args.received_bits / 8)

    # Límite del eje Y según resolución ADC real
    max_adc_value = (1 << args.adc_bits) - 1
    buffer = deque(maxlen=args.samples)

    try:
        ser = serial.Serial(port, args.baud, timeout=0.1)
    except serial.SerialException as e:
        print(f"Error abriendo puerto {port}: {e}")
        return

    stop_event = threading.Event()
    th = threading.Thread(target=reader_thread,
                          args=(ser, buffer, stop_event, read_bytes, args.received_bits, args.adc_bits),
                          daemon=True)
    th.start()
    print(f"Puerto {port} abierto a {args.baud} baudios. Ploteando... (Ctrl+C para salir)")
    print(f"Configuración: received_bits={args.received_bits}, adc_bits={args.adc_bits}, read_bytes={read_bytes}")

    used_style = choose_style("seaborn-darkgrid")
    if used_style:
        print(f"Usando estilo matplotlib: {used_style}")

    # Si el backend actual es 'Agg' y no hay DISPLAY, mpl no puede mostrar ventanas interactivas.
    backend = matplotlib.get_backend().lower()
    if backend == "agg" and not os.environ.get("DISPLAY"):
        print()
        print("Modo headless detectado (no hay DISPLAY y backend 'Agg').")
        print("Opciones para obtener una ventana interactiva:")
        print("  - Ejecutar con X forwarding: ssh -X ... o abrir desde tu escritorio local.")
        print("  - Instalar un backend gráfico (por ejemplo python3-tk para TkAgg):")
        print("      sudo apt install python3-tk")
        print("  - Usar xvfb para emular un framebuffer virtual:")
        print("      xvfb-run -s \"-screen 0 1920x1080x24\" python src/main.py")
        try:
            ser.close()
        except Exception:
            pass
        stop_event.set()
        th.join(timeout=1.0)
        return

    # Crear figura con manejo de errores si el backend seleccionado falla al inicializar
    try:
        fig, ax = plt.subplots()
    except Exception as e:
        print("No se pudo crear la figura de matplotlib con el backend actual:")
        print(f"  backend = {matplotlib.get_backend()}  error = {e}")
        print("Posibles soluciones:")
        print("  - Instalar un binding compatible (PyQt5/PySide2/… o python3-tk).")
        print("  - Ejecutar con X forwarding o usar xvfb-run si estás en headless.")
        try:
            ser.close()
        except Exception:
            pass
        stop_event.set()
        th.join(timeout=1.0)
        return

    line, = ax.plot([], [], lw=1)
    # aplicar estilo de dibujo (escalones por defecto para señales digitales)
    try:
        line.set_drawstyle(args.drawstyle)
    except Exception:
        pass
    ax.set_xlim(0, args.samples)
    ax.set_ylim(0, max_adc_value)
    ax.set_xlabel("Muestras (últimas)")
    ax.set_ylabel(f"Valor ADC (0..{max_adc_value})")
    ax.set_title(f"UART -> ADC (puerto: {port} @ {args.baud})")
    ax.grid(True)

    def update(frame):
        ylist = list(buffer)
        if not ylist:
            line.set_data([], [])
            return line,
        y = np.asarray(ylist, dtype=float)
        # suavizado por media móvil si se pidió
        if args.smooth and args.smooth > 1 and y.size > 1:
            k = args.smooth
            kernel = np.ones(k, dtype=float) / k
            y = np.convolve(y, kernel, mode="same")
        x = np.arange(len(y))
        line.set_data(x, y)
        return line,

    # Evitamos cache ilimitado de frames y warnings con cache_frame_data=False
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
        print("Cerrando. Bye.")


if __name__ == "__main__":
    main()