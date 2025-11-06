# Configuracion previa
puerto = "COM1" # nombre del puerto donde esta conectado la lpc
baudios = 9600 # debe coincidir con la configuracion de uart en la lpc
sample = 100
muestreando = False # no se esta muestreando -> Bandera global

for i in range(60):
    valor = (1023/3)*(i/16)
    print(valor)