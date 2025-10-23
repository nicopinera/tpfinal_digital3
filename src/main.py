
def menu():
    opcion = 0
    print("--- Menu Osciloscopio --- \n")
    print("1) Muestrear señales")
    print("2) Generar señal Seno")
    print("3) Generar señal rampa")
    print("4) Salir \n")
    opcion = int(input("Ingrese opcion [1,2,3,4]: "))
    return opcion


while(1):
    op = menu()
    if(op == 4):
        print("Saliendo")
        break
    