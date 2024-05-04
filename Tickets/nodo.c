#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>



#define MAX_PROCESOS 10
#define MAX_NODOS 10
#define SOLICITUD 1
#define CONFIRMACION 2

typedef mssg_ticket {
    int mtype;  //1=SOLICITUD   2=CONFIRMACION
    int id_nodo_origen;
    int ticket_origen;
    int prioridad_origen;
    int flag_consulta;
};

//Struct para pasarle parámetros a los procesos Escritores
struct arg_escritor {
    int prioridad;
    int nro_proceso;
} *argsEscritores;

struct arg_lector {
    int prioridad;
    int nro_proceso;
} *argsLectores;

int confirmaciones = 0;
int num_nodos;
int num_procesos;
int estoy_SC = 0;
int max_ticket = 0;
int mi_ticket;
int mi_prioridad = 0;
int quiero = 0;
int SC_consultas;
int vector_peticiones[MAX_PROCESOS];
int flag_pedir_otra_vez[MAX_PROCESOS] = {0};
int nodos_pendientes_count;
int *id_nodos_pend = NULL;
int *ticket_nodos_pend = NULL;
int flagConsultasSC = 0;
int contadorLectores = 0;


sem_t semaforos_de_paso[MAX_PROCESOS];
sem_t sem_esperando_pedir_SC[3];
sem_t sem_exclusionMutuaEscritor;
sem_t sem_ProtegeLectores;

void *receiver(void *arg) {

    int *msqid = (int *)arg;
    printf("id:%d\n",*msqid);
    struct mssg_ticket mensaje;
    size_t buf_length = sizeof(struct mssg_ticket) - sizeof(int); //tamaño del mensaje - mtype

    //variables para almacenar las solicitudes
    int capacidad = MAX_NODOS; // Capacidad inicial del vector
    nodos_pendientes_count = 0;

    if ((id_nodos_pend = (int *)malloc(capacidad * sizeof(int))) == NULL) {//asignar la memoria para el vector de ids
        printf("Error al asignar memoria.\n");
        exit(1);
    }
    if ((ticket_nodos_pend = (int *)malloc(capacidad * sizeof(int))) == NULL) {//asignar la memoria para el vector de tickets
        printf("Error al asignar memoria.\n");
        exit(1);
    }

    while (1) {
        if(msgrcv(*msqid, (void *)&mensaje, buf_length, 0, 0) < 0){
            printf("Error con msgrcv: %s\n", strerror(errno));
            exit(1);
        }
        max_ticket = MAX(max_ticket, mensaje.ticket_origen);

        if (mensaje.mtype == CONFIRMACION && mensaje.ticket_origen == mi_ticket) {
            confirmaciones++;
            if (confirmaciones == num_nodos) {
                mi_prioridad = 0;
                confirmaciones = 0;
                dar_SC(mensaje.ticket_origen);
            }
        } else if (mensaje.tipo_origen == PETICION && !quiero) {
            mensaje.mtype = CONFIRMACION;
            mensaje.id_nodo_origen = mi_id;
            // mensaje.ticket_origen = mensaje.ticket_origen;
            if (msgsnd(msqid_nodos[mensaje.id_nodo_origen], &mensaje, msgsz, IPC_NOWAIT) < 0)
                printf("Error con msgsnd: %s\n", strerror(errno));
        } else if (mensaje.tipo_origen == PETICION && !estoy_SC) {
            if (mensaje.prioridad_origen > mi_prioridad) {
                // Aquí se implementa el envío de confirmación para una peticion mas prioritaria
                mensaje.mtype = CONFIRMACION;
                mensaje.id_nodo_origen = mi_id;
                // mensaje.ticket_origen = mensaje.ticket_origen;
                if (msgsnd(msqid_nodos[mensaje.id_nodo_origen], &mensaje, msgsz, IPC_NOWAIT) < 0)
                    printf("Error con msgsnd: %s\n", strerror(errno));
                for(int i=0;i<MAX_PROCESOS;i++){
                    if(vector_peticiones[i]==mi_ticket){
                        flag_pedir_otra_vez[i] = 1;
                        sem_post(semaforos_de_paso[i]); //lo despertamos pero debe pedir otra vez a todos
                    }
                }
            } else if (mensaje.prioridad_origen == mi_prioridad && (mensaje.ticket_origen < mi_ticket || (mensaje.ticket_origen == mi_ticket && mensaje.id_nodo_origen < mi_id))) {
                // Aquí se implementa el envío de confirmación para un nodo mas prioritario
                mensaje.mtype = CONFIRMACION;
                mensaje.id_nodo_origen = mi_id;
                // mensaje.ticket_origen = mensaje.ticket_origen;
                if (msgsnd(msqid_nodos[mensaje.id_nodo_origen], &mensaje, msgsz, IPC_NOWAIT) < 0)
                    printf("Error con msgsnd: %s\n", strerror(errno));
            } else {
                // Aquí se implementa la lógica de almacenamiento de nodos pendientes
                if(nodos_pendientes_count >= capacidad){
                    capacidad = capacidad + MAX_NODOS;
                    if ((id_nodos_pend = (int *)realloc(nodos_pendientes, capacidad * sizeof(int))) == NULL) {//aumentamos la memoria
                        printf("Error al asignar memoria.\n");
                        exit(1);
                    }
                    if ((ticket_nodos_pend = (int *)realloc(nodos_pendientes, capacidad * sizeof(int))) == NULL) {//aumentamos la memoria
                        printf("Error al asignar memoria.\n");
                        exit(1);
                    }
                }  
                id_nodos_pend[nodos_pendientes_count] = mensaje.id_nodo_origen;
                ticket_nodos_pend[nodos_pendientes_count] = mensaje.ticket_origen;
                nodos_pendientes_count++;
            }
        } else if (estoy_SC && mensaje.flag_consulta && SC_consultas) {
            // Aquí se implementa el envío de confirmación para una peticion para una Consulta
            mensaje.mtype = CONFIRMACION;
            mensaje.id_nodo_origen = mi_id;
            // mensaje.ticket_origen = mensaje.ticket_origen;
            if (msgsnd(msqid_nodos[mensaje.id_nodo_origen], &mensaje, msgsz, IPC_NOWAIT) < 0)
                printf("Error con msgsnd: %s\n", strerror(errno));
        } else {
            // Aquí se implementa la lógica de almacenamiento de nodos pendientes
            if(nodos_pendientes_count >= capacidad){
                capacidad = capacidad + MAX_NODOS;
                if ((id_nodos_pend = (int *)realloc(nodos_pendientes, capacidad * sizeof(int))) == NULL) {//aumentamos la memoria
                    printf("Error al asignar memoria.\n");
                    exit(1);
                }
                if ((ticket_nodos_pend = (int *)realloc(nodos_pendientes, capacidad * sizeof(int))) == NULL) {//aumentamos la memoria
                    printf("Error al asignar memoria.\n");
                    exit(1);
                }
            }  
            id_nodos_pend[nodos_pendientes_count] = mensaje.id_nodo_origen;
            ticket_nodos_pend[nodos_pendientes_count] = mensaje.ticket_origen;
            nodos_pendientes_count++;
        }
    }
}

void dar_SC(int ticket) {
    for (int i = 0; i < num_procesos; i++) {
        if (vector_peticiones[i] == ticket) {
            flag_pedir_otra_vez[i] = 0;
            sem_post(&semaforos_de_paso[i]); // Damos el paso al que hizo la solicitud
        }
    }
}

void solicitar_SC(int num_proceso, int prioridad_solicitud, int flag_consulta) {
    do {
        quiero = 1;
        if (prioridad_solicitud > mi_prioridad) mi_prioridad = prioridad;
        // else while (quiero && !estoy_SC); //espera activa
        else{
            flag_esperando_para_pedir_SC[prioridad_solicitud-1]=1;
            sem_wait(&sem_esperando_pedir_SC[prioridad_solicitud-1]);
            flag_esperando_para_pedir_SC[prioridad_solicitud-1]=0;
        }
        mi_ticket = max_ticket + 1;
        vector_peticiones[num_proceso] = mi_ticket;
        // Preparamos las solicitudes
        struct mssg_ticket solicitud;
        size_t buf_length = sizeof(struct mssg_ticket) - sizeof(int); //tamaño del mensaje - mtype
        solicitud.mtype = SOLICITUD;
        solicitud.id_nodo_origen = mi_id;
        solicitud.ticket_origen = mi_ticket;
        solicitud.prioridad_origen = prioridad_solicitud;
        solicitud.flag_consulta = flag_consulta;
        for (int i = 0; i < num_nodos; i++) {
            if (msgsnd(msqid_nodos[mensaje.id_nodo_origen], &mensaje, msgsz, IPC_NOWAIT) < 0)
                printf("Error con msgsnd: %s\n", strerror(errno));
        }
        sem_wait(&semaforos_de_paso[num_proceso]);
    } while (flag_pedir_otra_vez[num_proceso]);
    quiero = 0;
}

void liberar_SC() {
    //quiero = 0;

    //Comprobar si hay procesos esperando por solicitar SC
    for(int i = 3;i>0;i--){
        if(flag_esperando_para_pedir_SC[i-1]){
            sem_post(&sem_esperando_pedir_SC[i-1]);
            break;
        }
    }

    // Preparamos las confirmaciones
    struct mssg_ticket confirmacion;
    size_t buf_length = sizeof(struct mssg_ticket) - sizeof(int); //tamaño del mensaje - mtype
    solicitud.mtype = CONFIRMACION;
    solicitud.id_nodo_origen = mi_id;
    for (int i = 0; i < nodos_pendientes_count; i++) {
        solicitud.ticket_origen = ticket_nodos_pend[i];
        if (msgsnd(id_nodos_pend[i], &mensaje, msgsz, IPC_NOWAIT) < 0)
            printf("Error con msgsnd: %s\n", strerror(errno));
    }
    nodos_pendientes_count = 0;
}

void *lector(void *threadArgs){
    //args: prioridad, nro_proceso, sem_sinc
    while(1){
        struct arg_lector *args = threadArgs;
        int prioridad = args->prioridad;
        int nro_proceso = args->nro_proceso;
        sem_wait(&semaforos_de_paso[nro_proceso]);
        printf("[Proceso %d]=>Pidiendo SC...\n", nro_proceso);
        solicitar_SC(prioridad,nro_proceso,1);
        printf("[Proceso %d]=>Dentro de SC...\n", nro_proceso);
        sem_wait(&sem_ProtegeLectores);  //sem(0,1) para cambiar el valor de flagConsultasSC en exclusión mutua
        flagConsultasSC = 1;
        contadorLectores ++;
        sem_post(&sem_ProtegeLectores);
        sleep(5);
        liberar_SC();
        printf("[Proceso %d]=>Saliendo SC...\n", nro_proceso);
        sem_wait(&sem_ProtegeLectores);  //sem(0,1) para cambiar el valor de flagConsultasSC en exclusión mutua
        contadorLectores --;
        if (contadorLectores == 0){
            flagConsultasSC = 0;
        }
        sem_post(&sem_ProtegeLectores);
        //sem_wait(&semaforos_de_paso[nro_proceso]);
    }
}

void *escritor(void *threadArgs){
    while(1){
        struct arg_escritor *args = threadArgs;
        int prioridad = args->prioridad;
        int nro_proceso = args->nro_proceso;
        sem_wait(&semaforos_de_paso[nro_proceso]);                      //Esperamos a que nos den paso desde el main
        printf("[Proceso %d]=>Pidiendo SC...\n", nro_proceso);
        if (prioridad == 1) sem_wait(&sem_esperando_pedir_SC[0]);
        else if(prioridad == 2) sem_wait(&sem_esperando_pedir_SC[1]);
        else if(prioridad == 3) sem_wait(&sem_esperando_pedir_SC[2]);
        sem_wait(&sem_exclusionMutuaEscritor);                          //Como el rcv manda peticiones mientras estamos en SC hay que poner un semáforo de Exclusión mutua
        solicitar_SC(prioridad,nro_proceso,0);
        printf("[Proceso %d]=>Dentro de SC...\n", nro_proceso);
        sleep(5);
        liberar_SC();
        printf("[Proceso %d]=>Saliendo de SC...\n", nro_proceso);
        if (prioridad == 1) sem_post(&sem_esperando_pedir_SC[0]);
        else if(prioridad == 2) sem_post(&sem_esperando_pedir_SC[1]);
        else if(prioridad == 3) sem_post(&sem_esperando_pedir_SC[2]);
    }
}

int main(int argc, char *argv[]){
    if(argc != xd){
        printf("Uso: %s <id_nodo> <xd>\n", argv[0]);
        printf("Primer nodo = 0 y el último = %d",MAX_NODOS);
        return 1;
    }

    mi_id = atoi(argv[1]);

    //incializar las variables para el intercambio de mensajes
    int msqid_nodos[MAX_NODOS];
    if ((msqid_nodos[mi_id] = msgget(1069+mi_id, IPC_CREAT | IPC_EXCL | 0666)) < 0) printf("Error con msgget: %s\n", strerror(errno));
    printf("Pulsa ENTER cuando todos los nodos estén inicializados\n", nodo);
    while(!getchar());
    for(int i=0;i<MAX_NODOS;i++){
        if(i==mi_id)continue;
        msqid_nodos[i] = msgget(1069+i, 0666); // Colas de mensajes de los nodos
    }

    //inicializar semaforos para sincronizar procesos servidor
    sem_t sem_solicitar_SC[MAX_PROCESOS];
    for(int i=0;i<MAX_PROCESOS;i++){
        if(sem_init(&sem_solicitar_SC[i],0,0)==-1)
            printf("Error con semáforo de paso para servidores: %s\n", strerror(errno));
    }

    //inicializar los procesos servidores
    pthread_t hilos_servidores[MAX_PROCESOS];
    int opcion, proceso=0;
    do {
        sleep(1);
        /******* Menú de opciones *******/
        printf("\n\n\t\t\tMenu de Procesos\n");
        printf("\t\t\t----------------\n");
        printf("\t[1] Consultas\n");
        printf("\t[2] Reservas\n");
        printf("\t[3] Pagos\n");
        printf("\t[4] Administración\n");
        printf("\t[5] Anulaciones\n");

        printf("\n\tIngrese una opcion: ");
        scanf(" %d", &opcion);

        switch (opcion) {
            case 1: 
                if ((pthread_create(&hilos_servidores[proceso], NULL, lector, args)) < 0)//aqui
                    printf("Error con pthread_create: %s\n", strerror(errno));
                else printf("[%d] Consulta creado",proceso);
                break;
            case 2: 
                if ((pthread_create(&hilos_servidores[proceso], NULL, escritor, args)) < 0)//aqui
                    printf("Error con pthread_create: %s\n", strerror(errno));
                else printf("[%d] Reserva creado",proceso);
                break;
            case 3: 
                if ((pthread_create(&hilos_servidores[proceso], NULL, escritor, args)) < 0)//aqui
                    printf("Error con pthread_create: %s\n", strerror(errno));
                else printf("[%d] Pago creado",proceso);
                break;
            case 4: 
                if ((pthread_create(&hilos_servidores[proceso], NULL, escritor, args)) < 0)//aqui
                    printf("Error con pthread_create: %s\n", strerror(errno));
                else printf("[%d] Administración creado",proceso);
                break;
            case 5: 
                if ((pthread_create(&hilos_servidores[proceso], NULL, escritor, args)) < 0)//aqui
                    printf("Error con pthread_create: %s\n", strerror(errno));
                else printf("[%d] Anulaciones creado",proceso);
                break;
            default:
                printf("Error con la elección");
            
        }proceso++;
    }while (proceso<10);

    //inicializamos el receiver
    pthread_t hilo_receptor;
    if ((pthread_create(&hilo_receptor, NULL, receiver, msqid_nodos[mi_id])) < 0)
        printf("Error con pthread_create: %s\n", strerror(errno));
    else printf("Receiver creado",proceso);

    //menú para pedir SC
    while (1) {
        sleep(1);

        printf ("\n");
        printf ("1. Introducir un proceso en la SC.\n");
        //printf ("2. Quitar un proceso de la SC.\n");
        printf ("3. Salir del programa.\n");
        printf ("Elige una opcion: ");

        scanf ("%i", &opcion);

        switch (opcion) {

            case 1:
                printf ("Que hilo quire introducir en la Seccion Critica? ");
                scanf ("%i", &opcion);
                sem_post (&sem_solicitar_SC[opcion]);
                break;
            case 2:
                printf ("Que hilo quire sacar de la Seccion Critica? ");
                scanf ("%i", &opcion);
                //sem_post (&semaforoSincronizacion[opcion + nHilos]);
                break;
            case 3:
                printf ("Cerrando programa.\n\n");
                free(id_nodos_pend);
                free(ticket_nodos_pend);
                if(msgctl(miID, IPC_RMID, NULL) < 0){
                    printf("Error con msgctl: %s\n", strerror(errno));
                    exit(1);
                }else return 0;
            default:
                printf ("Opcion no contemplada.\n");
                break;
        }
    }
}
