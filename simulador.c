#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int presente; // 1 se a página está na memória , 0 caso contrário
    int frame; // Número do frame onde a página está alocada (-1 se não alocada)
    int modificada; // 1 se a página foi modificada , 0 caso contrário
    int referenciada; // 1 se a página foi referenciada recentemente , 0 caso contrário
    int tempo_carga; // Instante em que a página foi carregada na memória
    int ultimo_acesso; // Instante do último acesso à página
} Pagina;

typedef struct {
    int pid; // Identificador do processo
    int tamanho; // Tamanho do processo em bytes
    int num_paginas; // Número de páginas do processo
    Pagina *tabela_paginas; // Tabela de páginas do processo
} Processo;

typedef struct {
    int num_frames; // Número total de frames na memória física
    int *frames; // Array de frames (cada elemento contém o pid e a página)
    // Ex: frames[i] = (pid << 16) | num_pagina
    int *tempo_carga; // Tempo em que cada frame foi carregado (para FIFO)
} MemoriaFisica;

typedef struct {
    int tempo_atual; // Contador de tempo da simulação
    int tamanho_pagina; // Tamanho da página em bytes
    int tamanho_memoria_fisica; // Tamanho da memória física em bytes
    int num_processos; // Número de processos na simulação
    Processo *processos; // Array de processos
    MemoriaFisica memoria; // Memória física

    // Estatísticas
    int total_acessos; // Total de acessos à memória
    int page_faults; // Total de page faults ocorridos

    // Algoritmo de substituição atual
    int algoritmo; // 0=FIFO , 1=LRU , 2=CLOCK , 3=RANDOM , 4= CUSTOM
} Simulador;

// Inicializa o simulador com os parâmetros fornecidos
Simulador* inicializar_simulador(int tamanho_pagina , int tamanho_memoria_fisica){
    Simulador *sim = (Simulador *)malloc(sizeof(Simulador));
    sim->tempo_atual = 0;
    sim->tamanho_pagina = tamanho_pagina;
    sim->tamanho_memoria_fisica = tamanho_memoria_fisica;
    sim->num_processos = 0;
    sim->memoria.num_frames = tamanho_memoria_fisica / tamanho_pagina;
    sim->memoria.frames = (int *)malloc(sizeof(int) * sim->memoria.num_frames);
    sim->memoria.tempo_carga = (int *)malloc(sizeof(int) * sim->memoria.num_frames);
    for (int i = 0; i < sim->memoria.num_frames; i++) {
        sim->memoria.frames[i] = -1;
        sim->memoria.tempo_carga[i] = -1;
    }
    sim->total_acessos = 0;
    sim->page_faults = 0;
    
    sim->processos = NULL;
    sim->algoritmo = 0; // Adicionar escolha de algoritmo
    return sim;
}
    
// Cria um novo processo e o adiciona ao simulador
Processo* criar_processo(Simulador *sim , int tamanho_processo){
    Processo proc;
    proc.pid = sim->num_processos;
    proc.tamanho = tamanho_processo; 
    proc.num_paginas = (tamanho_processo+1)/sim->tamanho_pagina; 
    proc.tabela_paginas = (Pagina *)malloc(sizeof(Pagina) * proc.num_paginas);

    for (int i = 0; i < proc.num_paginas; i++) {
        proc.tabela_paginas[i].presente = 0;
        proc.tabela_paginas[i].frame = -1;
        proc.tabela_paginas[i].modificada = 0;
        proc.tabela_paginas[i].referenciada = 0;
        proc.tabela_paginas[i].tempo_carga = 0;
        proc.tabela_paginas[i].ultimo_acesso = 0;
    }

    sim->processos = (Processo *)realloc(sim->processos, sizeof(Processo) * (sim->num_processos + 1));
    sim->processos[sim->num_processos] = proc;
    sim->num_processos++;
    printf("Processo %d criado: %d páginas (%d bytes)\n", proc.pid, proc.num_paginas, proc.tamanho);
}

int substituir_pagina_fifo(Simulador *sim);
int acessar_memoria(Simulador *sim , int pid , int endereco_virtual);

// Extrai o número da página e o deslocamento de um endereço virtual
void extrair_pagina_deslocamento(Simulador *sim , int endereco_virtual , int *pagina , int *deslocamento){
    *pagina = endereco_virtual / sim->tamanho_pagina;
    *deslocamento = endereco_virtual % sim->tamanho_pagina;
}

// Verifica se uma página está presente na memória física
int verificar_pagina_presente(Simulador *sim , int pid , int pagina){
    return sim->processos[pid].tabela_paginas[pagina].presente;
}
// Carrega uma página na memória física
// Retorna o número do frame onde a página foi carregada
int carregar_pagina(Simulador *sim , int pid , int pagina){
    int frame = -1;

    for (int i = 0; i < sim->memoria.num_frames; i++) {
        if (sim->memoria.frames[i] == -1) {
            frame = i;
            break;
        }
    }

    if (frame == -1) {
        switch (sim->algoritmo) {
            case 0:
                frame = substituir_pagina_fifo(sim);
                break;
        }
    }

    printf("Tempo t=%d: Carregando Página %d do Processo %d no Frame %d\n",
           sim->tempo_atual, pagina, pid + 1, frame);

    sim->memoria.frames[frame] = (pid << 16) | pagina;
    sim->memoria.tempo_carga[frame] = sim->tempo_atual;

    Pagina *pag = &sim->processos[pid].tabela_paginas[pagina];
    pag->presente = 1;
    pag->frame = frame;
    pag->tempo_carga = sim->tempo_atual;
    pag->ultimo_acesso = sim->tempo_atual;

    return frame;
}

// Traduz um endereço virtual para físico
// Retorna o endereço físico ou -1 em caso de page fault
int traduzir_endereco(Simulador *sim , int pid , int endereco_virtual){
    int pagina, deslocamento;
    extrair_pagina_deslocamento(sim, endereco_virtual, &pagina, &deslocamento);
    sim->total_acessos++;

    if (!verificar_pagina_presente(sim, pid, pagina)) {
        sim->page_faults++;
        carregar_pagina(sim, pid, pagina);
    }

    int frame = sim->processos[pid].tabela_paginas[pagina].frame;
    return frame * sim->tamanho_pagina + deslocamento;
}

// Implementa o algoritmo de substituição de páginas FIFO
int substituir_pagina_fifo(Simulador *sim){
    int mais_antigo = sim->tempo_atual + 1;
    int frame_substituir = -1;

    for (int i = 0; i < sim->memoria.num_frames; i++) {
        if (sim->memoria.tempo_carga[i] < mais_antigo) {
            mais_antigo = sim->memoria.tempo_carga[i];
            frame_substituir = i;
        }
    }

    int info = sim->memoria.frames[frame_substituir];
    int pid = info >> 16;
    int pag = info & 0xFFFF;

    printf("Tempo t=%d: Substituindo Página %d do Processo %d no Frame %d pela nova página (FIFO)\n",
           sim->tempo_atual, pag, pid + 1, frame_substituir);

    sim->processos[pid].tabela_paginas[pag].presente = 0;
    sim->processos[pid].tabela_paginas[pag].frame = -1;

    return frame_substituir;
}

// Implementa o algoritmo de substituição de páginas LRU
int substituir_pagina_lru(Simulador *sim);
// Implementa o algoritmo de substituição de páginas CLOCK
int substituir_pagina_clock(Simulador *sim);
// Implementa o algoritmo de substituição de páginas RANDOM
int substituir_pagina_random(Simulador *sim);


// Exibe o estado atual da memória física
void exibir_memoria_fisica(Simulador *sim){
    printf("Estado da Memória Física:\n");
    for (int i = 0; i < sim->memoria.num_frames; i++) {
        printf("--------\n");
        if (sim->memoria.frames[i] != -1) {
            int pid = sim->memoria.frames[i] >> 16;
            int pag = sim->memoria.frames[i] & 0xFFFF;
            printf("| P%d-%d |\n", pid + 1, pag);
        } else {
            printf("| ---- |\n");
        }
    }
    printf("--------\n");
}
// Exibe estatísticas da simulação
void exibir_estatisticas(Simulador *sim){
    printf("\n======== ESTATÍSTICAS DA SIMULAÇÃO ========\n");
    printf("Total de acessos à memória: %d\n", sim->total_acessos);
    printf("Total de page faults: %d\n", sim->page_faults);
    printf("Taxa de page faults: %.2f%%\n", 100.0 * sim->page_faults / sim->total_acessos);
    switch (sim->algoritmo){
        case 0:
            printf("Algoritmo: FIFO\n");
            break;
        case 1:
            printf("Algoritmo: LRU\n");
            break;
        case 2:
            printf("Algoritmo: CLOCK\n");
            break;
        case 3:
            printf("Algoritmo: RANDOM\n");
            break;
        case 4:
            printf("Algoritmo: CUSTOM\n");
            break;
    }
}

// Registra um acesso à memória
void registrar_acesso(Simulador *sim , int pid , int pagina , int tipo_acesso){
    Pagina *pag = &sim->processos[pid].tabela_paginas[pagina];
    pag->referenciada = 1;
    if (tipo_acesso == 1) {
        pag->modificada = 1;
    }
    pag->ultimo_acesso = sim->tempo_atual;
}

// Executa a simulação com uma sequência de acessos à memória
void executar_simulacao(Simulador *sim , int algoritmo){
    sim->algoritmo = algoritmo;

    printf("===== SIMULADOR DE PAGINAÇÃO =====\n");
    printf("Tamanho da página: %d bytes (%d KB)\n", sim->tamanho_pagina, sim->tamanho_pagina / 1024);
    printf("Tamanho da memória física: %d bytes (%d KB)\n", sim->tamanho_memoria_fisica, sim->tamanho_memoria_fisica / 1024);
    printf("Número de frames: %d\n", sim->memoria.num_frames);
    printf("Algoritmo de substituição: ");

    switch (algoritmo){
        case 0: printf("FIFO\n"); break;
        case 1: printf("LRU\n"); break;
        case 2: printf("CLOCK\n"); break;
        case 3: printf("RANDOM\n"); break;
        case 4: printf("CUSTOM\n"); break;
    }

    printf("======== INÍCIO DA SIMULAÇÃO ========\n\n");
    
    for (int i = 0; i < sim->num_processos; i++) {
        Processo *p = &sim->processos[i];
        int pid = p->pid; 

        for (int j = 0; j < p->num_paginas && j < 4; j++) {
            int endereco_virtual = j * sim->tamanho_pagina; 
            acessar_memoria(sim, pid, endereco_virtual);
        }

        if (p->num_paginas > 0) {
            acessar_memoria(sim, pid, 0); 
        }
    }

    exibir_estatisticas(sim);
}

// Simula um acesso à memória
int acessar_memoria(Simulador *sim , int pid , int endereco_virtual){
    int pagina, deslocamento;
    extrair_pagina_deslocamento(sim, endereco_virtual, &pagina, &deslocamento);
    sim->total_acessos++;

    if (!verificar_pagina_presente(sim, pid, pagina)) {
        printf("Tempo t=%d: [PAGE FAULT] Página %d do Processo %d não está na memória física!\n",
               sim->tempo_atual, pagina, pid + 1);
        sim->page_faults++;
        carregar_pagina(sim, pid, pagina);
    }

    registrar_acesso(sim, pid, pagina, 0); 

    int frame = sim->processos[pid].tabela_paginas[pagina].frame;
    int endereco_fisico = frame * sim->tamanho_pagina + deslocamento;

    printf("Tempo t=%d\n", sim->tempo_atual);
    exibir_memoria_fisica(sim);

    printf("Tempo t=%d: Endereço Virtual (P%d): %d -> Página: %d -> Frame: %d -> Endereço Físico: %d\n\n",
           sim->tempo_atual, pid + 1, endereco_virtual, pagina, frame, endereco_fisico);

    sim->tempo_atual++;
    return endereco_fisico;
}

void liberar_simulador(Simulador *sim){
    for (int i = 0; i < sim->num_processos; i++) {
        free(sim->processos[i].tabela_paginas);
    }
    free(sim->processos);
    free(sim->memoria.frames);
    free(sim->memoria.tempo_carga);
    free(sim);
}

int main(){
    Simulador *sim = inicializar_simulador(4096, 16384);
    for(int i = 0; i < 3; i++){
        criar_processo(sim, 16384);
    }
    executar_simulacao(sim, 0);
    liberar_simulador(sim);
    return 0;
}