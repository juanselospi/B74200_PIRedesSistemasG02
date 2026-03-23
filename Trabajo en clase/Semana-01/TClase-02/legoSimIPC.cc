#include <pthread.h>
#include <iostream>
#include <queue>
#include <string>
#include <unistd.h>
#include <vector>

using namespace std;

typedef struct {
    string tipo; // que tipo de request estoy haciendo
    string contenido; // que hay dentro del request
    int segmento; // el segmento de la figura que pido, mitad 1 o mitad 2
    int cantidad; // numero de piezas
} Mensaje;


queue<Mensaje> buzon_ci; // buzon cliente - intermediario
queue<Mensaje> buzon_is; // buzon intermediario - servidor

pthread_mutex_t mutex_ci;
pthread_mutex_t mutex_is;


bool terminado = false;

// simulador de cliente IPC
void* cliente(void* arg) {

    // sehace la solicitud de la figura
    Mensaje m;
    m.tipo = "PETICION_LEGO";
    m.contenido = "Caballo";
    m.segmento = 1;


    pthread_mutex_lock(&mutex_ci);

    buzon_ci.push(m);

    pthread_mutex_unlock(&mutex_ci);

    cout << "El cliente solicito una figura\n";

    // se espera la respuesta del intermediario
    while(true) {

        pthread_mutex_lock(&mutex_ci);

        if(!buzon_ci.empty()) {

            Mensaje respuesta = buzon_ci.front();

            if(respuesta.tipo == "PIEZAS_CABALLO") {

                buzon_ci.pop();

                pthread_mutex_unlock(&mutex_ci);

                // imprimo el contenido de la respeusta que recive le cliente
                cout << "\n===== RESULTADO =====\n";
                cout << respuesta.tipo << endl;
                cout << "Segmento: " << respuesta.segmento << endl;;
                cout << "Piezas: " << respuesta.contenido << endl;
                cout << "Numero de piezas: " << respuesta.cantidad << endl;

                terminado = true;

                break;
            }
        }

        pthread_mutex_unlock(&mutex_ci);

        usleep(100000);
    }

    pthread_exit(NULL);
}

// simulador de intermediario
void* intermediario(void* arg) {

    while(!terminado) {

        pthread_mutex_lock(&mutex_ci);

        if(!buzon_ci.empty()) {

            Mensaje m = buzon_ci.front();

            if(m.tipo == "PETICION_LEGO") {

                buzon_ci.pop();
                pthread_mutex_unlock(&mutex_ci);

                cout << "Intermediario recibio solicitud de figura\n";

                Mensaje peticion;
                peticion.tipo = "PETICION_PIEZAS";
                peticion.contenido = m.contenido;
                peticion.segmento = m.segmento;


                pthread_mutex_lock(&mutex_is);

                buzon_is.push(peticion);

                pthread_mutex_unlock(&mutex_is);

                cout << "Intermediario pidio piezas al server\n";
                
                continue;
            }
        }

        pthread_mutex_unlock(&mutex_ci);

        pthread_mutex_lock(&mutex_is);

        if(!buzon_is.empty()) {

            Mensaje m = buzon_is.front();

            if(m.tipo == "PIEZAS_CABALLO") {

                buzon_is.pop();
                pthread_mutex_unlock(&mutex_is);

                cout << "Intermediario recibio piezas del server\n";

                pthread_mutex_lock(&mutex_ci);

                buzon_ci.push(m);

                pthread_mutex_unlock(&mutex_ci);

                cout << "Intermediario envio piezas al cliente\n";

                continue;
            }
        }

        pthread_mutex_unlock(&mutex_is);

        usleep(100000);
    }

    pthread_exit(NULL);
}

// simulacion basica de un server
void* servidor(void* arg) {

    while(!terminado) {

        pthread_mutex_lock(&mutex_is);

        if(!buzon_is.empty()) {

            Mensaje m = buzon_is.front();

            if(m.tipo == "PETICION_PIEZAS") {

                buzon_is.pop();

                pthread_mutex_unlock(&mutex_is);

                cout << "Servidor procesando solicitud...\n";

                string piezas;
                int conteo;

                if(m.segmento == 1) {
                    // string que simula una base de datos simple para este ejemplo
                    piezas = "brick 2x2 yellow (x1), brick 2x4 white (x2), brick 2x4 yellow (x2), brick 2x6 yellow (x1)";
                    conteo = 6;
                } else {
                    piezas = "brick 2x3 yellow (x5), brick 1x1 yellow eye (x4), brick 2x1 yellow (x2), brick 2x2 white (x1)";
                    conteo = 12;
                }
               
                Mensaje respuesta;
                respuesta.tipo = "PIEZAS_CABALLO";
                respuesta.contenido = piezas;
                respuesta.segmento = m.segmento;
                respuesta.cantidad = conteo;


                pthread_mutex_lock(&mutex_is);

                buzon_is.push(respuesta);

                pthread_mutex_unlock(&mutex_is);

                cout << "Servidor envia piezas de vuelta\n";

                continue;
            }
        }

        pthread_mutex_unlock(&mutex_is);

        usleep(100000);
    }

    pthread_exit(NULL);
}


int main() {

    pthread_t hilos[3];

    pthread_mutex_init(&mutex_ci, NULL);

    pthread_mutex_init(&mutex_is, NULL);

    pthread_create(&hilos[0], NULL, servidor, NULL);
    pthread_create(&hilos[1], NULL, intermediario, NULL);
    pthread_create(&hilos[2], NULL, cliente, NULL);

    // join de hilos
    for(int i = 0; i < 3; i++) {

        pthread_join(hilos[i], NULL);
    }

    pthread_mutex_destroy(&mutex_ci);

    pthread_mutex_destroy(&mutex_is);

    return 0;
}

// CORRE CON make legoSimIPC.out y ./legoSiIPC.out

/*

 I  ~/De/V/PI Redes y Sistemas/Trabajo en clase/Semana-01/TClase-02  main !2 ?1  make legoSimIPC.out                   ✔  20:03:34 
$TARGETS = ipv4-udp-client.out ipv4-udp-server.out
g++ -g -pthread legoSimIPC.cc -o legoSimIPC.out
 I  ~/De/V/PI Redes y Sistemas/Trabajo en clase/Semana-01/TClase-02  main !2 ?2  ./legoSimIPC.out                      ✔  20:03:39 
El cliente solicito una figura
Intermediario recibio solicitud de figura
Intermediario pidio piezas al server
Servidor procesando solicitud...
Servidor envia piezas de vuelta
Intermediario recibio piezas del server
Intermediario envio piezas al cliente

===== RESULTADO =====
PIEZAS_CABALLO
Segmento: 1
Piezas: brick 2x2 yellow (x1), brick 2x4 white (x2), brick 2x4 yellow (x2), brick 2x6 yellow (x1)
Numero de piezas: 6

*/

