import serial as s #PySerial para la comunicacion UART
import threading
import struct
import matplotlib.pyplot as plt
import numpy as np
from collections import deque
import time
import os
import config
os.system('cls')


# Cuando nosotros transmitimos usando UART enviamos todos los caractres del comando
# En C nosotros tomamos estos caracteres que nos llegan y los almacenamos hasta encontrar un \n


lpc_uart = s.Serial(config.puerto,config.baudios,timeout=1) # abro el puerto, le digo que espere 1 segundo
data_buffer = deque(maxlen=config.sample) # creamos un buffer circular de 100 muestras
# time.sleep(2) # duermo para que se termine de configurar todo

# Esta funcion correra en un hilo demonio
def recibirDatos():
    while 1:
        if config.muestreando:
            lectura = lpc_uart.read(2) # se leen 2 bytes o los que lleguen antes de 1 segundo (timeout)
            if len(lectura) == 2:
                val_uint16 = struct.unpack('<H',lectura)[0] # se convierte a un uint16_t
                data_buffer.append(val_uint16)

hilo = threading.Thread(target=recibirDatos,daemon=True) # creamos un hilo deamon que ejecuta recibirDatos 
hilo.start()

# configuracion del plot
plt.ion() # modo interactivo
fig,ax = plt.subplots()
line,=ax.plot([],[])
ax.set_ylim(0,2**12)
ax.set_xlim(0,config.sample)

def actualizarPlot():
    if len(data_buffer) >0:
        y = np.array(data_buffer) # eje y 
        x = np.array(len(y)) # eje x
        line.set_data(x,y)
        plt.pause(0.001)
        fig.canvas.draw()

def enviar_Comandos(cmd):
    lpc_uart.write((cmd + '\n').encode())
    print(f"Osc> Enviado: {cmd}")

def menu():
    while 1:
        print("--- Osciloscopio ---")
        print("Osc> 1) Establecer frecuencia")
        print("Osc> 2) Iniciar conversion")
        print("Osc> 3) Detener conversion")
        print("Osc> 4) Generar Seno por Software")
        print("Osc> 5) Generar Rampa por Software")
        print("Osc> 6) Salir")
        op = int(input("Osc> Ingrese una opcion [1,2,3,4,5,6]: \nOsc>"))
        if op == 1:
            frec = input("Frecuencia en Hz: ")
            print(f"Osc> Estableciendo frecuencia en {frec}[Hz]")
            enviar_Comandos(frec)
        elif op == 2:
            config.muestreando = True
            enviar_Comandos("START_ADC")
        elif op == 3:
            config.muestreando = False
            enviar_Comandos("STOP_ADC")
        elif op == 4:
            enviar_Comandos("GEN_SIN")
        elif op == 5:
            enviar_Comandos("GEN_RAM")
        elif op == 6:
            print("Osc> Saliendo")
            lpc_uart.close() # cierro puerto serial
            plt.close('all') # cierro el plot
            break

if __name__ == "__main__":
    menu()

