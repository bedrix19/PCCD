import matplotlib.pyplot as plt
import numpy as np

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
archivos = ['TiempoNoSC0.txt', 'TiempoNoSC1.txt', 'TiempoNoSC2.txt', 'TiempoNoSC3.txt']
nombres = ['Nodo 0', 'Nodo 1', 'Nodo 2', 'Nodo 3']
etiquetas_x = ['Consultas y reservas', 'Pagos y administración', 'Anulaciones']
colores = ['b', 'g', 'r', 'c']

# Crear la gráfica de líneas
plt.figure(figsize=(12, 6))
for nombre, archivo, color in zip(nombres, archivos, colores):
    x, y = cargar_datos(archivo)
    plt.plot(x, y, marker='o', linestyle='none', label=nombre, color=color)

plt.xlabel('Prioridades')
plt.ylabel('Y')
plt.title('Tiempo de No SC')
plt.grid(True)
plt.xticks(range(1, len(etiquetas_x) + 1), etiquetas_x)

plt.legend()

# Crear la gráfica de barras de las medias
plt.figure(figsize=(8, 6))
medias = [np.mean(cargar_datos(archivo)[1]) for archivo in archivos]
plt.bar(nombres, medias, color=colores)
plt.xlabel('Nodos')
plt.ylabel('Tiempo medio (s)')
plt.title('Tiempo medio de no SC por nodo')

plt.show()
