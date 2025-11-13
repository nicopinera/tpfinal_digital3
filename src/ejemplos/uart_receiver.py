import serial
import threading
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque

# ================================================================
# Parámetros generales
# ================================================================
PORT = '/dev/ttyUSB0'      # Cambiá según tu puerto (ej: COM3 en Windows)
BAUDRATE = 115200
BUFFER_SIZE = 500          # Cantidad de muestras en el gráfico
ADC_BITS = 12              # Resolución del ADC (12 bits -> 0–4095)

# ================================================================
# Hilo lector del puerto serie
# ================================================================
def reader_thread(ser, buffer, stop_event):
    while not stop_event.is_set():
        try:
            # Leer 2 bytes del ADC (little endian)
            data = ser.read(2)
        except (serial.SerialException, OSError):
            stop_event.set()
            break

        # Si no llegaron los 2 bytes, esperar siguiente lectura
        if len(data) != 2:
            continue

        # Combinar bytes (low byte primero, high byte después)
        value_raw = int.from_bytes(data, byteorder="little", signed=False) & 0x0FFF

        # Guardar valor en el buffer (cola circular)
        buffer.append(value_raw)

# ================================================================
# Actualización de la animación
# ================================================================
def update_plot(frame, line, buffer):
    if not buffer:
        return line,

    # Eje X = índices (últimos N puntos)
    x = list(range(len(buffer)))
    y = list(buffer)

    line.set_data(x, y)
    return line,

# ================================================================
# Programa principal
# ================================================================
def main():
    # Abrir puerto serie
    ser = serial.Serial(PORT, BAUDRATE, timeout=1)

    # Buffer circular para las muestras
    buffer = deque(maxlen=BUFFER_SIZE)

    # Evento para detener el hilo limpiamente
    stop_event = threading.Event()

    # Iniciar hilo de lectura
    thread = threading.Thread(target=reader_thread, args=(ser, buffer, stop_event))
    thread.start()

    # Configurar gráfico
    plt.style.use('seaborn-v0_8-darkgrid')
    fig, ax = plt.subplots()
    ax.set_title("Lectura en tiempo real del ADC")
    ax.set_xlabel("Muestra")
    ax.set_ylabel("Valor ADC (0–4095)")
    ax.set_ylim(0, 4095)
    ax.set_xlim(0, BUFFER_SIZE)
    line, = ax.plot([], [], marker="o", linestyle="None", markersize=3, color="tab:blue")

    # Animación en tiempo real
    ani = animation.FuncAnimation(fig, update_plot, fargs=(line, buffer),
                                  interval=50, blit=True)

    try:
        plt.show()
    finally:
        # Detener hilo al cerrar ventana
        stop_event.set()
        thread.join()
        ser.close()
        print("Conexión cerrada correctamente.")

# ================================================================
# Entrada del programa
# ================================================================
if __name__ == "__main__":
    main()
