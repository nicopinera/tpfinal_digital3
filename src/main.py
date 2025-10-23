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

'''
Cuando nosotros transmitimos usando UART enviamos todos los caractres del comando
En C nosotros tomamos estos caracteres que nos llegan y los almacenamos hasta encontrar un \n
'''

lpc_uart = s.Serial(config.puerto,config.baudios,timeout=1) # abro el puerto, le digo que espere 1 segundo
data_buffer = deque(maxlen=config.sample) # creamos un buffer circular de 100 muestras
time.sleep(2) # duermo para que se termine de configurar todo

def menu():
    opcion = 0
    print("--- Menu Osciloscopio --- \n")
    print("1) Establecer Frecuencia")
    print("2) Muestrear señales")
    print("3) Generar señal Seno")
    print("4) Generar señal rampa")
    print("5) Salir \n")
    opcion = int(input("Ingrese opcion [1,2,3,4,5]: "))
    return opcion

# Esta funcion correra en un hilo demonio
def recibirDatos():
    while 1:
        if config.muestreando:
            lectura = lpc_uart.read(2) # se leen 2 bytes o los que lleguen antes de 1 segundo (timeout)
            if len(lectura) == 2:
                val_uint16 = struct.unpack('<H',lectura)[0] # se convierte a un uint16_t
                data_buffer.append(val_uint16)

threading.Thread(target=recibirDatos,daemon=True).start() # creamos un hilo deamon que ejecuta recibirDatos 

# configuracion del plot
plt.ion() # modo interactivo
fig,ax = plt.subplots()
line,=ax.prot([],[])
ax.set_ylim(0,2**12)
ax.set_xlim(0,config.sample)

def actualizarPlot():
    if len(data_buffer) >0:
        y = np.array(data_buffer) # eje y 
        x = np.array(len(y)) # eje x
        line.set_data(x,y)
        plt.pause(0.001)
        fig.canvas.draw()

def establecerFrecuencia():
    frec = int(input("Frecuencia en Hz: "))
    print(f"Estableciendo frecuencia en {frec}[Hz]")

def muestrear():
    print("Muestreando señal")

def senalSeno():
    print("Generando Señal Seno")

def senalRampa():
    print("Generando señal Rampa")

# Loop principal
while(1):
    op = menu()
    if(op == 1):
        establecerFrecuencia()
        continue
    elif op==2 :
        muestrear()
        continue
    elif op == 3:
        senalSeno()
        continue
    elif op == 4:
        senalRampa()
        continue
    elif op == 5:
        print("--- Saliendo ---")
        break
    else:
        print("Opcion invalida")
        
