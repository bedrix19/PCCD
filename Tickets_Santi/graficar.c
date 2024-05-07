#include <stdio.h>
#include <stdlib.h>

int main() {
    FILE *gnuplotPipe = popen("gnuplot -p", "w");
    if (gnuplotPipe == NULL) {
        printf("Error abriendo Gnuplot.\n");
        return 1;
    }

    // Especifica el archivo de datos que quieres graficar
    //fprintf(gnuplotPipe, "set xdata time\n");
    //fprintf(gnuplotPipe, "set timefmt '%s.%3S'\n");
    fprintf(gnuplotPipe, "set ylabel 'Tiempo'\n");
    fprintf(gnuplotPipe, "set autoscale\n");
    fprintf(gnuplotPipe, "set terminal png\n");
    fprintf(gnuplotPipe, "set output 'TiempoVidaNodo53.png'\n");
    //fprintf(gnuplotPipe, "set label 1 'Suma: ' +string(suma)+");
    //fprintf(gnuplotPipe, "set dotsize 5\n");
    fprintf(gnuplotPipe, "plot 'TiempoVidaNodo53.txt' using 1:2 with points\n");
    fflush(gnuplotPipe);
    pclose(gnuplotPipe);
    return 0;
}