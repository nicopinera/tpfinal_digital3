import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque
import time
import numpy as np

# Configuración inicial
MAX_POINTS = 500  # Reducir puntos para mejor visualización
BAUD_RATE = 9600  # Ajustar según tu configuración en LPC1769

# Buffer para almacenar datos
data_buffer = deque(maxlen=MAX_POINTS)
time_buffer = deque(maxlen=MAX_POINTS)

# Configurar la comunicación serial
try:
    ser = serial.Serial('COM3', BAUD_RATE, timeout=1)  # Cambia COM3 por tu puerto
    print(f"Conectado a {ser.port} a {BAUD_RATE} baudios")
except Exception as e:
    print(f"Error al conectar: {e}")
    exit()

# Configurar la gráfica
plt.ion()
fig, ax = plt.subplots(figsize=(12, 6))
line, = ax.plot([], [], 'b-', linewidth=1, alpha=0.7)
ax.set_xlabel('Tiempo (muestras)')
ax.set_ylabel('Valor ADC')
ax.set_title('Datos desde LPC1769 - Tiempo Real')
ax.grid(True, alpha=0.3)

# Variables para control de velocidad
last_print_time = 0
print_interval = 0.5  # Mostrar en consola cada 0.5 segundos
sample_count = 0

def read_and_process_data():
    global last_print_time, sample_count
    
    if ser.in_waiting > 0:
        try:
            # Leer línea completa
            line = ser.readline().decode('utf-8').strip()
            
            if line:
                # Procesar el dato (asumiendo que es un valor numérico)
                value = float(line)
                
                # Agregar a buffers
                data_buffer.append(value)
                time_buffer.append(sample_count)
                sample_count += 1
                
                # Mostrar en consola controladamente
                current_time = time.time()
                if current_time - last_print_time >= print_interval:
                    print(f"Muestra {sample_count}: {value}")
                    last_print_time = current_time
                
                return True
                
        except (ValueError, UnicodeDecodeError) as e:
            print(f"Error procesando dato: {e}")
            return False
    
    return False

def update_plot():
    if len(data_buffer) > 0:
        # Actualizar datos de la gráfica
        line.set_data(list(time_buffer), list(data_buffer))
        
        # Ajustar límites automáticamente
        ax.relim()
        ax.autoscale_view()
        
        # Redibujar
        fig.canvas.draw()
        fig.canvas.flush_events()

# Bucle principal
try:
    print("Iniciando adquisición... (Ctrl+C para detener)")
    print(f"Mostrando datos en consola cada {print_interval} segundos")
    
    while True:
        if read_and_process_data():
            update_plot()
        
        # Pequeña pausa para no saturar
        time.sleep(0.001)
        
except KeyboardInterrupt:
    print("\nDeteniendo adquisición...")
finally:
    ser.close()
    print("Conexión cerrada")