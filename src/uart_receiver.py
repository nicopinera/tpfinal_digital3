import serial
import time
import numpy as np
from collections import deque
import matplotlib.pyplot as plt

# Configuración
BAUD_RATE = 9600
SAMPLE_INTERVAL = 0.5  # segundos entre impresiones
PLOT_ENABLED = True    # Cambiar a False si solo quieres consola

# Configurar comunicación serial
try:
    ser = serial.Serial('/dev/ttyUSB0', BAUD_RATE, timeout=1)
    print(f"📡 Conectado a {ser.port} a {BAUD_RATE} baudios")
    print(f"⏱️  Mostrando datos cada {SAMPLE_INTERVAL} segundos")
    print("=" * 60)
except Exception as e:
    print(f"❌ Error al conectar: {e}")
    exit()

# Variables para estadísticas
last_print_time = time.time()
data_buffer = []
sample_count = 0
total_samples = 0
freq_estimate = 0

# Configurar gráfica si está habilitada
if PLOT_ENABLED:
    plt.ion()
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8))
    time_plot = ax1.plot([], [], 'b-', linewidth=1, alpha=0.8)[0]
    hist_plot = ax2.hist([], bins=50, range=(0, 255), alpha=0.7, color='green')
    ax1.set_title('Señal ADC en Tiempo Real (1 Hz esperada)')
    ax1.set_ylabel('Valor ADC (0-255)')
    ax1.grid(True, alpha=0.3)
    ax2.set_title('Histograma de Valores')
    ax2.set_xlabel('Valor ADC')
    ax2.set_ylabel('Frecuencia')
    ax2.grid(True, alpha=0.3)
    plt.tight_layout()

def calculate_frequency(timestamps):
    """Calcula frecuencia estimada basada en timestamps"""
    if len(timestamps) < 2:
        return 0
    intervals = np.diff(timestamps)
    return 1.0 / np.mean(intervals) if np.mean(intervals) > 0 else 0

def print_formatted_data(data, interval_samples, freq):
    """Imprime datos formateados de manera legible"""
    if not data:
        print(f"⏳ {time.strftime('%H:%M:%S')} - Sin datos en este intervalo")
        return
    
    # Estadísticas básicas
    data_array = np.array(data)
    mean_val = np.mean(data_array)
    std_val = np.std(data_array)
    min_val = np.min(data_array)
    max_val = np.max(data_array)
    
    print(f"\n📊 {time.strftime('%H:%M:%S')} - Intervalo: {SAMPLE_INTERVAL}s")
    print(f"   Muestras: {interval_samples} | Frecuencia estimada: {freq:.2f} Hz")
    print(f"   Estadísticas: Media={mean_val:.1f}, Std={std_val:.1f}, Min={min_val}, Max={max_val}")
    
    # Mostrar primeros 10 valores como ejemplo
    preview = data[:10]
    print(f"   Preview: {' '.join(f'{x:3d}' for x in preview)}" + (" ..." if len(data) > 10 else ""))
    
    # Detectar si hay una señal periódica
    if std_val > 10:  # Umbral arbitrario para detectar variación
        print("   📈 Señal: Variación detectada (posible señal periódica)")
    else:
        print("   📉 Señal: Poca variación (posible DC o ruido)")

def update_plot(data):
    """Actualiza la gráfica en tiempo real"""
    if not PLOT_ENABLED or not data:
        return
        
    # Actualizar plot de tiempo
    time_plot.set_data(range(len(data)), data)
    ax1.relim()
    ax1.autoscale_view()
    
    # Actualizar histograma
    ax2.clear()
    ax2.hist(data, bins=50, range=(0, 255), alpha=0.7, color='green', edgecolor='black')
    ax2.set_title('Histograma de Valores')
    ax2.set_xlabel('Valor ADC')
    ax2.set_ylabel('Frecuencia')
    ax2.grid(True, alpha=0.3)
    
    fig.canvas.draw()
    fig.canvas.flush_events()

# Bucle principal
try:
    timestamps = []
    
    while True:
        # Leer datos disponibles
        if ser.in_waiting > 0:
            try:
                # Leer línea completa
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    # Convertir a entero
                    value = int(line)
                    data_buffer.append(value)
                    sample_count += 1
                    total_samples += 1
                    timestamps.append(time.time())
                    
                    # Mantener solo últimos timestamps para cálculo de frecuencia
                    if len(timestamps) > 100:
                        timestamps.pop(0)
                        
            except (ValueError, UnicodeDecodeError) as e:
                # Ignorar errores de conversión
                pass
        
        # Mostrar datos cada intervalo
        current_time = time.time()
        if current_time - last_print_time >= SAMPLE_INTERVAL:
            if data_buffer:
                # Calcular frecuencia estimada
                freq_estimate = calculate_frequency(timestamps[-min(50, len(timestamps)):])
                
                # Mostrar datos formateados
                print_formatted_data(data_buffer, sample_count, freq_estimate)
                
                # Actualizar gráfica
                update_plot(data_buffer)
                
                # Reiniciar contadores
                data_buffer = []
                sample_count = 0
            
            last_print_time = current_time
        
        time.sleep(0.001)
        
except KeyboardInterrupt:
    print(f"\n\n🛑 Adquisición detenida")
    print(f"📈 Total de muestras recibidas: {total_samples}")
finally:
    ser.close()
    print("🔌 Conexión cerrada")