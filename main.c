// Projeto: Simulador de Processos
// Grupo: Bruno Gustavo Rocha - 10400926; Gabriel de Jesus Silva Rodrigues - 10409071; Gabriel Jun Ito - 10427610

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int presente;
    int frame;
    int modificada;
    int referenciada;
    int tempoRecarga;
    int ultimoAcesso;
} Pagina;

typedef struct {
    int pid;
    int tamanho;
    int numPaginas;
    Pagina *tabelaPaginas;
} Processo;

typedef struct {
    int numFrames;
    int *frames;
    int *tempoCarga;
} MemoriaFisica;

typedef struct {
    int tempoAtual;
    int tamanhoPagina;
    int tamanhoMemoriaFisica;
    int numProcessos;
    Processo *processos;
    MemoriaFisica memoria;
    int totalAcessos;
    int pageFaults;
    int algoritmo;
} Simulador;
