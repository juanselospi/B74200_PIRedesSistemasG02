#include <pthread.h>
#include <iostream>
#include <queue>
#include <string>
#include <unistd.h>

using namespace std;

typedef struct {
    int origen;
    string mensaje;
} Mensaje;

queue<Mensaje> buzon;

pthread_mutex_t mutex_buzon;

int mensajes_recibidos = 0;
const int TOTAL_MENSAJES = 2;

void* cliente(void* arg) {

    int id = *((int*)arg);

    Mensaje m;
    m.origen = id;
    m.mensaje = "Aqui el proceso " + to_string(id) + ", cambio!";

    pthread_mutex_lock(&mutex_buzon);

    buzon.push(m);

    pthread_mutex_unlock(&mutex_buzon);

    cout<<"Cliente "<<id<<" envio mensaje\n";

    pthread_exit(NULL);
}

void* servidor(void* arg) {

    while(mensajes_recibidos < TOTAL_MENSAJES) {

        pthread_mutex_lock(&mutex_buzon);

        if(!buzon.empty()) {

            Mensaje m = buzon.front();
            buzon.pop();

            cout<<"Servidor recibio mensaje de proceso " << m.origen << ": (" << m.mensaje << ")\n";

            mensajes_recibidos++;
        }

        pthread_mutex_unlock(&mutex_buzon);

        usleep(100000); // delay para evitar mucho busy wait
    }

    pthread_exit(NULL);
}

int main() {

    pthread_t hilos[3]; // 3 hilos como especificaba el profe

    pthread_mutex_init(&mutex_buzon, NULL);

    int id0 = 0;
    int id1 = 1;


    pthread_create(&hilos[0], NULL, servidor, NULL);
    pthread_create(&hilos[1], NULL, cliente, &id0);
    pthread_create(&hilos[2], NULL, cliente, &id1);

    // join de hilos creados
    for(int i = 0; i < 3; i++) {

        pthread_join(hilos[i], NULL);
    }

    pthread_mutex_destroy(&mutex_buzon);

    return 0;
}
