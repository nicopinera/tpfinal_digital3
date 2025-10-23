import serial as s #PySerial para la comunicacion UART
import os
os.system('clear')

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

def establecerFrecuencia():
    frec = int(input("Frecuencia en Hz: "))
    print(f"Estableciendo frecuencia en {frec}[Hz]")

def muestrear():
    print("Muestreando señal")

def senalSeno():
    print("Generando Señal Seno")

def senalRampa():
    print("Generando señal Rampa")


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