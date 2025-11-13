#!/usr/bin/env python3
"""
uart_plotter.py

Ventana interactiva que recibe muestras por UART y las muestra en tiempo real.
Versión corregida para recibir señales de diferentes frecuencias.
"""
import argparse
import os
import sys
import time
import threading
from collections import deque
from math import ceil
import importlib.util
import serial
import matplotlib

# Configuración del backend GUI
def _binding_available(pkg: str) -> bool:
    return importlib.util.find_spec(pkg) is not None

if os.environ.get("DISPLAY"):
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

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.animation import FuncAnimation

def parse_args():
    p = argparse.ArgumentParser(description="Plot UART ADC data in an interactive window.")
    p.add_argument("-p", "--port", default=os.environ.get("SERIAL_PORT", "/dev/ttyUSB0"))
    p.add_argument("-b", "--baud", type=int, default=9600)
    p.add_argument("--samples", type=int, default=1000, help="Tamaño del buffer mostrado")
    p.add_argument("-i", "--interval", type=float, default=30.0, help="Intervalo de actualización (ms)")
    p.add_argument("--smooth", type=int, default=1, help="Suavizado de media móvil")
    p.add_argument("--history", type=int, default=2000, help="Máximo histórico de muestras a guardar")
    return p.parse_args()

def reader_thread(ser, buf: deque, stop_event: threading.Event, history_size: int):
    """
    Hilo que lee bytes individuales del UART y los almacena en el buffer
    """
    temp_buffer = bytearray()
    
    while not stop_event.is_set():
        try:
            # Leer todos los bytes disponibles
            available = ser.in_waiting
            if available > 0:
                data = ser.read(available)
                temp_buffer.extend(data)
                
                # Procesar todos los bytes completos recibidos
                for byte_val in temp_buffer:
                    buf.append(byte_val)
                
                temp_buffer.clear()
                
            else:
                time.sleep(0.001)  # Pequeña pausa si no hay datos
                
        except (serial.SerialException, OSError) as e:
            print(f"Error de lectura serie: {e}", file=sys.stderr)
            stop_event.set()
            break
        except Exception as e:
            print(f"Error inesperado: {e}", file=sys.stderr)
            time.sleep(0.01)

def calculate_frequency(timestamps):
    """Calcula la frecuencia basada en timestamps de muestras"""
    if len(timestamps) < 2:
        return 0.0
    
    intervals = np.diff(timestamps)
    valid_intervals = intervals[intervals > 0]  # Filtrar intervalos válidos
    
    if len(valid_intervals) == 0:
        return 0.0
    
    avg_interval = np.mean(valid_intervals)
    return 1.0 / avg_interval if avg_interval > 0 else 0.0

def main():
    args = parse_args()
    
    # Verificar backend GUI
    backend = matplotlib.get_backend().lower()
    if backend == "agg" or not os.environ.get("DISPLAY"):
        print("No se detectó backend GUI interactivo.", file=sys.stderr)
        print("Instala PyQt5, PySide2 o python3-tk y exporta DISPLAY.", file=sys.stderr)
        return 1

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.1)
        print(f"Conectado a {args.port} @ {args.baud} baudios")
    except Exception as e:
        print(f"Error abriendo puerto {args.port}: {e}", file=sys.stderr)
        return 1

    # Buffer principal con más capacidad para histórico
    buf = deque(maxlen=args.history)
    stop_event = threading.Event()
    
    # Timestamps para cálculo de frecuencia
    timestamps = deque(maxlen=100)  # Guardar últimos 100 timestamps
    
    # Iniciar hilo de lectura
    th = threading.Thread(target=reader_thread,
                         args=(ser, buf, stop_event, args.history),
                         daemon=True)
    th.start()

    # Configurar la gráfica
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8))
    fig.suptitle(f'Señal ADC en Tiempo Real - {args.port} @ {args.baud} baudios')
    
    # Gráfica principal de la señal
    line1, = ax1.plot([], [], 'b-', linewidth=1.5, alpha=0.8, label='Señal ADC')
    ax1.set_xlim(0, args.samples)
    ax1.set_ylim(0, 255)
    ax1.set_xlabel('Muestras')
    ax1.set_ylabel('Valor ADC (0-255)')
    ax1.grid(True, alpha=0.3)
    ax1.legend()
    
    # Gráfica de espectro (FFT)
    line2, = ax2.plot([], [], 'r-', linewidth=1.5, alpha=0.8, label='Espectro')
    ax2.set_xlim(0, 50)  # Mostrar hasta 50 Hz
    ax2.set_ylim(0, 100)
    ax2.set_xlabel('Frecuencia (Hz)')
    ax2.set_ylabel('Magnitud')
    ax2.grid(True, alpha=0.3)
    ax2.legend()
    
    # Texto para información en tiempo real
    info_text = ax1.text(0.02, 0.98, '', transform=ax1.transAxes, verticalalignment='top',
                        bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))

    # Variables para estadísticas
    last_update_time = time.time()
    sample_count = 0
    last_sample_count = 0

    def update(frame):
        nonlocal last_update_time, last_sample_count
        
        current_time = time.time()
        ylist = list(buf)
        
        if not ylist:
            line1.set_data([], [])
            line2.set_data([], [])
            info_text.set_text('Esperando datos...')
            return line1, line2, info_text
        
        # Calcular frecuencia de muestreo actual
        current_sample_count = len(ylist)
        if current_sample_count > last_sample_count:
            timestamps.append(current_time)
            last_sample_count = current_sample_count
        
        sampling_freq = calculate_frequency(list(timestamps))
        
        # Preparar datos para la gráfica principal
        y_data = np.array(ylist[-args.samples:], dtype=float)
        x_data = np.arange(len(y_data))
        
        # Aplicar suavizado si está habilitado
        if args.smooth > 1 and len(y_data) > args.smooth:
            kernel = np.ones(args.smooth) / args.smooth
            y_smooth = np.convolve(y_data, kernel, mode='same')
            line1.set_data(x_data, y_smooth)
        else:
            line1.set_data(x_data, y_data)
        
        # Actualizar límites dinámicamente
        if len(y_data) > 0:
            ax1.set_xlim(0, max(len(y_data), args.samples))
            y_min, y_max = np.min(y_data), np.max(y_data)
            y_range = max(y_max - y_min, 10)  # Mínimo rango de 10
            ax1.set_ylim(max(0, y_min - y_range * 0.1), min(255, y_max + y_range * 0.1))
        
        # Calcular y mostrar FFT para análisis de frecuencia
        if len(y_data) >= 64:  # Mínimo para FFT útil
            # Remover componente DC
            y_clean = y_data - np.mean(y_data)
            
            # Calcular FFT
            N = len(y_clean)
            if sampling_freq > 0:
                yf = np.fft.fft(y_clean)
                xf = np.fft.fftfreq(N, 1/sampling_freq)
                
                # Tomar solo frecuencias positivas
                idx_pos = np.where(xf > 0)
                xf_pos = xf[idx_pos]
                yf_pos = np.abs(yf[idx_pos]) / N * 2
                
                # Encontrar frecuencia dominante
                if len(yf_pos) > 0:
                    dominant_freq_idx = np.argmax(yf_pos)
                    dominant_freq = xf_pos[dominant_freq_idx]
                    dominant_mag = yf_pos[dominant_freq_idx]
                else:
                    dominant_freq = 0
                    dominant_mag = 0
                
                line2.set_data(xf_pos, yf_pos)
                ax2.set_ylim(0, max(dominant_mag * 1.2, 10))
            else:
                dominant_freq = 0
                dominant_mag = 0
        else:
            dominant_freq = 0
            dominant_mag = 0
        
        # Actualizar información en tiempo real
        info_text.set_text(
            f'Muestras: {len(ylist)}\n'
            f'Frec. muestreo: {sampling_freq:.1f} Hz\n'
            f'Frec. dominante: {dominant_freq:.2f} Hz\n'
            f'Valor actual: {ylist[-1] if ylist else 0}'
        )
        
        last_update_time = current_time
        return line1, line2, info_text

    # Animación más rápida para mejor respuesta
    ani = FuncAnimation(fig, update, interval=args.interval, blit=False, cache_frame_data=False)

    try:
        print("Iniciando visualización... Presiona Ctrl+C para salir")
        plt.tight_layout()
        plt.show(block=True)
    except KeyboardInterrupt:
        print("\nCerrando aplicación...")
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