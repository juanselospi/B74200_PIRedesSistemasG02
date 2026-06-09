#include <pthread.h>
#include <iostream>
#include <queue>
#include <string>
#include <unistd.h>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <chrono>

using namespace std;

struct EntradaDirectorio {
    string nombre; // servidor-figuras / intermediario
    string rol; // servidor o intermediario o cliente
    bool activo;
};

vector<EntradaDirectorio> directorioGlobal;
pthread_mutex_t mutex_directorio = PTHREAD_MUTEX_INITIALIZER;

// flags de descubrimiento
bool servidor_descubierto = false; // intermediario encontro al servidor
bool intermediario_descubierto = false; // servidor encontró al intermediario


void registrarEnDirectorio(const string& nombre, const string& rol) {

    pthread_mutex_lock(&mutex_directorio);
    EntradaDirectorio e;
    e.nombre  = nombre;
    e.rol     = rol;
    e.activo  = true;
    directorioGlobal.push_back(e);
    pthread_mutex_unlock(&mutex_directorio);
}


// devuelve true si encuentra una entrada con el rol buscado y escribe el nombre en encontrado
bool buscarEnDirectorio(const string& rolBuscado, string& encontrado) {

    pthread_mutex_lock(&mutex_directorio);

    for(const auto& e : directorioGlobal) {

        if(e.rol == rolBuscado && e.activo) {

            encontrado = e.nombre;
            pthread_mutex_unlock(&mutex_directorio);
            return true;
        }
    }

    pthread_mutex_unlock(&mutex_directorio);
    return false;
}


pthread_mutex_t mutex_log = PTHREAD_MUTEX_INITIALIZER;


void writeLog(const string& entidad, const string& tipoLog, const string& mensaje) {

    auto now = chrono::system_clock::now();
    time_t now_time = chrono::system_clock::to_time_t(now);
    struct tm* timeinfo = localtime(&now_time);
    
    char timeBuffer[80];

    strftime(timeBuffer, sizeof(timeBuffer), "%d/%m/%Y %H:%M", timeinfo);
    
    pthread_mutex_lock(&mutex_log);

    ofstream logFile("bitacora.log", ios::app);

    if(logFile.is_open()) {

        logFile << "[" << entidad << "] [" << timeBuffer << "] [" << tipoLog << "] " << mensaje << "\n";
        
        logFile.close();
    }

    pthread_mutex_unlock(&mutex_log);
}

typedef struct {
    string tipo; // que tipo de request
    string id; // ID del cliente original
    string figura; // la figura que el usuario pide
    vector<string> figuras; // si el usuario pide la lista
    int segmento; // mitad 1 o mitad 2
    vector<pair<string, int>> piezas; // piezas y cantidades individuales          
} Mensaje;


// CADA PIEZA TIENE SU PROPIA METADATA (se puede ampliar)
struct MetadataPieza {

    string tipo; // ej 1x1 2x2
    string fechaCreacion;
};


struct Pieza {

    string nombre;
    int cantidad;

    MetadataPieza metaPieza;
};


// LA FIGURA COMO TAL TIENE SU PROPIA METADATA
struct MetadataFigura {

    int totalPiezas;
    string fechaCreacion;
    string ultimaModificacion;
};


// CADA FIGURA TIENE PIEZAS ASOCIADAS
struct Figura {

    string nombre;

    vector<Pieza> segmento1;
    vector<Pieza> segmento2;

    MetadataFigura meta;
};


map<string, Figura> fileSystem_main;
map<string, Figura> fileSystem_backup;


map<string, vector<string>> servidores; // mapa de servidores y las figuras que tienen

queue<Mensaje> buzon_ci; // buzon cliente - intermediario
queue<Mensaje> buzon_is; // buzon intermediario - servidor
queue<Mensaje> buzon_i2;

pthread_mutex_t mutex_ci; // buzon cliente - intermediario
pthread_mutex_t mutex_is; // buzon intermediario - servidor
pthread_mutex_t mutex_i2;


// mensajes tipo ANNOUNCE / DISCOVER / ACK_DISCOVER
// se usan colas separadas para no interferir con el flujo principal
queue<Mensaje> buzon_discovery;
pthread_mutex_t mutex_discovery = PTHREAD_MUTEX_INITIALIZER;

bool terminado = false;


// anuncia su presencia y busca activamente al servidor de figuras
void* discovery_intermediario(void* arg) {

    const string MI_NOMBRE = "intermediario-1";
    const string ROL_BUSCADO = "SERVIDOR";

    // anunciarse en el directorio
    registrarEnDirectorio(MI_NOMBRE, "INTERMEDIARIO");
    cout << "[DISCOVERY][INTERM] ANNOUNCE -> Me registro como '" << MI_NOMBRE << "'" << endl;
    writeLog("DISCOVERY-INTERM", "ANNOUNCE", "Intermediario registrado en directorio global");

    // publicar mensaje ANNOUNCE en el buzon de discovery para que otros lo vean
    {
        Mensaje anuncio;
        anuncio.tipo = "ANNOUNCE";
        anuncio.id   = MI_NOMBRE;
        pthread_mutex_lock(&mutex_discovery);
        buzon_discovery.push(anuncio);
        pthread_mutex_unlock(&mutex_discovery);
    }

    // buscar al servidor de figuras con reintentos
    int intentos = 0;
    while(!terminado && !servidor_descubierto) {

        string nombreServidor;
        if(buscarEnDirectorio(ROL_BUSCADO, nombreServidor)) {

            servidor_descubierto = true;
            cout << "[DISCOVERY][INTERM] DISCOVER -> Encontre al servidor: '" << nombreServidor << "'" << endl;
            writeLog("DISCOVERY-INTERM", "DISCOVER", "Servidor encontrado: " + nombreServidor);

            // enviar ACK al servidor por el buzón de discovery
            Mensaje ack;
            ack.tipo = "ACK_DISCOVER";
            ack.id   = MI_NOMBRE;
            ack.figura = nombreServidor; // reutilizamos el campo para el destinatario
            pthread_mutex_lock(&mutex_discovery);
            buzon_discovery.push(ack);
            pthread_mutex_unlock(&mutex_discovery);

        } else {
            intentos++;
            cout << "[DISCOVERY][INTERM] DISCOVER -> Servidor no encontrado aun (intento " << intentos << ")" << endl;
            writeLog("DISCOVERY-INTERM", "DISCOVER", "Servidor no encontrado, reintentando...");
            usleep(500000); // espera antes de reintentar
        }
    }

    cout << "[DISCOVERY][INTERM] Descubrimiento completado" << endl;
    return NULL;
}


// anuncia su presencia y espera ser encontrado por el Intermediario
void* discovery_servidor(void* arg) {

    const string MI_NOMBRE = "servidor-figuras";
    const string ROL_BUSCADO = "INTERMEDIARIO";

    usleep(200000);

    // anunciarse en el directorio (ANNOUNCE)
    registrarEnDirectorio(MI_NOMBRE, "SERVIDOR");
    cout << "[DISCOVERY][SERV] ANNOUNCE -> Me registro como '" << MI_NOMBRE << "'" << endl;
    writeLog("DISCOVERY-SERV", "ANNOUNCE", "Servidor registrado en directorio global");

    {
        Mensaje anuncio;
        anuncio.tipo = "ANNOUNCE";
        anuncio.id   = MI_NOMBRE;
        pthread_mutex_lock(&mutex_discovery);
        buzon_discovery.push(anuncio);
        pthread_mutex_unlock(&mutex_discovery);
    }

    // buscar al intermediario (DISCOVER) con reintentos
    int intentos = 0;
    while(!terminado && !intermediario_descubierto) {

        string nombreInterm;
        if(buscarEnDirectorio(ROL_BUSCADO, nombreInterm)) {

            intermediario_descubierto = true;
            cout << "[DISCOVERY][SERV] DISCOVER -> Encontre al intermediario: '" << nombreInterm << "'" << endl;
            writeLog("DISCOVERY-SERV", "DISCOVER", "Intermediario encontrado: " + nombreInterm);

        } else {
            intentos++;
            cout << "[DISCOVERY][SERV] DISCOVER -> Intermediario no encontrado aun (intento " << intentos << ")" << endl;
            writeLog("DISCOVERY-SERV", "DISCOVER", "Intermediario no encontrado, reintentando...");
            usleep(500000);
        }
    }

    // 3. Escuchar ACKs entrantes del buzón de discovery
    int espera = 0;
    while(!terminado && espera < 20) {

        pthread_mutex_lock(&mutex_discovery);

        if(!buzon_discovery.empty()) {

            Mensaje m = buzon_discovery.front();
            buzon_discovery.pop();
            pthread_mutex_unlock(&mutex_discovery);

            if(m.tipo == "ACK_DISCOVER" && m.figura == MI_NOMBRE) {

                cout << "[DISCOVERY][SERV] ACK_DISCOVER <- Recibido de '" << m.id << "'. Enlace establecido!" << endl;
                writeLog("DISCOVERY-SERV", "ACK_DISCOVER", "Enlace confirmado con: " + m.id);
                break;
            }

        } else {
            pthread_mutex_unlock(&mutex_discovery);
        }

        usleep(200000);
        espera++;
    }

    cout << "[DISCOVERY][SERV] Descubrimiento completado" << endl;
    return NULL;
}


// simulador de cliente IPC
void* cliente(void* arg) {

    string cliente = "B74200";
    string peticion;
    int segmento;
    bool primera_vez = true;

    while(true) {

        usleep(500000);
        
        if(primera_vez) {

            peticion = "legos";
            primera_vez = false;

        } else {

            cout << endl << "Para ver legos disponibles digite: LEGOS" << endl;
            cout << "Para pedir un kit digite el nombre" << endl;
            cout << "Para salir digite: EXIT \n" << endl;

            /*
            no se hacen grandes checks para el input del cliente porque se asume un cliente tonto que no conoce de antemano lo que contiene un servidor y
            por ende se limita a enviar la solicitud de lo que pida el usuario.

            se entiende que si el usuario mete un nombre que no existe en legos no es un error, sino una intencion de desarrollo, pues imita el escenario donde tal figura
            pueda ser agregada al servidor propio o ajeno luego de que la lista de figuras fuera solicitada.
            */
            
            getline(cin, peticion);

            if(peticion.empty()) {

                continue;
            }


            for(char &c : peticion) {
                
                c = tolower((unsigned char)c);
            }

        }

        if(peticion == "exit") {

            cout << "cliente finalizando..." << endl;
            terminado = true;

            Mensaje m_exit;
            m_exit.tipo = "exit";
            
            pthread_mutex_lock(&mutex_ci);

            buzon_ci.push(m_exit);

            pthread_mutex_unlock(&mutex_ci);

            break;
        }

        Mensaje m;
        m.id = cliente;

        if(peticion == "legos") {

            m.tipo = "LIST_FIGURES";

        } else {

            m.tipo = "GET_FIGURE";
            m.figura = peticion;

            cout << "que mitad desea, 1 o 2?" << endl;

            cin >> segmento;
            cin.ignore(1000, '\n');

            if(segmento != 1 && segmento != 2) {

                cout << "peticion invalida" << endl;
                cout << "segmento 1 por defecto" << endl;
                m.segmento = 1;

            } else {
                m.segmento = segmento;
            }
        }

        pthread_mutex_lock(&mutex_ci);

        buzon_ci.push(m);

        pthread_mutex_unlock(&mutex_ci);

        cout << "El cliente " << m.id << " pide " << peticion << endl;

        if(m.tipo == "LIST_FIGURES") {

            writeLog("CLIENTE", "SOLICITUD", "LIST_FIGURES");

        } else {

            writeLog("CLIENTE", "SOLICITUD", "GET_FIGURE figura=" + m.figura + " segmento=" + to_string(m.segmento));
        }

        bool respuesta_recibida = false;
        int time_waited = 0;

        while(!respuesta_recibida) {

            pthread_mutex_lock(&mutex_ci);

            if(!buzon_ci.empty()) {

                Mensaje respuesta = buzon_ci.front();

                if(respuesta.id == cliente) {

                    buzon_ci.pop();

                    pthread_mutex_unlock(&mutex_ci);

                    if(respuesta.tipo == "RETURN_FIGURE") {

                        writeLog("CLIENTE", "RESPUESTA", "RETURN_FIGURE figura=" + respuesta.figura + " segmento=" + to_string(respuesta.segmento));
                        cout << "FIGURA: " << respuesta.figura << endl;
                        cout << "SEGMENTO: " << respuesta.segmento << endl;
                        cout << "PIEZAS:" << endl;

                        int total = 0;

                        for(const auto& piece : respuesta.piezas) {

                            cout << "- " << piece.first << " x" << piece.second << endl;
                            total += piece.second;

                        }

                        cout << "TOTAL DE PIEZAS:" << endl;

                        cout << total << endl;

                        break;

                    } else if(respuesta.tipo == "FIGURES_LIST") {

                        writeLog("CLIENTE", "RESPUESTA", "FIGURES_LIST");
                        cout << "Lista de LEGOS:" << endl;

                        for(const string& figura : respuesta.figuras) {

                            cout << "- " << figura << endl;
                        }

                        break;

                    } else if(respuesta.tipo == "FIGURE_NOT_FOUND") {

                        writeLog("CLIENTE", "RESPUESTA", "FIGURE_NOT_FOUND");
                        cout << "Figura no encontrada" << endl;

                        break;
                    }

                    respuesta_recibida = true;

                }
            }

            pthread_mutex_unlock(&mutex_ci);

            usleep(100000);
            time_waited++;

            if(time_waited > 50) {

                writeLog("CLIENTE", "ERROR", "Timeout esperando respuesta");
                cout << "Timeout esperando respuesta" << endl;
                break;
            }
        }
    }

    pthread_exit(NULL);
}


void* intermediario(void* arg) {

    while(!terminado) {

        pthread_mutex_lock(&mutex_ci);

        if(!buzon_ci.empty()) {

            Mensaje m = buzon_ci.front();

            if(m.tipo == "exit") {

                buzon_ci.pop();
                pthread_mutex_unlock(&mutex_ci);

                Mensaje m_exit;
                m_exit.tipo = "exit";


                pthread_mutex_lock(&mutex_is);
                buzon_is.push(m_exit);
                pthread_mutex_unlock(&mutex_is);



                pthread_mutex_lock(&mutex_i2);
                buzon_i2.push(m_exit);
                pthread_mutex_unlock(&mutex_i2);


                cout << "Intermediario finalizando..." << endl;
                break;
            }

            if(m.tipo == "GET_FIGURE") {

                cout << "Intermediario procesa solicitud " << m.tipo << " de cliente " << m.id << endl;
                writeLog("INTERM", "SOLICITUD", "ASK_FIGURE figura=" + m.figura + " segmento=" + to_string(m.segmento));
                writeLog("INTERM", "ENRUTAMIENTO", "Enviando solicitud a servidor");

                buzon_ci.pop();
                pthread_mutex_unlock(&mutex_ci);

                Mensaje peticion;
                peticion.tipo = "ASK_FIGURE";
                peticion.id = m.id;
                peticion.figura = m.figura;
                peticion.segmento = m.segmento;

                pthread_mutex_lock(&mutex_is);

                buzon_is.push(peticion);

                pthread_mutex_unlock(&mutex_is);

                continue;
            }

            if(m.tipo == "LIST_FIGURES") {

                cout << "Intermediario procesa solicitud " << m.tipo << " de cliente " << m.id << endl;
                writeLog("INTERM", "SOLICITUD", "ASK_FIGURE figura=ALL");
                writeLog("INTERM", "ENRUTAMIENTO", "Enviando solicitud a servidor");

                buzon_ci.pop();
                pthread_mutex_unlock(&mutex_ci);

                Mensaje peticion;
                peticion.tipo = "ASK_FIGURE";
                peticion.id = m.id;
                peticion.figura = "ALL"; // CAMBIO
                peticion.segmento = 0;

                pthread_mutex_lock(&mutex_is);

                buzon_is.push(peticion);

                pthread_mutex_unlock(&mutex_is);

                continue;
            }

            pthread_mutex_unlock(&mutex_ci);

        } else {

            pthread_mutex_unlock(&mutex_ci);
        }


        pthread_mutex_lock(&mutex_is);

        if(!buzon_is.empty()) {

            Mensaje m = buzon_is.front();

            // si la encuentra
            if(m.tipo == "FIGURE_FOUND") {

                cout << "Intermediario recibio respuesta " << m.tipo << " de servidor para cliente " << m.id << endl;
                
                buzon_is.pop();
                pthread_mutex_unlock(&mutex_is);

                Mensaje respuesta;
                respuesta.id = m.id;

                if(!m.figuras.empty()) {

                    respuesta.tipo = "FIGURES_LIST";
                    respuesta.figuras = m.figuras;
                    writeLog("INTERM", "RESPUESTA", "FIGURES_LIST");

                } else {

                    respuesta.tipo = "RETURN_FIGURE";
                    respuesta.figura = m.figura;
                    respuesta.segmento = m.segmento;
                    respuesta.piezas = m.piezas;
                    writeLog("INTERM", "RESPUESTA", "RETURN_FIGURE figura=" + m.figura + " segmento=" + to_string(m.segmento));
                }

                pthread_mutex_lock(&mutex_ci);

                buzon_ci.push(respuesta);

                pthread_mutex_unlock(&mutex_ci);

                continue;
            }


            // hace un fallback al intermediario
            if(m.tipo == "FIGURE_NOT_FOUND") {

                cout << "Intermediario recibio respuesta " << m.tipo << " del servidor. Redireccionando solicitud... " << endl;

                writeLog("INTERM", "ENRUTAMIENTO", "Figura no encontrada localmente");
                writeLog("INTERM", "SOLICITUD", "GET_FIGURE figura=" + m.figura + " segmento=" + to_string(m.segmento) + " (reenvío a intermediarios)");
                writeLog("INTERM", "ENRUTAMIENTO", "Reenviando GET_FIGURE (interm-interm)");

                buzon_is.pop();
                pthread_mutex_unlock(&mutex_is);

                Mensaje peticion;
                peticion.tipo = "ASK_FIGURE";
                peticion.id = m.id;
                peticion.figura = m.figura;
                peticion.segmento = m.segmento;

                pthread_mutex_lock(&mutex_i2);

                buzon_i2.push(peticion);

                pthread_mutex_unlock(&mutex_i2);

                // cout << "SI ENTRE AL FALLBACK" << endl;

                continue;
            }

            pthread_mutex_unlock(&mutex_is);

        } else {

            pthread_mutex_unlock(&mutex_is);

        }


        pthread_mutex_lock(&mutex_i2);

        if(!buzon_i2.empty()) {

            Mensaje m = buzon_i2.front();

            if(m.tipo == "FIGURE_FOUND") {

                cout << "Intermediario recibio respuesta " << m.tipo << " de otro intermediario para cliente " << m.id << endl;
                writeLog("INTERM", "RESPUESTA", "RETURN_FIGURE figura=" + m.figura + " segmento=" + to_string(m.segmento));

                buzon_i2.pop();
                pthread_mutex_unlock(&mutex_i2);

                Mensaje respuesta;
                respuesta.tipo = "RETURN_FIGURE";
                respuesta.id = m.id;
                respuesta.figura = m.figura;
                respuesta.segmento = m.segmento;
                respuesta.piezas = m.piezas;

                pthread_mutex_lock(&mutex_ci);

                buzon_ci.push(respuesta);

                pthread_mutex_unlock(&mutex_ci);

                continue;

            } else if (m.tipo == "FIGURE_NOT_FOUND") {

                cout << "Intermediario recibio respuesta " << m.tipo << " de otro intermediario para cliente " << m.id << endl;
                writeLog("INTERM", "RESPUESTA", "FIGURE_NOT_FOUND figura=" + m.figura + " segmento=" + to_string(m.segmento));
                writeLog("INTERM", "ERROR", "Ningún intermediario respondió a la solicitud");

                buzon_i2.pop();
                pthread_mutex_unlock(&mutex_i2);

                Mensaje respuesta;
                respuesta.tipo = "FIGURE_NOT_FOUND";
                respuesta.id = m.id;

                pthread_mutex_lock(&mutex_ci);
                buzon_ci.push(respuesta);
                pthread_mutex_unlock(&mutex_ci);

                continue;
            }

            pthread_mutex_unlock(&mutex_i2);

        } else {

            pthread_mutex_unlock(&mutex_i2);
        }
    }

    return NULL;
}


void* intermediario2(void* arg) {

    while(!terminado) {

        pthread_mutex_lock(&mutex_i2);

        if(!buzon_i2.empty()) {

            Mensaje m = buzon_i2.front();

            if(m.tipo == "exit") {

                buzon_i2.pop();
                pthread_mutex_unlock(&mutex_i2);

                cout << "Intermediario2 finalizando..." << endl;
                break;
            }

            if(m.tipo == "ASK_FIGURE") {

                buzon_i2.pop();
                pthread_mutex_unlock(&mutex_i2);

                Mensaje respuesta;
                respuesta.id = m.id;
                respuesta.figura = m.figura;
                respuesta.segmento = m.segmento;

                // el intermediario usa el filesystem backup para emular otra mesa imaginaria a mini escala

                if(fileSystem_backup.find(m.figura) != fileSystem_backup.end()) {

                    respuesta.tipo = "FIGURE_FOUND";

                    Figura figura = fileSystem_backup[m.figura];

                    vector<Pieza> bloque;

                    if(m.segmento == 1) {

                        bloque = figura.segmento1;

                    } else {

                        bloque = figura.segmento2;
                    }

                    for(auto& bloq : bloque) {

                        respuesta.piezas.push_back({bloq.nombre, bloq.cantidad});
                    }

                } else {

                    // si tampoco existe en el server backup
                    respuesta.tipo = "FIGURE_NOT_FOUND";
                }

                pthread_mutex_lock(&mutex_i2);

                buzon_i2.push(respuesta);

                pthread_mutex_unlock(&mutex_i2);

                continue;
            }
        }

        pthread_mutex_unlock(&mutex_i2);

        usleep(100000);
    }

    return NULL;
}


void* servidor(void* arg) {

    while(!terminado) {

        pthread_mutex_lock(&mutex_is);

        if(!buzon_is.empty()) {

            Mensaje m = buzon_is.front();

            if(m.tipo == "exit") {

                buzon_is.pop();
                pthread_mutex_unlock(&mutex_is);

                cout << "Servidor finalizando..." << endl;
                
                break;
            }

            if(m.tipo == "ASK_FIGURE") {

                buzon_is.pop();

                pthread_mutex_unlock(&mutex_is);

                cout << "Servidor procesa solicitud " << m.tipo << " de cliente " << m.id << endl;

                Mensaje respuesta;
                respuesta.id = m.id;
                respuesta.figura = m.figura;
                respuesta.segmento = m.segmento;

                // ESTE SI USA EL FILESYSTEM MAIN

                if(m.figura.empty()) {

                    respuesta.tipo = "FIGURE_NOT_FOUND";
                    writeLog("SERVIDOR", "ERROR", "Error procesando solicitud ASK_FIGURE");

                } else if(m.figura == "ALL") {

                    respuesta.tipo = "FIGURE_FOUND";

                    for(auto& catalogo : fileSystem_main) {

                        respuesta.figuras.push_back(catalogo.first);
                    }

                } else if(fileSystem_main.find(m.figura) != fileSystem_main.end()) {

                    respuesta.tipo = "FIGURE_FOUND";

                    Figura figura = fileSystem_main[m.figura];

                    vector<Pieza> bloque;

                    if(m.segmento == 1) {

                        bloque = figura.segmento1;

                    } else {

                        bloque = figura.segmento2;
                    }

                    for(auto& bloq : bloque) {

                        respuesta.piezas.push_back({bloq.nombre, bloq.cantidad});
                    }

                } else {

                    // esto actuva la secuancia del fallback
                    respuesta.tipo = "FIGURE_NOT_FOUND";
                    respuesta.figura = m.figura;
                    respuesta.segmento = m.segmento;
                }

                pthread_mutex_lock(&mutex_is);
                buzon_is.push(respuesta);
                pthread_mutex_unlock(&mutex_is);

                cout << "Servidor respondio solicitud " << m.tipo << " de cliente " << m.id << endl;

                if(respuesta.tipo == "FIGURE_FOUND") {

                    if(m.figura == "ALL") {

                        writeLog("SERVIDOR", "RESPUESTA", "FIGURE_FOUND figura=ALL");
                    } else {

                        writeLog("SERVIDOR", "RESPUESTA", "FIGURE_FOUND figura=" + m.figura + " segmento=" + to_string(m.segmento));
                    }
                } else if(respuesta.tipo == "FIGURE_NOT_FOUND") {
                    
                    writeLog("SERVIDOR", "RESPUESTA", "FIGURE_NOT_FOUND figura=" + m.figura + " segmento=" + to_string(m.segmento));
                }

                continue;
            }
        }

        pthread_mutex_unlock(&mutex_is);

        usleep(100000);
    }

    pthread_exit(NULL);
}


int main() {

    Figura deathStar;

    deathStar.nombre = "death star ii";
    deathStar.meta = {16, "2026-03-20", "2026-03-20"};

    // MOCKUPS
    // segmento 1
    deathStar.segmento1.push_back({"brick 2x2 light gray", 1});
    deathStar.segmento1.push_back({"brick 2x2 dark gray", 2});
    deathStar.segmento1.push_back({"brick 2x3 light gray", 1});
    deathStar.segmento1.push_back({"brick 2x3 dark gray", 1});
    deathStar.segmento1.push_back({"brick 2x4 light gray", 2});
    deathStar.segmento1.push_back({"brick 2x4 dark gray", 1});
    deathStar.segmento1.push_back({"brick 2x6 light gray",1});
    deathStar.segmento1.push_back({"brick 2x6 dark gray", 1});
    deathStar.segmento1.push_back({"plate 2x2 black", 1});

    // segmento 2
    deathStar.segmento2.push_back({"plate 2x3 dark gray", 1});
    deathStar.segmento2.push_back({"plate 2x4 light gray", 1});
    deathStar.segmento2.push_back({"plate 2x4 black",1});
    deathStar.segmento2.push_back({"tile 2x2 dark gray", 1});
    deathStar.segmento2.push_back({"tile 2x3 light gray", 1});
    deathStar.segmento2.push_back({"slope 2x2 dark gray",1});
    deathStar.segmento2.push_back({"slope 2x3 light gray", 1});
    deathStar.segmento2.push_back({"round 2x2 light gray", 1});
    deathStar.segmento2.push_back({"round 2x2 transparent green", 1});

    // guardar en filesystem
    fileSystem_main["death star ii"] = deathStar;

    // inicializar servidores
    servidores["server1"].push_back("death star ii");



    Figura millenniumFalcon;

    millenniumFalcon.nombre = "millennium falcon";
    millenniumFalcon.meta = {20, "2026-03-20", "2026-03-20"};

    // MOCKUPS
    // segmento 1
    millenniumFalcon.segmento1.push_back({"brick 2x2 light gray", 2});
    millenniumFalcon.segmento1.push_back({"brick 2x3 light gray", 2});
    millenniumFalcon.segmento1.push_back({"brick 2x4 light gray", 2});
    millenniumFalcon.segmento1.push_back({"brick 2x2 dark gray", 1});
    millenniumFalcon.segmento1.push_back({"brick 2x3 dark gray", 1});
    millenniumFalcon.segmento1.push_back({"plate 2x4 light gray", 2});
    millenniumFalcon.segmento1.push_back({"plate 2x2 dark gray", 1});
    millenniumFalcon.segmento1.push_back({"round 2x2 light gray", 1});
    millenniumFalcon.segmento1.push_back({"round 2x2 transparent blue", 1});
    millenniumFalcon.segmento1.push_back({"tile 2x2 dark gray", 1});

    // segmento 2
    millenniumFalcon.segmento2.push_back({"plate 2x3 light gray", 2});
    millenniumFalcon.segmento2.push_back({"plate 2x4 dark gray", 1});
    millenniumFalcon.segmento2.push_back({"tile 2x3 light gray", 1});
    millenniumFalcon.segmento2.push_back({"tile 2x4 dark gray", 1});
    millenniumFalcon.segmento2.push_back({"slope 2x2 light gray", 2});
    millenniumFalcon.segmento2.push_back({"slope 2x3 dark gray", 1});
    millenniumFalcon.segmento2.push_back({"wedge 2x2 light gray", 2});
    millenniumFalcon.segmento2.push_back({"wedge 2x3 light gray", 1});
    millenniumFalcon.segmento2.push_back({"plate 1x4 dark gray", 1});
    millenniumFalcon.segmento2.push_back({"tile 1x4 black", 1});


    // guardar en filesystem
    fileSystem_backup["millennium falcon"] = millenniumFalcon;

    // inicializar servidores
    servidores["server2"].push_back("millennium falcon");



    Figura imperialDestroyer;

    imperialDestroyer.nombre = "imperial destroyer";
    imperialDestroyer.meta = {25, "2026-03-20", "2026-03-20"};

    // MOCKUPS
    // segmento 1
    imperialDestroyer.segmento1.push_back({"brick 2x2 light gray", 2});
    imperialDestroyer.segmento1.push_back({"brick 2x2 dark gray", 1});
    imperialDestroyer.segmento1.push_back({"brick 2x3 light gray", 2});
    imperialDestroyer.segmento1.push_back({"brick 2x3 dark gray", 1});
    imperialDestroyer.segmento1.push_back({"brick 2x4 light gray", 2});
    imperialDestroyer.segmento1.push_back({"brick 2x4 dark gray", 1});
    imperialDestroyer.segmento1.push_back({"brick 2x6 light gray", 1});
    imperialDestroyer.segmento1.push_back({"plate 2x4 light gray", 2});
    imperialDestroyer.segmento1.push_back({"plate 2x3 light gray", 1});
    imperialDestroyer.segmento1.push_back({"plate 2x2 dark gray", 1});
    imperialDestroyer.segmento1.push_back({"tile 2x2 dark gray", 1});
    imperialDestroyer.segmento1.push_back({"round 2x2 light gray", 1});

    // segmento 2
    imperialDestroyer.segmento2.push_back({"wedge 2x4 light gray", 2});
    imperialDestroyer.segmento2.push_back({"wedge 2x3 light gray", 2});
    imperialDestroyer.segmento2.push_back({"slope 2x3 light gray", 2});
    imperialDestroyer.segmento2.push_back({"slope 2x2 dark gray", 1});
    imperialDestroyer.segmento2.push_back({"tile 2x4 light gray", 1});
    imperialDestroyer.segmento2.push_back({"tile 2x3 dark gray", 1});
    imperialDestroyer.segmento2.push_back({"plate 1x6 light gray", 1});
    imperialDestroyer.segmento2.push_back({"plate 1x4 dark gray", 1});
    imperialDestroyer.segmento2.push_back({"round 2x2 light gray", 1});
    imperialDestroyer.segmento2.push_back({"round 1x1 dark gray", 2});
    imperialDestroyer.segmento2.push_back({"tile 1x4 black", 1});
    imperialDestroyer.segmento2.push_back({"plate 2x2 black", 1});
    imperialDestroyer.segmento2.push_back({"slope 1x2 light gray", 1});

    // guardar en filesystem
    fileSystem_main["imperial destroyer"] = imperialDestroyer;

    // inicializar servidores
    servidores["server1"].push_back("imperial destroyer");



    Figura speederBike;

    speederBike.nombre = "speeder bike";
    speederBike.meta = {14, "2026-03-20", "2026-03-20"};

    // MOCKUPS
    // segmento 1
    speederBike.segmento1.push_back({"brick 1x2 dark gray", 2});
    speederBike.segmento1.push_back({"brick 1x3 black", 1});
    speederBike.segmento1.push_back({"plate 1x4 dark gray", 1});
    speederBike.segmento1.push_back({"plate 1x3 black", 1});
    speederBike.segmento1.push_back({"tile 1x2 dark gray", 1});
    speederBike.segmento1.push_back({"slope 1x2 dark gray", 1});
    speederBike.segmento1.push_back({"round 1x1 black", 2});

    // segmento 2
    speederBike.segmento2.push_back({"plate 1x6 dark gray", 1});
    speederBike.segmento2.push_back({"tile 1x4 black", 1});
    speederBike.segmento2.push_back({"slope 1x2 black", 1});
    speederBike.segmento2.push_back({"bar 1x4 black", 1});
    speederBike.segmento2.push_back({"round 1x1 transparent red", 1});

    // guardar en filesystem
    fileSystem_main["speeder bike"] = speederBike;

    // inicializar servidores
    servidores["server1"].push_back("speeder bike");


    Figura c3po;

    c3po.nombre = "c3po";
    c3po.meta = {9, "2026-03-20", "2026-03-20"};

    // MOCKUPS
    // segmento 1
    c3po.segmento1.push_back({"brick 1x2 yellow", 2});
    c3po.segmento1.push_back({"plate 1x2 yellow", 1});
    c3po.segmento1.push_back({"tile 1x2 yellow", 1});
    c3po.segmento1.push_back({"round 1x1 yellow", 1});

    // segmento 2
    c3po.segmento2.push_back({"plate 1x3 yellow", 1});
    c3po.segmento2.push_back({"tile 1x3 yellow", 1});
    c3po.segmento2.push_back({"bar 1x2 yellow", 1});
    c3po.segmento2.push_back({"round 1x1 black", 1});

    // guardar en filesystem
    fileSystem_main["c3po"] = c3po;

    // inicializar servidores
    servidores["server1"].push_back("c3po");



    pthread_t hilos[6];

    pthread_mutex_init(&mutex_ci, NULL);
    pthread_mutex_init(&mutex_is, NULL);
    pthread_mutex_init(&mutex_i2, NULL);

    // hilos de descubrimiento
    pthread_create(&hilos[4], NULL, discovery_intermediario, NULL);
    pthread_create(&hilos[5], NULL, discovery_servidor, NULL);

    // el flujo principal
    {
        int espera = 0;
        while((!servidor_descubierto || !intermediario_descubierto) && espera < 30) {

            usleep(100000);
            espera++;
        }
        if(servidor_descubierto && intermediario_descubierto) {

            cout << "\n[MAIN] Descubrimiento exitoso. Iniciando flujo principal...\n" << endl;
        } else {

            cout << "\n[MAIN] ADVERTENCIA: descubrimiento incompleto. Continuando de todas formas.\n" << endl;
        }
    }

    // hilos principales
    pthread_create(&hilos[0], NULL, servidor, NULL);
    pthread_create(&hilos[1], NULL, intermediario, NULL);
    pthread_create(&hilos[2], NULL, intermediario2, NULL);
    pthread_create(&hilos[3], NULL, cliente, NULL);

    // join de hilos principales
    for(int i = 0; i < 4; i++) {

        pthread_join(hilos[i], NULL);
    }

    // join de hilos de descubrimiento
    for(int i = 4; i < 6; i++) {
        
        pthread_join(hilos[i], NULL);
    }

    pthread_mutex_destroy(&mutex_ci);
    pthread_mutex_destroy(&mutex_is);
    pthread_mutex_destroy(&mutex_i2);
    pthread_mutex_destroy(&mutex_discovery);
    pthread_mutex_destroy(&mutex_directorio);
    pthread_mutex_destroy(&mutex_log);

    return 0;
}
