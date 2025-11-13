#!/usr/bin/env python3
import serial as s #PySerial para la comunicacion UART
import threading
import struct
import matplotlib.pyplot as plt
import numpy as np
from collections import deque
import config

puerto = "/dev/ttyUSB0" # nombre del puerto donde esta conectado la lpc
baudios = 115000 # debe coincidir con la configuracion de uart en la lpc
sample = 100
muestreando = False # no se esta muestreando -> Bandera global


# Cuando nosotros transmitimos usando UART enviamos todos los caractres del comando
# En C nosotros tomamos estos caracteres que nos llegan y los almacenamos hasta encontrar un \n


lpc_uart = s.Serial(puerto,baudios,timeout=1) # abro el puerto, le digo que espere 1 segundo
data_buffer = deque(maxlen=config.sample) # creamos un buffer circular de 100 muestras
# time.sleep(2) # duermo para que se termine de configurar todo

# Esta funcion correra en un hilo demonio
def recibirDatos():
    while 1:
        if muestreando:
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

