#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <limits.h>

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
sem_t semaforos_de_paso[MAX_PROCESOS];
int flag_pedir_otra_vez[MAX_PROCESOS] = {0};
int nodos_pend = 0;
int id_nodos_pend[MAX_NODOS];
int ticket_nodos_pend[MAX_NODOS];

void *receiver(void *arg) {

    int *msqid = (int *)arg;
    printf("id:%d\n",*msqid);
    struct mssg_ticket mensaje;
    size_t buf_length = sizeof(struct msgbuf) - sizeof(int); //tamaño del mensaje - mtype

    while (1) {
        if(msgrcv(*msqid, (void *)&mensaje, buf_length, 0, 0) < 0){
            printf("Error con msgrcv: %s\n", strerror(errno));
            exit(-1);
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
                        sem_post(semaforos_de_paso[i]); //lo despertamos pero debe pedir otra vez
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
                if (nodos_pend < MAX_NODOS) {
                    id_nodos_pend[nodos_pend] = mensaje.id_nodo_origen;
                    ticket_nodos_pend[nodos_pend] = mensaje.ticket_origen;
                    nodos_pend++;
                } else {
                    // Manejar el caso en que se excede el límite de nodos pendientes
                    printf("Error: Se ha excedido el límite de nodos pendientes.\n");
                }
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
            if (nodos_pend < MAX_NODOS) {
                id_nodos_pend[nodos_pend] = mensaje.id_nodo_origen;
                ticket_nodos_pend[nodos_pend] = mensaje.ticket_origen;
                nodos_pend++;
            } else {
                // Manejar el caso en que se excede el límite de nodos pendientes
                printf("Error: Se ha excedido el límite de nodos pendientes.\n");
            }
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
        else while (quiero && !estoy_SC);
        mi_ticket = max_ticket + 1;
        vector_peticiones[num_proceso] = mi_ticket;
        // Preparamos las solicitudes
        struct mssg_ticket solicitud;
        size_t buf_length = sizeof(struct msgbuf) - sizeof(int); //tamaño del mensaje - mtype
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
}

void liberar_SC() {
    quiero = 0;
    // Preparamos las confirmaciones
    struct mssg_ticket confirmacion;
    size_t buf_length = sizeof(struct msgbuf) - sizeof(int); //tamaño del mensaje - mtype
    solicitud.mtype = CONFIRMACION;
    solicitud.id_nodo_origen = mi_id;
    for (int i = 0; i < nodos_pend; i++) {
        solicitud.ticket_origen = ticket_nodos_pend[i];
        if (msgsnd(id_nodos_pend[i], &mensaje, msgsz, IPC_NOWAIT) < 0)
            printf("Error con msgsnd: %s\n", strerror(errno));
    }
    nodos_pend = 0;
}

int main(int argc, char *argv[]){//este es el que usé para testigo
    if(argc != 2){
		printf("Uso: %s <Número de nodo>\n", argv[0]);
        return 1;
	}
    // Inicializar semáforos
    sem_init(&sem_vector_peticiones, 0, 1);
    sem_init(&sem_vector_atendidas, 0, 1);
    sem_init(&sem_testigo, 0, 1);

    pthread_t hilo_receptor;
    struct msgbuf mensaje;
    size_t msgsz = sizeof(struct msgbuf) - sizeof(int); //tamaño de (mtype + vector + nodoID) - mtype
    nodo = atoi(argv[1]);
    int mi_peticion = 0;
    int k=0;

    // Intentar obtener el token
    int msqid_token = msgget(1069, 0666); // Cola de mensajes del token
    if (msgrcv(msqid_token, &mensaje, msgsz, 1, IPC_NOWAIT) < 0) {
        if (errno == ENOMSG) {
            printf("No hay token.\n");
            testigo = 0;
        } else {
            perror("msgrcv");
            return -1;
        }
    }else{
        printf("Tengo el token.\n");
        testigo = 1;
    }

    // Almacena los ids
    for(int i=0;i<N;i++) msqid_nodos[i] = msgget(1069+i+1, 0666); // Colas de mensajes de los nodos

    // Lanzar el hilo del recibidor de solicitudes
    int hilo_recibidor;
    if ((hilo_recibidor=pthread_create(&hilo_receptor, NULL, receiver, &msqid_nodos[nodo-1])) < 0) printf("Error con pthread_create: %s\n", strerror(errno));

    // Bucle del nodo
    while(1){
        printf("Pulsa enter para intentar entrar a la sección critica\n");
        fflush(stdout);
        while(!getchar());
        printf("Intentando entrar a la SC...\n");
        fflush(stdout);

        if(!testigo){
            mensaje.mtype = 2;
            mensaje.vector[nodo-1] = ++mi_peticion;
            mensaje.nodoID = nodo;
            for(int i=0;i<N;i++){
                if(i+1 == nodo) continue;
                if (msgsnd(msqid_nodos[i], &mensaje, msgsz, IPC_NOWAIT) < 0){
                    printf("Error con msgsnd: %s\n", strerror(errno));
                    return -1;
                }
                else printf("Solicitud enviada al nodo %d.\n",i+1);
            }
            printf("Esperando recibir el testigo..\n");
            if (msgrcv(msqid_nodos[nodo-1], &mensaje, msgsz, 1, 0) < 0){
                printf("Error con msgrcv: %s\n", strerror(errno));
                return -1;
            }
            testigo=1;
        }

        dentro = 1;
        printf("En la seccion critica, pulsa ENTER para salir\n");
        fflush(stdout);
        while(!getchar());
        mensaje.vector[nodo]=mi_peticion;
        dentro = 0;

        k++;
        for(int i=k;i<N;i++){
            if(vector_atendidas[i]<vector_peticiones[i]){
                if (msgsnd(msqid_nodos[i], &mensaje, msgsz, IPC_NOWAIT) < 0)
                    printf("Error con msgsnd: %s\n", strerror(errno));
                break;
            }
        }
        for(int i=0;i<k;i++){
            if(vector_atendidas[i]<vector_peticiones[i]){
                if (msgsnd(msqid_nodos[i], &mensaje, msgsz, IPC_NOWAIT) < 0)
                    printf("Error con msgsnd: %s\n", strerror(errno));
                break;
            }
        }
        printf("Fuera de la sección crítica.\n");
        fflush(stdout);
    }
    if(pthread_join(hilo_recibidor,NULL) != 0)
            perror("Error al unirse el thread");
    return 0;
}
