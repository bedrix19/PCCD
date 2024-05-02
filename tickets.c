#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <pthread.h>
#include <semaphore.h>

#define nUsuarios 4     // Cantidad de Nodos
#define nHilos 10       // Cantidad de procesos 


/*  TypeDefs

    DatosProceso --> Informacion del Proceso
    DatosHilo    --> Sincronizacion Nodo-Proceso
*/
typedef struct {

    long mtype;
    float ticket;

    int idOrigen;

} datosProceso;


typedef struct {

    sem_t* semaforoEntrada;
    sem_t* semaforoSalida;

    int numeroHilo;

    int priority;

} datosHilo;



sem_t disputaSC, entradaSC, exclusionMutua;

float ticket, minTicket = 0;

int miID, 
    idNodos [nUsuarios - 1],
    colaNodos [nUsuarios - 1],
    quieroEntrar = 0,
    pendientes = 0,
    esperandoSC = 0,
    enSC = 0
;

key_t clave;


void* receive();
void* funcionProceso (datosHilo*);
float max (float, float);


int main (int argc, char* argv[]) { 

    if (argc != 7) {

        printf ("\nError durante el inicio del programa. Argumentos de entrada incorrectos.\n");
        printf ("Uso: %s <claveKey_t> <numeroConsultas> <numeroReservas> <numeroPagos> <numeroAdmin> <numeroAnulaciones>\n\n", argv[0]);

        exit (-1);

    }
    else {

        int consultas = atoi(argv[2]);
        int reservas = atoi(argv[3]);
        int pagos = atoi(argv[4]);
        int admin = atoi(argv[5]);
        int anulaciones = atoi(argv[6]);

        int suma = consultas + reservas + pagos + admin + anulaciones;

        //printf ("Procesos totales: %i\n\n", suma);

        if (suma != 10) {

            printf ("Deben ser 10 procesos. Cerrando programa.\n\n");
            exit (-1);

        }

        

    }

    printf ("Clave: %s\n\n", argv[1]);

    printf ("Numero de Consultas: %3s\n", argv[2]);
    printf ("Numero de Reservas: %4s\n", argv[3]);
    printf ("Numero de Pagos: %7s\n", argv[4]);
    printf ("Numero de Admin: %7s\n", argv[5]);
    printf ("Numero de Anulaciones: %s\n\n", argv[6]);

    
    clave = atoi(argv[1]);

    pthread_t idHilo [nHilos + 1];              // Todos los procesos + receiver
    sem_t semaforoSincronizacion [nHilos * 2];  // Agrupa semaforos de entrada y salida de datosHilo
    int opcion;

    datosHilo argHilo[nHilos];


    miID = msgget (clave, IPC_CREAT | 0777);


    if (miID == -1) {

       printf ("Error al crear mi buzon. Cerrando programa.\n\n");
        exit(-1);

    }

    printf ("Buzon con ID %i, creado exitosamente.\n", miID);


    int error;

    error = sem_init (&disputaSC, 0, 0);

    if (error == -1) {

        printf ("Error inicializando el semaforo de disputa.\n\n");
        exit(-1);

    }

    error = sem_init (&entradaSC, 0, 0);
    
    if ( error == -1) {

        printf ("Error inicializando el semaforo de entrada.\n\n");
        exit(-1);
        
    }

    error = sem_init (&exclusionMutua, 0, 1);

    if ( error == -1) {

        printf ("Error inicializando el semaforo de exclusion.\n\n");
        exit(-1);
        
    }


    error = 0;

    for (int i = 0; i < (nHilos * 2); i++) {

        error += sem_init (&semaforoSincronizacion[i], 0, 0);

    }

    if (error != 0) {

        printf ("Error al crear los semaforos de sincronizacion.\n\n");
        exit(-1);

    }

    for (int i = 0; i < (nUsuarios - 1); i++) {

        printf ("Introduzca la ID del usuario %i: ", i);
        scanf ("%i", &idNodos [i]);

    }

    // Inicializamos el proceso receptor
    error = pthread_create (&idHilo[0], NULL, receive, NULL);

    if (error == -1) {

        printf ("Error al crear el hilo receptor.\n\n");
        exit(-1);

    }

    int consultas = atoi(argv[2]);
    int reservas = atoi(argv[3]);
    int pagos = atoi(argv[4]);
    int admin = atoi(argv[5]);
    int anulaciones = atoi(argv[6]);

    for (int i = 0; i < consultas; i++) {

        //printf ("Contador: %i\n", i);

        argHilo[i].numeroHilo = i;
        argHilo[i].semaforoEntrada = &semaforoSincronizacion[i];
        argHilo[i].semaforoSalida = &semaforoSincronizacion[i + nHilos];

        argHilo[i].priority = 1;

        error = pthread_create (&idHilo[i+1], NULL, (void*) funcionProceso, &argHilo[i]);

        if (error == -1) {

            printf ("Error al crear el hilo %i. Cerrando programa.\n\n", i);
            exit(-1);

        }

    }

    reservas += consultas;

    for (int i = consultas; i < reservas; i++) {

        //printf ("Contador: %i\n", i);

        argHilo[i].numeroHilo = i;
        argHilo[i].semaforoEntrada = &semaforoSincronizacion[i];
        argHilo[i].semaforoSalida = &semaforoSincronizacion[i + nHilos];

        argHilo[i].priority = 1;

        error = pthread_create (&idHilo[i+1], NULL, (void*) funcionProceso, &argHilo[i]);

        if (error == -1) {

            printf ("Error al crear el hilo %i. Cerrando programa.\n\n", i);
            exit(-1);

        }

    }

    pagos += reservas;

    for (int i = reservas; i < pagos; i++) {

        //printf ("Contador: %i\n", i);

        argHilo[i].numeroHilo = i;
        argHilo[i].semaforoEntrada = &semaforoSincronizacion[i];
        argHilo[i].semaforoSalida = &semaforoSincronizacion[i + nHilos];

        argHilo[i].priority = 2;

        error = pthread_create (&idHilo[i+1], NULL, (void*) funcionProceso, &argHilo[i]);

        if (error == -1) {

            printf ("Error al crear el hilo %i. Cerrando programa.\n\n", i);
            exit(-1);

        }
    }

    admin += pagos;

    for (int i = pagos; i < admin; i++) {

        //printf ("Contador: %i\n", i);

        argHilo[i].numeroHilo = i;
        argHilo[i].semaforoEntrada = &semaforoSincronizacion[i];
        argHilo[i].semaforoSalida = &semaforoSincronizacion[i + nHilos];

        argHilo[i].priority = 2;

        error = pthread_create (&idHilo[i+1], NULL, (void*) funcionProceso, &argHilo[i]);

        if (error == -1) {

            printf ("Error al crear el hilo %i. Cerrando programa.\n\n", i);
            exit(-1);

        }

    }

    anulaciones += admin;

    for (int i = admin; i < anulaciones; i++) {

        //printf ("Contador: %i\n", i);

        argHilo[i].numeroHilo = i;
        argHilo[i].semaforoEntrada = &semaforoSincronizacion[i];
        argHilo[i].semaforoSalida = &semaforoSincronizacion[i + nHilos];

        argHilo[i].priority = 3;

        error = pthread_create (&idHilo[i+1], NULL, (void*) funcionProceso, &argHilo[i]);

        if (error == -1) {

            printf ("Error al crear el hilo %i. Cerrando programa.\n\n", i);
            exit(-1);

        }

    }

    while (1) {

        sleep(1);

        printf ("\n");
        printf ("1. Introducir un proceso en la SC.\n");
        printf ("3. Salir del programa.\n");
        printf ("Elige una opcion: ");

        scanf ("%i", &opcion);

        switch (opcion) {

            case 1:

                printf ("Que hilo quire introducir en la Seccion Critica? ");
                scanf ("%i", &opcion);

                sem_post (&semaforoSincronizacion[opcion]);

                break;

            case 2:

                printf ("Que hilo quire sacar de la Seccion Critica? ");
                scanf ("%i", &opcion);

                //sem_post (&semaforoSincronizacion[opcion + nHilos]);

                break;                

            case 3:

                printf ("Cerrando programa.\n\n");
                
                do {

                    error = msgctl (miID, IPC_RMID, NULL);

                } while (error != 0);
                
                exit(1);

                break;

            default:

                printf ("Opcion no contemplada.\n");
                break;

        }

    }

    return 0;

}


void* receive () {

    int error, numeroConfirmaciones;
    datosProceso mensajeIn, mensajeOut;



    while (1) {

        error = msgrcv (miID, &mensajeIn, sizeof(mensajeIn), 0, 0);

        if (error == -1) {

            printf ("Error recibiendo le mensaje.\n\n");

        }
        else if (mensajeIn.mtype == 2) {

            numeroConfirmaciones++;

            if (numeroConfirmaciones == (nUsuarios - 1)) {
                // Tenemos todas las confirmaciones

                sem_post (&disputaSC);
                numeroConfirmaciones = 0;

            }
            
            printf ("Confirmacion recibida.\n");

        }
        else if (quieroEntrar == 1) {

            // Guardamos la peticion en la cola de pendientes

            colaNodos [pendientes] = mensajeIn.idOrigen;
            pendientes++;

        }
        else if ( (enSC == 0) && ((quieroEntrar == 0) || (ticket > mensajeIn.ticket || (ticket == mensajeIn.ticket && miID > mensajeIn.idOrigen))) ) {

            minTicket = max (minTicket, mensajeIn.ticket);

            mensajeOut.idOrigen = miID;
            mensajeOut.ticket = 0;
            mensajeOut.mtype = 2;


            do {

                error = msgsnd (mensajeIn.idOrigen, &mensajeOut, sizeof(mensajeOut), 0);

                if (error == -1) {

                    printf ("Error enviando un mensaje.\n\n");
                    exit(-1);

                }

                printf ("Confirmacion enviada");

            } while (error != 0);

        }

    }
}

void* funcionProceso (datosHilo* entrada) {

    datosProceso mensaje;

    int error;

    printf ("Hilo %i, con prioridad %i, creado correctamente.\n", entrada->numeroHilo, entrada->priority);

    while (1) {

        printf ("[H: %i, P: %i] - Esperando...\n", entrada->numeroHilo, entrada->priority);
        sem_wait(entrada->semaforoEntrada);
        
        // Semaforo Contador (Veces que puede acceder a la seccion critica)
        //sem_wait (&contSC);
        sem_wait (&exclusionMutua);
        
        esperandoSC++;


        if ( (esperandoSC == 1) && (enSC == 0) ) {

            sem_post (&exclusionMutua);
            printf ("[H: %i, P: %i] - Disputando la SC.\n", entrada->numeroHilo, entrada->priority);

            srand ((unsigned) time (NULL));
            ticket = minTicket + (float) ((rand () % 1000) / 1000.0f);
            printf ("[H: %i, P: %i] - Ticket creado: %0.3f.\n", entrada->numeroHilo, entrada->priority, ticket);

            minTicket = max (minTicket, ticket);
            
            quieroEntrar = 1;

            mensaje.idOrigen = miID;
            mensaje.mtype = 1;
            mensaje.ticket = ticket;


            for (int i = 0; i < (nUsuarios - 1); i++) {

                do {

                    error = msgsnd (idNodos[i], &mensaje, sizeof(mensaje), 0);

                    if (error == -1) {

                        printf ("Error al enviar peticion. Reintentando.\n");

                    }

                } while (error != 0);

            }

            printf ("[H: %i, P: %i] - Mensajes enviados.\n", entrada->numeroHilo, entrada->priority);

            sem_wait(&disputaSC);
            printf ("[H: %i, P: %i] - Se han recibido todas las confirmaciones.\n", entrada->numeroHilo, entrada->priority);

        }
        else {

            sem_post (&exclusionMutua);
            printf ("[H: %i, P: %i] - Disputando la SC.\n", entrada->numeroHilo, entrada->priority);
            sem_wait (&entradaSC);

        }

        sem_wait (&exclusionMutua);
        esperandoSC--;
        enSC++;

        sem_post (&exclusionMutua);
        printf ("[H: %i, P: %i] - Entrada en la SC.\n", entrada->numeroHilo, entrada->priority);

        sleep(5);
        //sem_wait (entrada->semaforoSalida);

        printf ("[H: %i, P: %i] - Saliendo de la SC.\n", entrada->numeroHilo, entrada->priority);
        sem_wait(&exclusionMutua);

        enSC--;

        if (esperandoSC == 0) {

            sem_post (&exclusionMutua);

            /*
            for (int i = 0; i < maxPasesSC; i++) {

                sem_post (&contSC);

            }
            */

            quieroEntrar = 0;

            mensaje.idOrigen = miID;
            mensaje.mtype = 2;
            mensaje.ticket = 0;
            

            for (int i = 0; i < pendientes; i++) {

                do {
                    
                    error = msgsnd (colaNodos[i], &mensaje, sizeof(mensaje), 0);

                } while (error != 0);

            }

            printf ("[H: %i, P: %i] - Confirmaciones pendientes enviadas.\n", entrada->numeroHilo, entrada->priority);

        }
        else {

            sem_post (&exclusionMutua);
            sem_post (&entradaSC);

        }

    }

}



float max (float n1, float n2) {
    return (n1 > n2) ? n1 : n2;
}