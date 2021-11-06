//JAVIER GARCÍA PECHERO
//MIGUEL GONZALEZ TELLEZ DE MENESES
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <windows.h>
#include "cruce2.h"
//CONSTANTES PARA EL CORRECTO FUNCIONAMIENTO DEL PROGRAMA
#define MAX 127
#define MIN 2
#define ANCHO 5000
#define ALTO 1000
HANDLE semCruzando1;
HANDLE semCruzando2;
//FUNCIONES
int cargarDLL();
int crearIPCs();
void eliminarIPCs();
bool pos_ok(struct posiciOn);
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType);
DWORD WINAPI ciclo_semaforico(LPVOID parametro);
DWORD WINAPI peaton(LPVOID parametro);
struct posiciOn mov_peat(struct posiciOn pos, struct posiciOn* anterior);
struct posiciOn mov_coch(struct posiciOn pos, struct posiciOn* anterior);
DWORD WINAPI coche(LPVOID parametro);
//CREACIÓN DE LAS FUNCIONES PARA USAR LA LIBRERÍA
HINSTANCE libreria;
typedef int (*CRUCE_inicio) (int, int);
typedef int (*CRUCE_fin) (void);
typedef int (*CRUCE_gestor_inicio) (void);
typedef int (*CRUCE_pon_semAforo) (int, int);
typedef int (*CRUCE_nuevo_proceso) (void);
typedef struct posiciOn(*CRUCE_inicio_coche) (void);
typedef struct posiciOn(*CRUCE_avanzar_coche) (struct posiciOn);
typedef int (*CRUCE_fin_coche) (void);
typedef struct posiciOn(*CRUCE_nuevo_inicio_peatOn) (void);
typedef struct posiciOn(*CRUCE_avanzar_peatOn) (struct posiciOn);
typedef int  (*CRUCE_fin_peatOn) (void);
typedef int (*pausa) (void);
typedef int  (*pausa_coche) (void);
typedef void (*pon_error) (const char*);

struct funcionesDLL {
    CRUCE_inicio                cruceIni;
    CRUCE_fin                   cruceFin;
    CRUCE_gestor_inicio         cruceGestIni;
    CRUCE_pon_semAforo          crucePonSem;
    CRUCE_nuevo_proceso         cruceNuevoProc;
    CRUCE_inicio_coche          cruceIniCoche;
    CRUCE_avanzar_coche         cruceAvCoche;
    CRUCE_fin_coche             cruceFinCoche;
    CRUCE_nuevo_inicio_peatOn   cruceIniPeat;
    CRUCE_avanzar_peatOn        cruceAvPeat;
    CRUCE_fin_peatOn            cruceFinPeat;
    pausa                       pause;
    pausa_coche                 pauseCoche;
    pon_error                   error;
}fDLL;

//DECLARACIÓN DE HANDLES
//SEMAFOROS BIN
HANDLE semP1;
HANDLE semC1;
HANDLE semP2;
HANDLE semC2;
HANDLE semCruce;
HANDLE semPeat;
HANDLE semNacCocheH;
HANDLE semNacCocheV;
//SEMAFORO COORDENADAS
HANDLE semCoor[ALTO][ANCHO];
//SEMAFOROS NORMALES
HANDLE semProc;
//HANDLES DE FUNCIONES
HANDLE ciclosem;
HANDLE peat;
HANDLE coch;
//EVENTOS Y SECCION CRITICA
HANDLE ctrl;
CRITICAL_SECTION scp;
CRITICAL_SECTION scc;
//VARIABLES
DWORD fallo;
int proc;
int max_proc;
int vel;
//MAIN
int main(int argc, char* argv[]) {
    system("mode con:cols=80 lines=25"); //FIJA AUTOMÁTICAMENTE A 80x25
    setlocale(LC_ALL, "");
    int res;
    int tipo;
    //Manejadora
    SetConsoleCtrlHandler(CtrlHandler, TRUE);
    //ENLAZADO DE LIBRERÍA Y SUS FUNCIONES CON LA COMPROBACIÓN DE ERRORES
    if ((res = cargarDLL()) == 1) {
        FreeLibrary(libreria);
        exit(3);
    }
    //COMPROBACIÓN ARGUMENTOS
    if (argc != 3) {
        perror("Numero de argumentos erroneo, tienen que ser 2 argumentos obligatorios\n");
        FreeLibrary(libreria);
        exit(2);
    }
    max_proc = atoi(argv[1]);
    vel = atoi(argv[2]);
    //COMPROBACIÓN NUMERO PROCESOS
    if (max_proc > MAX || max_proc < MIN) {
        perror("Lo sentimos, ese numero esta fuera de los limites de procesos\n");
        FreeLibrary(libreria);
        exit(2);
    }
    //COMPROBACIÓN VELOCIDAD
    if (vel < 0) {
        perror("Lo sentimos, ese numero esta fuera de los limites de la velocidad\n");
        FreeLibrary(libreria);
        exit(2);
    }
    if ((res = crearIPCs()) == -1) {
        perror("Error creacion de IPCs");
        FreeLibrary(libreria);
        eliminarIPCs();
        exit(1);
    }
    //CRUCE INICIO
    if ((fDLL.cruceIni(vel, max_proc)) == -1) {
        perror("Error Cruce Inicio");
        FreeLibrary(libreria);
        eliminarIPCs();
        exit(1);
    }
    //CICLO SEMAFORICO
    fallo = WaitForSingleObject(semProc, INFINITE);
    ciclosem = CreateThread(NULL, 0, ciclo_semaforico, 0, 0, NULL);
    if (ciclosem == NULL) {
        perror("Error al crear el hilo");
        FreeLibrary(libreria);
        eliminarIPCs();
        exit(1);
    }
    int j = 0;
    //BUCLE INFINITO CREACION PROCESOS
    while (INFINITE) {
        tipo = fDLL.cruceNuevoProc();
        fallo = WaitForSingleObject(semProc, INFINITE);
        switch (tipo) {
        case PEAToN:
            peat = CreateThread(NULL, 0, peaton, 0, 0, NULL);
            if (peat == NULL) {
                perror("Error al crear el peaton");
                FreeLibrary(libreria);
                eliminarIPCs();
                exit(2);
            }
            break;
        case COCHE:
            coch = CreateThread(NULL, 0, coche, 0, 0, NULL);
            if (coch == NULL) {
                perror("Error al crear el coche");
                FreeLibrary(libreria);
                eliminarIPCs();
                exit(3);
            }
            break;
        }
    }
    return 0;
}

//CREAR IPCs
int crearIPCs() {
    //DECLARACION RECURSOS IPCS
    //Asignamos controlador a un EVENTO
    ctrl = CreateEvent(NULL, FALSE, FALSE, "ctrl");
    if (ctrl == NULL) {
        perror("Error al crear evento ctrl");
        return -1;
    }
    //SEMAFOROS
    semC1 = CreateSemaphore(NULL, 0, 1, NULL);
    if (semC1 == NULL) {
        perror("Error al crear semaforo C1");
        return -1;
    }

    semC2 = CreateSemaphore(NULL, 0, 1, NULL);
    if (semC2 == NULL) {
        perror("Error al crear semaforo C2");
        return -1;
    }

    semP1 = CreateSemaphore(NULL, 1, 1, NULL);
    if (semP1 == NULL) {
        perror("Error al crear semaforo P1");
        return -1;
    }

    semP2 = CreateSemaphore(NULL, 0, 1, NULL);
    if (semP2 == NULL) {
        perror("Error al crear semaforo P2");
        return -1;
    }

    semCruce = CreateSemaphore(NULL, 1, 1, NULL);
    if (semCruce == NULL) {
        perror("Error al crear semaforo CRUCE");
        return -1;
    }

    semNacCocheH = CreateSemaphore(NULL, 1, 1, NULL);
    if (semNacCocheH == NULL) {
        perror("Error al crear semaforo NacCoche");
        return -1;
    }

    semNacCocheV = CreateSemaphore(NULL, 1, 1, NULL);
    if (semNacCocheH == NULL) {
        perror("Error al crear semaforo NacCoche");
        return -1;
    }

    semCruzando1 = CreateSemaphore(NULL, 1, 1, NULL);
    if (semCruzando1 == NULL) {
        perror("Error al crear semaforo NacCoche");
        return -1;
    }

    semCruzando2 = CreateSemaphore(NULL, 1, 1, NULL);
    if (semCruzando2 == NULL) {
        perror("Error al crear semaforo NacCoche");
        return -1;
    }


    //Semaforo coordenadas
    int i, j;
    for (i = 0; i < ALTO; i++) {
        for (j = 0; j < ANCHO; j++) {
            semCoor[i][j] = (HANDLE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(HANDLE));
            semCoor[i][j] = CreateSemaphore(NULL, 1, 1, NULL);
            if (semCoor[i][j] == NULL) {
                perror("Error al crear semaforo");
                eliminarIPCs();
                FreeLibrary(libreria);
                exit(1);
            }
        }
    }
    //SEMAFORO NORMAL
    semProc = CreateSemaphore(NULL, max_proc - 1, max_proc - 1, NULL);
    if (semProc == NULL) {
        perror("Error al crear semaforo Procesos");
        return -1;
    }
    //CREACION SECCION CRITICA
    InitializeCriticalSection(&scp);
    InitializeCriticalSection(&scc);
    return 0;
}

//ELIMINAR IPCs
void eliminarIPCs() {
    int i, j;
    //HAY QUE ELIMINAR LOS SEMAFOROS BINARIOS DE COORDENADAS
    for (i = 0; i < ALTO; i++) {
        for (j = 0; j < ANCHO; j++) {
            CloseHandle(semCoor[i][j]);
        }
    }
    //Eliminamos todos los semáforos
    CloseHandle(semC1);
    CloseHandle(semC2);
    CloseHandle(semP1);
    CloseHandle(semP2);
    CloseHandle(semCruce);
    CloseHandle(semPeat);
    CloseHandle(semNacCocheH);
    CloseHandle(semNacCocheV);
    //EVENTOS Y SECCIONES CRITICAS
    CloseHandle(ctrl);
    DeleteCriticalSection(&scp);
    DeleteCriticalSection(&scc);
    FreeLibrary(libreria);
}


//CICLO SEMAFORICO
DWORD WINAPI ciclo_semaforico(LPVOID parametro) {
    int i;
    if ((fDLL.cruceGestIni()) == -1) {
        perror("Error cruceGestorInicio");
        FreeLibrary(libreria);
        eliminarIPCs();
        exit(1);
    }
    if ((fDLL.crucePonSem(SEM_C2, ROJO)) == -1) {
        perror("Error semaforo Rojo C2");
        FreeLibrary(libreria);
        eliminarIPCs();
        exit(1);
    }
    while (INFINITE) {
        //P1
        //Wait
        fallo = WaitForSingleObject(semP1, INFINITE);
        if (fallo == WAIT_FAILED) {
            perror("Error semP1 ciclosem");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        if ((fDLL.crucePonSem(SEM_P1, ROJO)) == -1) {
            perror("Error semaforo Rojo P1");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        if ((fDLL.crucePonSem(SEM_C1, VERDE)) == -1) {
            perror("Error semaforo Verde C1");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        if ((fDLL.crucePonSem(SEM_P2, VERDE)) == -1) {
            perror("Error semaforo Verde P2");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        //C1
        //Signal
        if (ReleaseSemaphore(semC1, 1, NULL) == 0) {
            perror("Error semC1 ciclosem");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }

        //P2
        //Signal
        if (ReleaseSemaphore(semP2, 1, NULL) == 0) {
            perror("Error semP2 ciclosem");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        for (i = 0; i < 6; i++) {
            fDLL.pause();
        }
        //C1
        //Wait
        fallo = WaitForSingleObject(semC1, INFINITE);
        if (fallo == WAIT_FAILED) {
            perror("Error semC1 ciclosem");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        //P2
        //Wait
        fallo = WaitForSingleObject(semP2, INFINITE);
        if (fallo == WAIT_FAILED) {
            perror("Error semP2 ciclosem");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        if ((fDLL.crucePonSem(SEM_C1, AMARILLO)) == -1) {
            perror("Error semaforo Amarillo C1");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        if ((fDLL.crucePonSem(SEM_P2, ROJO)) == -1) {
            perror("Error semaforo Rojo P2");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        fDLL.pause();
        fDLL.pause();
        //CRUCE
        //Wait
        fallo = WaitForSingleObject(semCruce, INFINITE);
        if (fallo == WAIT_FAILED) {
            perror("Error semCruce ciclosem");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        if ((fDLL.crucePonSem(SEM_C1, ROJO)) == -1) {
            perror("Error semaforo Rojo C1");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        //Signal
        if (ReleaseSemaphore(semCruce, 1, NULL) == 0) {
            perror("Error semCruce ciclosem");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        if ((fDLL.crucePonSem(SEM_C2, VERDE)) == -1) {
            perror("Error semaforo Verde C2");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        //C2
        //Signal
        if (ReleaseSemaphore(semC2, 1, NULL) == 0) {
            perror("Error semC2 ciclosem");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        for (i = 0; i < 9; i++) {
            fDLL.pause();
        }
        //Wait
        fallo = WaitForSingleObject(semC2, INFINITE);
        if (fallo == WAIT_FAILED) {
            perror("Error semC2 ciclosem");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        if ((fDLL.crucePonSem(SEM_C2, AMARILLO)) == -1) {
            perror("Error semaforo Amarillo C2");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        fDLL.pause();
        fDLL.pause();
        //CRUCE
        //Wait
        fallo = WaitForSingleObject(semCruce, INFINITE);
        if (fallo == WAIT_FAILED) {
            perror("Error semCruce ciclosem");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        if ((fDLL.crucePonSem(SEM_C2, ROJO)) == -1) {
            perror("Error semaforo Rojo C2");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        //Signal
        if (ReleaseSemaphore(semCruce, 1, NULL) == 0) {
            perror("Error semCruce ciclosem");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        if ((fDLL.crucePonSem(SEM_P1, VERDE)) == -1) {
            perror("Error semaforo Verde P1");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        //Signal P1
        if (ReleaseSemaphore(semP1, 1, NULL) == 0) {
            perror("Error semP1 ciclosem");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        for (i = 0; i < 12; i++) {
            fDLL.pause();
        }
    }
    return 1;
}

struct posiciOn mov_peat(struct posiciOn pos, struct posiciOn* anterior) {
    struct posiciOn sig;
    //Recibimos coordenadas de la posicion siguiente  
    //En frente de P2
    if (pos.x < 28 && pos.x > 20 && pos.y == 11) {
        fallo = WaitForSingleObject(semP2, INFINITE);
        if (fallo == WAIT_FAILED) {
            perror("Error semP2 movPeat");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        fallo = WaitForSingleObject(semCoor[(pos.y * 5)][(pos.x + 5)], INFINITE);
        if (fallo == WAIT_FAILED) {
            perror("Error semCoor Peaton");
            eliminarIPCs();
            FreeLibrary(libreria);
            exit(4);
        }
        fallo = WaitForSingleObject(semCruzando2, INFINITE);
        if (fallo == WAIT_FAILED) {
            perror("Error semCoor Peaton");
            eliminarIPCs();
            FreeLibrary(libreria);
            exit(4);
        }
        sig = fDLL.cruceAvPeat(pos);
        if (ReleaseSemaphore(semP2, 1, NULL) == 0) {
            perror("Error semP2 movPeat");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
    }
    else if (pos.y < 16 && pos.y > 12 && pos.x == 30) {
        //En frente de P1
        fallo = WaitForSingleObject(semP1, INFINITE);
        if (fallo == WAIT_FAILED) {
            perror("Error semP1 movPeat");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        fallo = WaitForSingleObject(semCoor[(pos.y * 5)][(pos.x + 5)], INFINITE);
        if (fallo == WAIT_FAILED) {
            perror("Error semCoor Peaton");
            eliminarIPCs();
            FreeLibrary(libreria);
            exit(4);
        }
        sig = fDLL.cruceAvPeat(pos);
        if (ReleaseSemaphore(semP1, 1, NULL) == 0) {
            perror("Error semP1 movPeat");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
    }
    else {
        fallo = WaitForSingleObject(semCoor[(pos.y * 5)][(pos.x + 5)], INFINITE);
        if (fallo == WAIT_FAILED) {
            perror("Error semCoor Peaton");
            eliminarIPCs();
            FreeLibrary(libreria);
            exit(4);
        }
        if (pos.y == 8 && pos.x < 30) {
            if (ReleaseSemaphore(semCruzando2, 1, NULL) == 0) {
                perror("Error semP1 movPeat");
                FreeLibrary(libreria);
                eliminarIPCs();
                exit(1);
            }
        }
        if (pos.x == 31) {
            fallo = WaitForSingleObject(semCruzando1, INFINITE);
            if (fallo == WAIT_FAILED) {
                perror("Error semCoor Peaton");
                eliminarIPCs();
                FreeLibrary(libreria);
                exit(4);
            }
        }
        if (pos.x == 40) {
            if (ReleaseSemaphore(semCruzando1, 1, NULL) == 0) {
                perror("Error semP1 movPeat");
                FreeLibrary(libreria);
                eliminarIPCs();
                exit(1);
            }
        }
        sig = fDLL.cruceAvPeat(pos);
    }
    //Enviamos coordenadas de la posicion anterior
    if (anterior->x != -1 && anterior->y != -1) {
        if (!ReleaseSemaphore(semCoor[anterior->y * 5][(anterior->x + 5)], 1, NULL)) {
            perror("Error semCoor Peaton");
            eliminarIPCs();
            FreeLibrary(libreria);
            exit(6);
        }
    }
    anterior->x = pos.x;
    anterior->y = pos.y;
    return sig;
}

//FUNCION PEATONES
DWORD WINAPI peaton(LPVOID parametro) {
    struct posiciOn pos, anterior = { -1,-1 };
    pos = fDLL.cruceIniPeat();
    pos = mov_peat(pos, &anterior);
    while (INFINITE) {
        pos = mov_peat(pos, &anterior);
        if (!pos_ok(pos)) break;
        fDLL.pause();
    }
    if (fDLL.cruceFinPeat() == -1) {
        perror("Error cruce Fin Peaton");
        FreeLibrary(libreria);
        eliminarIPCs();
        exit(3);
    }
    //Enviamos mensaje de las ultimas coordenadas
    if (!ReleaseSemaphore(semCoor[anterior.y * 5][(anterior.x + 5)], 1, NULL)) {
        perror("Error semCoor peat");
        eliminarIPCs();
        FreeLibrary(libreria);
        exit(6);
    }
    ReleaseSemaphore(semProc, 1, NULL);
    ExitThread(0);
}

//COMPROBAR POSICION
bool pos_ok(struct posiciOn pos) {
    if (pos.y < 0) {
        return FALSE;
    }
    else {
        return TRUE;
    }
}

//MOVIMIENTO COCHES
struct posiciOn mov_coch(struct posiciOn pos, struct posiciOn* anterior) {
    struct posiciOn sig;
    int i, j;
    if (pos.x == 33 && pos.y == 6) {
        //Semaforo C1
        fallo = WaitForSingleObject(semC1, INFINITE);
        if (fallo == WAIT_FAILED) {
            perror("Error semC1 movCoch");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(9);
        }
        //Block cruce
        fallo = WaitForSingleObject(semCruce, INFINITE);
        if (fallo == WAIT_FAILED) {
            perror("Error semCruce ciclosem");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(1);
        }
        //VERTICAL
        for (i = 0; i < 6; i++) {
            fallo = WaitForSingleObject(semCoor[(pos.y * 5 + i)][(pos.x + 5) + i], INFINITE);
            if (fallo == WAIT_FAILED) {
                perror("Error semCoor MovCoche C1");
                eliminarIPCs();
                FreeLibrary(libreria);
                exit(4);
            }
        }
        sig = fDLL.cruceAvCoche(pos);
        if (!(ReleaseSemaphore(semC1, 1, NULL))) {
            perror("Error semC1 movCoch");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(9);
        }
        if (!ReleaseSemaphore(semNacCocheV, 1, NULL)) {
            perror("Error semCruce semNacCocheV");
            eliminarIPCs();
            FreeLibrary(libreria);
            exit(6);
        }
    }
    else if (pos.x == 13 && pos.y == 10) {
        //Semaforo C2
        fallo = WaitForSingleObject(semC2, INFINITE);
        if (fallo == WAIT_FAILED) {
            perror("Error semC2 movCoch");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(9);
        }
        //HORIZONTAL
        fallo = WaitForSingleObject(semCruzando2, INFINITE);
        if (fallo == WAIT_FAILED) {
            perror("Error semCoor C2 Horizontal");
            eliminarIPCs();
            FreeLibrary(libreria);
            exit(4);
        }
        for (i = 0; i < 9; i++) {
            for (j = 0; j < 3; j++) {
                fallo = WaitForSingleObject(semCoor[(pos.y * 5) - j][(pos.x + 5) + i], INFINITE);
                if (fallo == WAIT_FAILED) {
                    perror("Error semCoor C2 Horizontal");
                    eliminarIPCs();
                    FreeLibrary(libreria);
                    exit(4);
                }
            }
        } 
        sig = fDLL.cruceAvCoche(pos);
        if (!ReleaseSemaphore(semNacCocheH, 1, NULL)) {
            perror("Error semCruce semNacCocheH");
            eliminarIPCs();
            FreeLibrary(libreria);
            exit(6);
        }
        if (!(ReleaseSemaphore(semC2, 1, NULL))) {
            perror("Error semC2 movCoch");
            FreeLibrary(libreria);
            eliminarIPCs();
            exit(9);
        }
    }
    else {
        if (pos.x == 23 && pos.y == 10) {
            //ANTES DE ENTRAR AL CRUCE DESDE LA HORIZONTAL
            //Block cruce
            fallo = WaitForSingleObject(semCruce, INFINITE);
            if (fallo == WAIT_FAILED) {
                perror("Error semCruce ciclosem");
                FreeLibrary(libreria);
                eliminarIPCs();
                exit(1);
            }
            for (i = 0; i < 9; i++) {
                for (j = 0; j < 3; j++) {
                    fallo = WaitForSingleObject(semCoor[(pos.y * 5) - j][(pos.x + 5) + i], INFINITE);
                    if (fallo == WAIT_FAILED) {
                        perror("Error semCoor C2 Horizontal Cruce");
                        eliminarIPCs();
                        FreeLibrary(libreria);
                        exit(4);
                    }
                }
            }           
            sig = fDLL.cruceAvCoche(pos);
            if (ReleaseSemaphore(semCruzando2, 1, NULL) == 0) {
                perror("Error semP1 movPeat");
                FreeLibrary(libreria);
                eliminarIPCs();
                exit(1);
            }
        }
        else if (pos.x == 33 && pos.y == 10 && anterior->y == 10 && anterior->x == 31) {          
            sig = fDLL.cruceAvCoche(pos);
            if (sig.x == 33 && sig.y == 12) {
                for (i = 0; i < 6; i++) {
                    fallo = WaitForSingleObject(semCoor[(pos.y * 5 + i)][(pos.x + 5) + i], INFINITE);
                    if (fallo == WAIT_FAILED) {
                        perror("Error semCoor C2 Horizontal Cruce Dentro");
                        eliminarIPCs();
                        FreeLibrary(libreria);
                        exit(4);
                    }
                }
            }
        }
        else if (pos.x < 33 && pos.y == 10) {        
            if (pos.x <= -3) {
                fallo = WaitForSingleObject(semNacCocheH, INFINITE);
                if (fallo == WAIT_FAILED) {
                    perror("Error semNac Horizontal Principio");
                    eliminarIPCs();
                    FreeLibrary(libreria);
                    exit(4);
                }
            }
            sig = fDLL.cruceAvCoche(pos);
        }
        else if (pos.y <= 22 && pos.x == 33) {
            //Vertical
            if (pos.y == 12) {
                fallo = WaitForSingleObject(semCruzando1, INFINITE);
                if (fallo == WAIT_FAILED) {
                    perror("Error semCoor C2 Horizontal");
                    eliminarIPCs();
                    FreeLibrary(libreria);
                    exit(4);
                }
            }
            for (i = 0; i < 6; i++) {
                fallo = WaitForSingleObject(semCoor[(pos.y * 5 + i)][(pos.x + 5) + i], INFINITE);
                if (fallo == WAIT_FAILED) {
                    perror("Error semCoor C2 Vertical Principio");
                    eliminarIPCs();
                    FreeLibrary(libreria);
                    exit(4);
                }
            }
            if (pos.y <= 1) {
                fallo = WaitForSingleObject(semNacCocheV, INFINITE);
                if (fallo == WAIT_FAILED) {
                    perror("Error semNac Horizontal Principio");
                    eliminarIPCs();
                    FreeLibrary(libreria);
                    exit(4);
                }
            }        
            if (pos.y == 20) {
                if (ReleaseSemaphore(semCruzando1, 1, NULL) == 0) {
                    perror("Error semP1 movPeat");
                    FreeLibrary(libreria);
                    eliminarIPCs();
                    exit(1);
                }
            }
            sig = fDLL.cruceAvCoche(pos);
        }
    }

    if (pos.x == 33 && pos.y == 16) {
        //Salida Cruce
        if (!ReleaseSemaphore(semCruce, 1, NULL)) {
            perror("Error semCruce MovCoche");
            eliminarIPCs();
            FreeLibrary(libreria);
            exit(6);
        }
    }

    if (pos.x == 33 && pos.y == 10 && anterior->y == 10 && anterior->x == 31) {
        if (sig.x == 33 && sig.y == 12) {

        }
        else {
            for (i = 0; i < 9; i++) {
                for (j = 0; j < 3; j++) {
                    if (!ReleaseSemaphore(semCoor[(anterior->y * 5) - j][(anterior->x + 5) + i], 1, NULL)) {
                        perror("Error semCoor MovCoche giro");
                        eliminarIPCs();
                        FreeLibrary(libreria);
                        exit(6);
                    }
                }
            }
        }
    }
    else if (anterior->x == 33 && anterior->y == 6) {
        for (i = 0; i < 6; i++) {
            if (!ReleaseSemaphore(semCoor[(anterior->y * 5 + i)][(anterior->x + 5) + i], 1, NULL)) {
                perror("Error semCoor MovCoche");
                eliminarIPCs();
                FreeLibrary(libreria);
                exit(6);
            }
        }
    }
    else if (anterior->x == 13 && anterior->y == 10) {
        for (i = 0; i < 9; i++) {
            for (j = 0; j < 3; j++) {
                if (!ReleaseSemaphore(semCoor[(anterior->y * 5) - j][(anterior->x + 5) + i], 1, NULL)) {
                    perror("Error semCoor C2 Horizontal");
                    eliminarIPCs();
                    FreeLibrary(libreria);
                    exit(4);
                }
            }
        }
    }
    else if (anterior->x == 23 && anterior->y == 10) {
        for (i = 0; i < 9; i++) {
            for (j = 0; j < 3; j++) {
                if (!ReleaseSemaphore(semCoor[(anterior->y * 5) - j][(anterior->x + 5) + i], 1, NULL)) {
                    perror("Error semCoor C2 Horizontal");
                    eliminarIPCs();
                    FreeLibrary(libreria);
                    exit(4);
                }
            }
        }
    }
    else if (anterior->x < 33 && anterior->y == 10) {
    
    }
    else if (anterior->x > 31 && anterior->y <= 21) {
        for (i = 0; i < 6; i++) {
            if (!ReleaseSemaphore(semCoor[(anterior->y * 5 + i)][(anterior->x + 5) + i], 1, NULL)) {
                perror("Error semCoor MovCoche");
                eliminarIPCs();
                FreeLibrary(libreria);
                exit(6);
            }
        }
    }
    anterior->x = pos.x;
    anterior->y = pos.y;
    return sig;
}

//FUNCION COCHE
DWORD WINAPI coche(LPVOID parametro) {
    struct posiciOn pos, anterior = { -1,-1 };
    int i;
    pos = fDLL.cruceIniCoche();
    while (INFINITE) {
        pos = mov_coch(pos, &anterior);
        if (!pos_ok(pos)) break;
        fDLL.pauseCoche();
    }

    if (fDLL.cruceFinCoche() == -1) {
        perror("Error cruce Fin Coche");
        FreeLibrary(libreria);
        eliminarIPCs();
        exit(3);
    }

    for (i = 0; i < 6; i++) {
        if (!ReleaseSemaphore(semCoor[(anterior.y * 5 + i)][(anterior.x + 5) + i], 1, NULL)) {
            perror("Error semCoor MovCoche");
            eliminarIPCs();
            FreeLibrary(libreria);
            exit(6);
        }
    }
    ReleaseSemaphore(semProc, 1, NULL);
    ExitThread(0);
}

//MANEJADORA
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    switch (fdwCtrlType) {
        // Handle the CTRL-C signal.
    case CTRL_C_EVENT:
        if ((fDLL.cruceFin()) == -1) {
            perror("Error fin");
        }
        FreeLibrary(libreria);
        eliminarIPCs();
        return TRUE;
    default:
        return FALSE;
    }
}

//FUNCION CARGA LIBRERIA
int cargarDLL(void) {
    //CARGA LIBRERIA
    libreria = LoadLibrary("cruce2.dll");
    if (libreria == NULL) {
        perror("LOADLIBRARY");
        return 1;
    }
    //CARGA FUNCIONES
    fDLL.cruceIni = (CRUCE_inicio)GetProcAddress(libreria, "CRUCE_inicio");
    if (fDLL.cruceIni == NULL) {
        perror("Error carga cruceInicio");
        return 1;
    }

    fDLL.cruceFin = (CRUCE_fin)GetProcAddress(libreria, "CRUCE_fin");
    if (fDLL.cruceFin == NULL) {
        perror("Error carga cruceFin");
        return 1;
    }

    fDLL.cruceGestIni = (CRUCE_gestor_inicio)GetProcAddress(libreria, "CRUCE_gestor_inicio");
    if (fDLL.cruceGestIni == NULL) {
        perror("Error carga cruceGestIni");
        return 1;
    }

    fDLL.crucePonSem = (CRUCE_pon_semAforo)GetProcAddress(libreria, "CRUCE_pon_semAforo");
    if (fDLL.crucePonSem == NULL) {
        perror("Error carga crucePonSem");
        return 1;
    }

    fDLL.cruceNuevoProc = (CRUCE_nuevo_proceso)GetProcAddress(libreria, "CRUCE_nuevo_proceso");
    if (fDLL.cruceNuevoProc == NULL) {
        perror("Error carga cruceNuevoProc");
        return 1;
    }

    fDLL.cruceIniCoche = (CRUCE_inicio_coche)GetProcAddress(libreria, "CRUCE_inicio_coche");
    if (fDLL.cruceIniCoche == NULL) {
        perror("Error carga cruceIniCoche");
        return 1;
    }

    fDLL.cruceAvCoche = (CRUCE_avanzar_coche)GetProcAddress(libreria, "CRUCE_avanzar_coche");
    if (fDLL.cruceAvCoche == NULL) {
        perror("Error carga cruceAvCoche");
        return 1;
    }

    fDLL.cruceFinCoche = (CRUCE_fin_coche)GetProcAddress(libreria, "CRUCE_fin_coche");
    if (fDLL.cruceFinCoche == NULL) {
        perror("Error carga cruceFinCoche");
        return 1;
    }

    fDLL.cruceIniPeat = (CRUCE_nuevo_inicio_peatOn)GetProcAddress(libreria, "CRUCE_nuevo_inicio_peatOn");
    if (fDLL.cruceIniPeat == NULL) {
        perror("Error carga cruceIniPeat");
        return 1;
    }

    fDLL.cruceAvPeat = (CRUCE_avanzar_peatOn)GetProcAddress(libreria, "CRUCE_avanzar_peatOn");
    if (fDLL.cruceAvPeat == NULL) {
        perror("Error carga cruceAvPeat");
        return 1;
    }

    fDLL.cruceFinPeat = (CRUCE_fin_peatOn)GetProcAddress(libreria, "CRUCE_fin_peatOn");
    if (fDLL.cruceFinPeat == NULL) {
        perror("Error carga cruceFinPeat");
        return 1;
    }

    fDLL.pause = (pausa)GetProcAddress(libreria, "pausa");
    if (fDLL.pause == NULL) {
        perror("Error carga pausa");
        return 1;
    }

    fDLL.pauseCoche = (pausa_coche)GetProcAddress(libreria, "pausa_coche");
    if (fDLL.pauseCoche == NULL) {
        perror("Error carga pausaCoche");
        return 1;
    }

    fDLL.error = (pon_error)GetProcAddress(libreria, "pon_error");
    if (fDLL.error == NULL) {
        perror("Error carga funcion error");
        return 1;
    }

    return 0;
}






