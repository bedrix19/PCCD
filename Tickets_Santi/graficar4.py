import matplotlib.pyplot as plt

# Función para cargar los datos de un archivo
def cargar_datos(nombre_archivo):
    x = []
    y = []
    with open(nombre_archivo, 'r') as file:
        for line in file:
            data = line.split()
            x.append(float(data[0]))
            y.append(float(data[1]))
    return x, y

# Cargar los datos de los archivos
archivos = ['MensajesNodo0.txt', 'MensajesNodo1.txt', 'MensajesNodo2.txt', 'MensajesNodo3.txt']
nombres = ['Nodo 0', 'Nodo 1', 'Nodo 2', 'Nodo 3']
colores = ['b', 'g', 'r', 'c']

# Crear la gráfica de barras
plt.figure(figsize=(8, 6))
for nombre, archivo, color in zip(nombres, archivos, colores):
    x, y = cargar_datos(archivo)
    plt.bar(x, y, color=color, label=nombre)

plt.xlabel('Prioridades')
plt.ylabel('Nº Mensajes')
plt.title('Numero de Mensajes por Nodo')
plt.grid(True)
plt.xticks(range(0, len(nombres)), nombres)
plt.legend()

plt.show()
