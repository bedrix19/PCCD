#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/msg.h>
#include <time.h> //para el rand

#define MAX_PROCESOS 10
#define MAX_NODOS 4
#define NUM_PRIORIDADES 3
#define SOLICITUD 1
#define CONFIRMACION 2
#define maxLength 100

typedef struct {
    int mtype; //1=SOLICITUD   2=CONFIRMACION
    int id_nodo_origen;
    int ticket_origen;
    int prioridad_origen;
    int flag_consulta;
} mssg_ticket;

//Struct para pasarle parámetros a los procesos Escritores
struct arg_servidor {
    int prioridad;
    int nro_proceso;
};

struct arg_escritor {
    int prioridad;
    int nro_proceso;
} *argsEscritores;

struct arg_lector {
    int prioridad;
    int nro_proceso;
} *argsLectores;

int mi_id;
int estoy_SC = 0;
int quiero = 0;
int max_ticket = 0;
int mi_ticket = 0;
int mi_prioridad = 0;
int confirmaciones = 0;
int vector_peticiones[NUM_PRIORIDADES] = {0};   //ya esta protegido por el semaforo de tickets
//int vector_peticiones[MAX_PROCESOS];
int flag_pedir_otra_vez[NUM_PRIORIDADES] = {0};
int flag_esperando_confirmacion[NUM_PRIORIDADES] = {0};
//int flag_pedir_otra_vez[MAX_PROCESOS] = {0};
int nodos_pendientes_count;
int *id_nodos_pend = NULL;  //ya esta protegido por el semaforo de nodos_pendientes_count
int *ticket_nodos_pend = NULL;  //ya esta protegido por el semaforo de nodos_pendientes_count
int SC_consultas = 0;
int contadorLectores = 0;
int flag_solicitudes_pendientes = 0;  //ya esta protegido por el semaforo de nodos_pendientes_count
//faltan semaforos?
int msqid_nodos[MAX_NODOS];
int flag_esperando_para_pedir_SC[NUM_PRIORIDADES] = {0};

//semaforos de sincronización => sem(0,1)
sem_t sem_solicitar_SC[MAX_PROCESOS];
sem_t semaforos_de_paso[MAX_PROCESOS];
sem_t sem_esperando_pedir_SC[NUM_PRIORIDADES];
sem_t sem_exclusion_peticiones[NUM_PRIORIDADES];
sem_t sem_exclusion_nodo;

//semaforos para proteger variables => sem(1,1)
sem_t sem_exclusionMutuaEscritor;
sem_t sem_nodos_pendientes_count;
sem_t sem_estoy_SC_y_quiero;
sem_t sem_flag_pedir_again;
sem_t sem_ProtegeLectores;
sem_t sem_mi_prioridad;
sem_t sem_tickets;

int MAX(int a, int b) {
    return (a > b) ? a : b;
}

void dar_SC(int ticket) {
    for (int i = 0; i < NUM_PRIORIDADES; i++) {
        if (vector_peticiones[i] == ticket) {
            flag_pedir_otra_vez[i] = 0;
            sem_post(&semaforos_de_paso[i]); // Damos el paso al que hizo la solicitud
        }
    }
}

void solicitar_SC(int num_proceso, int prioridad_solicitud, int flag_consulta) {
    printf("[Proceso %d] => Solicitando SC\n",num_proceso);
    while(1) {
        sem_wait(&sem_estoy_SC_y_quiero);
        quiero = 1;
        sem_post(&sem_estoy_SC_y_quiero);

        sem_wait(&sem_mi_prioridad); //printf("\n[Proceso %d]=> paso sem_mi_prioridad\n",num_proceso);
        //printf("\n[Proceso %d]=>prioridad actual: %d\n",num_proceso, mi_prioridad);
        if (prioridad_solicitud > mi_prioridad){
            if(mi_prioridad!=0) {
                confirmaciones = 0;
                flag_pedir_otra_vez[mi_prioridad-1]=1;
                sem_post(&semaforos_de_paso[mi_prioridad-1]);
            }
            mi_prioridad = prioridad_solicitud; //printf("\n[Proceso %d]=> mi_prioridad: %d\n",num_proceso,mi_prioridad);
            sem_post(&sem_mi_prioridad);
        } else {
            sem_post(&sem_mi_prioridad);
            flag_esperando_para_pedir_SC[prioridad_solicitud-1]=1;
            sem_wait(&sem_esperando_pedir_SC[prioridad_solicitud-1]);
            flag_esperando_para_pedir_SC[prioridad_solicitud-1]=0;
        }
        
        // Preparamos las solicitudes
        sem_wait(&sem_tickets);
        mi_ticket = max_ticket + 1;
        //printf("\n[Proceso %d] => ticket actual: %d\n",num_proceso, mi_ticket);
        vector_peticiones[prioridad_solicitud-1] = mi_ticket;
        mssg_ticket solicitud;
        size_t buf_length = sizeof(mssg_ticket) - sizeof(int); //tamaño del mensaje - mtype
        solicitud.mtype = SOLICITUD;
        solicitud.id_nodo_origen = mi_id;
        solicitud.ticket_origen = mi_ticket;
        solicitud.prioridad_origen = prioridad_solicitud;
        solicitud.flag_consulta = flag_consulta;
        for (int i = 0; i < MAX_NODOS; i++) {
            if (i==mi_id) continue;
            if (msgsnd(msqid_nodos[i], &solicitud, buf_length, IPC_NOWAIT) < 0)
                printf("\nError con msgsnd solicitando SC a los nodos: %s\n", strerror(errno));
            else {
                //printf("Se envió la solicitud: [destino: %d, ticket: %d, prioridad: %d, flag consultas: %d]\n", i, solicitud.ticket_origen, solicitud.prioridad_origen, flag_consulta);
            }
        }
        sem_post(&sem_tickets);

        //flag_esperando_confirmacion[prioridad_solicitud-1]=1;
        sem_wait(&semaforos_de_paso[prioridad_solicitud-1]); // Esperamos a que dar_SC nos de el paso

        sem_wait(&sem_flag_pedir_again);
        if(!flag_pedir_otra_vez[prioridad_solicitud-1]){
            sem_post(&sem_flag_pedir_again);
            break;
        }
        sem_post(&sem_flag_pedir_again);
    }
}

void liberar_SC() {
    sem_wait(&sem_estoy_SC_y_quiero);
    quiero = 0;
    estoy_SC = 0;
    sem_post(&sem_estoy_SC_y_quiero);

    //Comprobar si hay procesos esperando por solicitar SC
    for(int i = NUM_PRIORIDADES;i>0;i--){
        if(flag_esperando_para_pedir_SC[i-1]){
            sem_post(&sem_esperando_pedir_SC[i-1]);
            break;
        }
    }

    // Preparamos las confirmaciones
    sem_wait(&sem_nodos_pendientes_count);
    if(flag_solicitudes_pendientes){
        mssg_ticket confirmacion;
        size_t buf_length = sizeof(mssg_ticket) - sizeof(int); //tamaño del mensaje - mtype
        confirmacion.mtype = CONFIRMACION;
        confirmacion.id_nodo_origen = mi_id;
            for (int i = 0; i < nodos_pendientes_count; i++) {
                printf("Confirmamos al ticket: %d del nodo: %d\n",ticket_nodos_pend[i],id_nodos_pend[i]);
                confirmacion.ticket_origen = ticket_nodos_pend[i];
                if (msgsnd(msqid_nodos[id_nodos_pend[i]], &confirmacion, buf_length, IPC_NOWAIT) < 0) {
                    printf("\nid del nodo: %d",id_nodos_pend[i]);
                    printf("\nError con msgsnd respondiendo solicitudes después de SC: %s\n", strerror(errno));
                }
            }
            flag_solicitudes_pendientes = 0;
            nodos_pendientes_count = 0;
        }
    sem_post(&sem_nodos_pendientes_count);
}

void *receiver(void *arg) {

    int *msqid = (int *)arg;
    mssg_ticket mensaje;
    size_t buf_length = sizeof(mssg_ticket) - sizeof(int); //tamaño del mensaje - mtype

    //variables para contar las confirmaciones que recibimos y entrar en SC
    confirmaciones = 0;

    //variables para almacenar las solicitudes
    int capacidad = MAX_NODOS; // Capacidad inicial del vector
    nodos_pendientes_count = 0;

    if ((id_nodos_pend = (int *)malloc(capacidad * sizeof(int))) == NULL) {//asignar la memoria para el vector de ids
        printf("\nError al asignar memoria.\n");
        exit(1);
    }
    if ((ticket_nodos_pend = (int *)malloc(capacidad * sizeof(int))) == NULL) {//asignar la memoria para el vector de tickets
        printf("\nError al asignar memoria.\n");
        exit(1);
    }

    while (1) {
        int nodo_destino;
        //printf("\nEsperando recibir mensajes\n");
        if(msgrcv(*msqid, (void *)&mensaje, buf_length, 0, 0) < 0){
            printf("\nError con msgrcv: %s\n", strerror(errno));
            exit(1);
        } else {
            if(mensaje.mtype == SOLICITUD){
                nodo_destino = mensaje.id_nodo_origen;
                //printf("\nRecibí una solicitud con ticket igual a %d\n",mensaje.ticket_origen);
            }
            if(mensaje.mtype == CONFIRMACION) {
                //printf("\nRecibí una confirmacion con ticket igual a %d\n",mensaje.ticket_origen);
            }
        }
        sem_wait(&sem_tickets);  //printf("paso sem_tickets ");
        sem_wait(&sem_mi_prioridad); //printf("paso sem_mi_prioridad ");
        max_ticket = MAX(max_ticket, mensaje.ticket_origen);
        if (mensaje.mtype == CONFIRMACION && mensaje.ticket_origen == mi_ticket) {
            confirmaciones++;
            if (confirmaciones == MAX_NODOS-1) {
                mi_prioridad = 0;
                confirmaciones = 0;
                estoy_SC = 1;
                sem_wait(&sem_flag_pedir_again);
                //printf("paso sem_flag_pedir_again ");
                dar_SC(mensaje.ticket_origen);
                sem_post(&sem_flag_pedir_again);
            }
            sem_post(&sem_mi_prioridad);
            sem_post(&sem_tickets);
            continue;
        }
        
        sem_wait(&sem_estoy_SC_y_quiero); //printf("paso sem_estoy_SC_y_quiero ");
        //sem_wait(&sem_tickets);
        //sem_wait(&sem_mi_prioridad);
        sem_wait(&sem_ProtegeLectores); //printf("paso sem_ProtegeLectores ");
        if (mensaje.mtype == SOLICITUD && !quiero) {
            //printf("Respondemos, porque no queremos SC.\n");
            mensaje.mtype = CONFIRMACION;
            mensaje.id_nodo_origen = mi_id;
            // mensaje.ticket_origen = mensaje.ticket_origen;
            if (msgsnd(msqid_nodos[nodo_destino], &mensaje, buf_length, IPC_NOWAIT) < 0)
                printf("\nError con msgsnd respondiendo a una solicitud: %s\n", strerror(errno));
        } else if (mensaje.mtype == SOLICITUD && quiero && !estoy_SC) {
            if (mensaje.prioridad_origen > mi_prioridad) {
                //printf("Respondemos, porque tiene una prioridad mayor.\n");
                // Aquí se implementa el envío de confirmación para una peticion mas prioritaria
                mensaje.mtype = CONFIRMACION;
                mensaje.id_nodo_origen = mi_id;
                // mensaje.ticket_origen = mensaje.ticket_origen;
                if (msgsnd(msqid_nodos[nodo_destino], &mensaje, buf_length, IPC_NOWAIT) < 0)
                    //printf("\nError con msgsnd respondiendo a una solicitud más prioritaria: %s\n", strerror(errno));

                // como es una solicitud con prioridad mayor debemos enviar el nuestro otra vez
                sem_wait(&sem_flag_pedir_again);
                for(int i=0;i<NUM_PRIORIDADES;i++){
                    if(vector_peticiones[i]==mi_ticket){
                        flag_pedir_otra_vez[i] = 1;
                        sem_post(&semaforos_de_paso[i]); //lo despertamos pero debe pedir otra vez a todos
                    }
                }
                sem_post(&sem_flag_pedir_again);
            } else if (mensaje.prioridad_origen == mi_prioridad && (mensaje.ticket_origen < mi_ticket || (mensaje.ticket_origen == mi_ticket && mensaje.id_nodo_origen < mi_id))) {
                //printf("Respondemos, porque es un nodo más prioritario.\n");
                // Aquí se implementa el envío de confirmación para un nodo mas prioritario
                mensaje.mtype = CONFIRMACION;
                mensaje.id_nodo_origen = mi_id;
                // mensaje.ticket_origen = mensaje.ticket_origen;
                if (msgsnd(msqid_nodos[nodo_destino], &mensaje, buf_length, IPC_NOWAIT) < 0)
                    printf("\nError con msgsnd respondiendo a un nodo más prioritario: %s\n", strerror(errno));
            } else {
                //printf("Almacenamos la solicitud.\n");
                // Aquí se implementa la lógica de almacenamiento de nodos pendientes
                sem_wait(&sem_nodos_pendientes_count);
                flag_solicitudes_pendientes = 1;
                if(nodos_pendientes_count >= capacidad){
                    capacidad = capacidad + MAX_NODOS;
                    if ((id_nodos_pend = (int *)realloc(id_nodos_pend, capacidad * sizeof(int))) == NULL) {//aumentamos la memoria
                        printf("\nError al asignar memoria.\n");
                        exit(1);
                    }
                    if ((ticket_nodos_pend = (int *)realloc(ticket_nodos_pend, capacidad * sizeof(int))) == NULL) {//aumentamos la memoria
                        printf("\nError al asignar memoria.\n");
                        exit(1);
                    }
                }  
                id_nodos_pend[nodos_pendientes_count] = mensaje.id_nodo_origen;
                ticket_nodos_pend[nodos_pendientes_count] = mensaje.ticket_origen;
                nodos_pendientes_count++;
                sem_post(&sem_nodos_pendientes_count);
            }
        } else if (mensaje.mtype == SOLICITUD && estoy_SC && mensaje.flag_consulta && SC_consultas) {
            //printf("Respondemos, porque es una consulta y estamos en SC con consultas.\n");
            //sem_post(&sem_ProtegeLectores);
            // Aquí se implementa el envío de confirmación para una peticion para una Consulta
            mensaje.mtype = CONFIRMACION;
            mensaje.id_nodo_origen = mi_id;
            // mensaje.ticket_origen = mensaje.ticket_origen;
            if (msgsnd(msqid_nodos[nodo_destino], &mensaje, buf_length, IPC_NOWAIT) < 0)
                printf("\nError con msgsnd a solicitudes de Consultas y yo estoy en SC consultas: %s\n", strerror(errno));
        } else {
            //printf("Almacenamos la solicitud.\n");
            // Aquí se implementa la lógica de almacenamiento de nodos pendientes
            sem_wait(&sem_nodos_pendientes_count);
            flag_solicitudes_pendientes = 1;
            if(nodos_pendientes_count >= capacidad){
                capacidad = capacidad + MAX_NODOS;
                if ((id_nodos_pend = (int *)realloc(id_nodos_pend, capacidad * sizeof(int))) == NULL) {//aumentamos la memoria
                    printf("\nError al asignar memoria.\n");
                    exit(1);
                }
                if ((ticket_nodos_pend = (int *)realloc(ticket_nodos_pend, capacidad * sizeof(int))) == NULL) {//aumentamos la memoria
                    printf("\nError al asignar memoria.\n");
                    exit(1);
                }
            }  
            id_nodos_pend[nodos_pendientes_count] = mensaje.id_nodo_origen;
            ticket_nodos_pend[nodos_pendientes_count] = mensaje.ticket_origen;
            nodos_pendientes_count++;
            sem_post(&sem_nodos_pendientes_count);
        }
        sem_post(&sem_ProtegeLectores);
        sem_post(&sem_mi_prioridad);
        sem_post(&sem_tickets);
        sem_post(&sem_estoy_SC_y_quiero);
    }
}

void *lector(void *threadArgs){
    //args: prioridad, nro_proceso, sem_sinc
    struct arg_servidor *args = threadArgs;
    int prioridad = args->prioridad;
    int nro_proceso = args->nro_proceso;
    while(1){
        printf("[Proceso %d] => Esperando poder solicitar SC\n", nro_proceso);
        sem_wait(&sem_solicitar_SC[nro_proceso]);                      //Esperamos a que nos den paso desde el main
        printf("[Proceso %d] => Pidiendo SC...\n", nro_proceso);
        sem_wait(&sem_exclusion_peticiones[0]);
        solicitar_SC(nro_proceso,prioridad,1);
        sem_post(&sem_exclusion_peticiones[0]);
        printf("[Proceso %d] => Dentro de SC...\n", nro_proceso);
        sem_wait(&sem_ProtegeLectores);  //sem(0,1) para cambiar el valor de SC_consultas en exclusión mutua
        contadorLectores ++;
        if (contadorLectores == 1){
            //printf("\nnumero de consultas: %d",contadorLectores);
            sem_wait(&sem_exclusionMutuaEscritor);
            SC_consultas = 1;
        }
        sem_post(&sem_ProtegeLectores);
        sleep(7);
        printf("\n[Proceso %d] => Saliendo SC...\n", nro_proceso);
        sem_wait(&sem_ProtegeLectores);  //sem(0,1) para cambiar el valor de SC_consultas en exclusión mutua
        contadorLectores --;

        if (contadorLectores == 0){
        
            printf("\nNumero de consultas: %d\n",contadorLectores);
            liberar_SC();
            SC_consultas = 0;
            sem_post(&sem_exclusionMutuaEscritor);
        
        }
        
        sem_post(&sem_ProtegeLectores);
        
        //sem_wait(&semaforos_de_paso[nro_proceso]);
    }
}

void *escritor(void *threadArgs){
    struct arg_servidor *args = threadArgs;
    int prioridad = args->prioridad;
    int nro_proceso = args->nro_proceso;
    while(1){
        printf("[Proceso %d] => Esperando poder solicitar SC\n", nro_proceso);
        sem_wait(&sem_solicitar_SC[nro_proceso]);                      //Esperamos a que nos den paso desde el main
        printf("[Proceso %d] => Pidiendo SC...\n", nro_proceso);
        sem_wait(&sem_exclusion_peticiones[prioridad-1]);
        solicitar_SC(nro_proceso,prioridad,0);
        sem_post(&sem_exclusion_peticiones[prioridad-1]);
        sem_wait(&sem_exclusionMutuaEscritor);                          //Como el rcv manda peticiones mientras estamos en SC hay que poner un semáforo de Exclusión mutua
        printf("[Proceso %d] => Dentro de SC...\n", nro_proceso);
        sleep(7);
        liberar_SC();
        printf("\n[Proceso %d] => Saliendo de SC...\n", nro_proceso);
        sem_post(&sem_exclusionMutuaEscritor);
    }
}

int main(int argc, char *argv[]){
    if(argc != 2){
        printf("Uso: %s <Número de nodo>\n", argv[0]);
        printf("Primer nodo = 0 y el último = %d\n",MAX_NODOS-1);
        return 1;
    }

    mi_id = atoi(argv[1]);

    //inicializar las variables para el intercambio de mensajes
    if ((msqid_nodos[mi_id] = msgget(1069+mi_id, IPC_CREAT | IPC_EXCL | 0666)) < 0){
        printf("Error con msgget: %s\n", strerror(errno));
        if (errno == EEXIST) {
            msqid_nodos[mi_id] = msgget(1069+mi_id, 0);
            if (msgctl(msqid_nodos[mi_id], IPC_RMID, NULL) == -1) {
                perror("Error al eliminar la cola de mensajes existente");
                return 1;
            } else printf("Se elimino la cola de mensajes que ya existía\n");
            if ((msqid_nodos[mi_id] = msgget(1069+mi_id, IPC_CREAT | IPC_EXCL | 0666)) < 0){
                printf("Error con msgget: %s\n", strerror(errno));
                return 1;
            }
        }
    }
    printf("[Nodo %d] Pulsa ENTER cuando todos los nodos estén inicializados\n", mi_id);
    while(!getchar());
    for(int i=0;i<MAX_NODOS;i++){
        if(i==mi_id)continue;
        msqid_nodos[i] = msgget(1069+i, 0666); // Colas de mensajes de los nodos
    }

    //inicializar semaforos para sincronizar procesos servidor
    for(int i=0;i<MAX_PROCESOS;i++){
        if(sem_init(&sem_solicitar_SC[i],0,0)==-1)
            printf("Error con semáforo de paso para servidores: %s\n", strerror(errno));
    }

    //inicializar semaforos de sincronizacion cuando pedimos SC
    for(int i=0;i<MAX_PROCESOS;i++){
        if(sem_init(&semaforos_de_paso[i],0,0)==-1)
            printf("Error con semáforo de paso para servidores: %s\n", strerror(errno));
    }

    //inicializar semaforos de sincronizacion cuando queremos pedir SC pero debemos esperar
    for(int i=0;i<NUM_PRIORIDADES;i++){
        if(sem_init(&sem_esperando_pedir_SC[i],0,0)==-1)
            printf("Error con semáforo de paso para servidores: %s\n", strerror(errno));
    }

    //inicializar semaforos de sincronizacion cuando queremos pedir SC pero debemos esperar
    for(int i=0;i<NUM_PRIORIDADES;i++){
        if(sem_init(&sem_exclusion_peticiones[i],0,1)==-1)
            printf("Error con semáforo de paso para servidores: %s\n", strerror(errno));
    }

    //incializar semaforo de exclusion mutua de procesos en el nodo
    if(sem_init(&sem_exclusionMutuaEscritor,0,1)==-1) printf("Error inicializando un semáforo");

    //inicializar los semáforos para proteger variables
    if(sem_init(&sem_nodos_pendientes_count,0,1)==-1) printf("Error inicializando un semáforo");
    if(sem_init(&sem_estoy_SC_y_quiero,0,1)==-1) printf("Error inicializando un semáforo");
    if(sem_init(&sem_flag_pedir_again,0,1)==-1) printf("Error inicializando un semáforo");
    if(sem_init(&sem_ProtegeLectores,0,1)==-1) printf("Error inicializando un semáforo");
    if(sem_init(&sem_mi_prioridad,0,1)==-1) printf("Error inicializando un semáforo");
    if(sem_init(&sem_tickets,0,1)==-1) printf("Error inicializando un semáforo");

    //inicializar los procesos servidores
    pthread_t hilos_servidores[MAX_PROCESOS];
    struct arg_servidor parametros;
    int opcion, proceso=0;

    // Inicializacion de la semilla.
    srand(time(NULL));

    do {
        /******* Menú de opciones *******/
        /*
        printf("\n\n\t\t\tMenu de Procesos\n");
        printf("\t\t\t----------------\n");
        printf("\t[1] Consultas\n");
        printf("\t[2] Reservas\n");
        printf("\t[3] Pagos\n");
        printf("\t[4] Administración\n");
        printf("\t[5] Anulaciones\n");

        printf("\n\tIngrese una opcion: ");
        scanf(" %d", &opcion);
        */
        opcion = rand() % 5 + 1;

        parametros.nro_proceso = proceso;
        switch (opcion) {
            case 1: 
                parametros.prioridad = 1;
                if ((pthread_create(&hilos_servidores[proceso], NULL, lector, (void *)&parametros)) < 0)//aqui
                    printf("Error con pthread_create: %s\n", strerror(errno));
                else printf("\t[%d] Consulta creado.\t\t",proceso);
                break;
            case 2: 
                parametros.prioridad = 1;
                if ((pthread_create(&hilos_servidores[proceso], NULL, escritor, (void *)&parametros)) < 0)//aqui
                    printf("Error con pthread_create: %s\n", strerror(errno));
                else printf("\t[%d] Reserva creado.\t\t",proceso);
                break;
            case 3: 
                parametros.prioridad = 2;
                if ((pthread_create(&hilos_servidores[proceso], NULL, escritor, (void *)&parametros)) < 0)//aqui
                    printf("Error con pthread_create: %s\n", strerror(errno));
                else printf("\t[%d] Pago creado.\t\t",proceso);
                break;
            case 4: 
                parametros.prioridad = 2;
                if ((pthread_create(&hilos_servidores[proceso], NULL, escritor, (void *)&parametros)) < 0)//aqui
                    printf("Error con pthread_create: %s\n", strerror(errno));
                else printf("\t[%d] Administración creado.\t",proceso);
                break;
            case 5: 
                parametros.prioridad = 3;
                if ((pthread_create(&hilos_servidores[proceso], NULL, escritor, (void *)&parametros)) < 0)//aqui
                    printf("Error con pthread_create: %s\n", strerror(errno));
                else printf("\t[%d] Anulaciones creado.\t\t",proceso);
                break;
            default:
                printf("Error con la elección\n");
                continue;
        }proceso++;
        sleep(1);
    }while (proceso<MAX_PROCESOS);

    //inicializamos el receiver
    pthread_t hilo_receptor;
    if ((pthread_create(&hilo_receptor, NULL, receiver, (void *)&msqid_nodos[mi_id])) < 0)
        printf("Error con pthread_create: %s\n", strerror(errno));
    else printf("\n\tReceiver creado.\n");

    //menú para pedir SC
    while (1) {
        sleep(1);

        printf ("\n");
        printf ("\t[1] Introducir un proceso en la SC.\n");
        //printf ("\t[2] Quitar un proceso de la SC.\n");
        printf ("\t[3] Salir del programa.\n");
        printf ("\t[4] Introducir una secuencia de procesos.\n");
        printf("\n\tIngrese una opcion: ");

        scanf ("%i", &opcion);

        switch (opcion) {

            case 1:
                printf ("\t¿Que hilo quire introducir en la Seccion Critica? ");
                scanf ("%i", &opcion);
                sem_post (&sem_solicitar_SC[opcion]);
                break;
            case 2:
                printf ("\tQue hilo quire sacar de la Seccion Critica? ");
                scanf ("%i", &opcion);
                //sem_post (&semaforoSincronizacion[opcion + nHilos]);
                break;
            case 3:
                printf ("\tCerrando programa.\n\n");
                if (msgctl(msqid_nodos[mi_id], IPC_RMID, NULL) == -1) perror("Error al eliminar la cola de mensajes");
                free(id_nodos_pend);
                free(ticket_nodos_pend);
                return 0;

            case 4:

                char linea[maxLength], buffer[256];
                int secuencia[maxLength];
                int  i = 0;

                printf ("Introduzca los IDs de los procesos en la secuencia deseada: \n");
                
                fgets (buffer, sizeof(buffer), stdin);
                fgets (linea, sizeof(linea), stdin);

                // Utilizamos strtok() para separar los números
                char *token = strtok(linea, " \n");
                while (token != NULL && i < maxLength) {
                    // Convertimos el token a entero y lo almacenamos en el array secuencia
                    secuencia[i] = atoi(token);
                    i++;
                    // Avanzamos al siguiente token
                    token = strtok(NULL, " \n");
                }

                for (int t = 0; t < i; t++) {

                    sem_post (&sem_solicitar_SC[secuencia[t]]);

                    sleep (2);

                }

                break;
            
            default:
                printf ("Opcion no contemplada.\n");
                break;
        }
    }
}