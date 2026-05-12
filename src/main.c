/******************************************************************************
 * Projeto: Contador Estatistico Escalonavel (Approximate Counter)
 * Disciplina: Sistemas Operacionais (ProjSO)
 * Grupo: 3
 *
 * Descricao:
 * Este programa demonstra a diferenca de desempenho entre duas abordagens de
 * contagem em um ambiente multithreaded:
 *
 * 1. TRADICIONAL: Cada thread usa um mutex para acessar e incrementar a
 *    variavel global a cada iteracao. Isso causa ALTA CONTENCAO porque
 *    todas as threads ficam "brigando" pelo mesmo recurso (o mutex).
 *
 * 2. ESCALONAVEL (Approximate Counter): Cada thread incrementa uma variavel
 *    LOCAL (sem mutex!). Apenas quando o valor local atinge o "threshold"
 *    (limite), o valor e transferido para o global com mutex. Isso reduz
 *    drasticamente a contencao.
 *
 * O programa executa 3 etapas automaticamente:
 *   (A) Comparacao direta: Tradicional vs Escalonavel
 *   (B) Varredura de Thresholds: mostra como o threshold impacta o tempo
 *   (C) Varredura de Threads: mostra como o numero de threads impacta
 *
 * Uso:
 *   ./contador [threads] [incrementos_por_thread] [threshold]
 *   Exemplo: ./contador 8 10000000 1000
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <string.h>

/* ============================================================================
 * CONFIGURACOES PADRAO
 * Se o usuario nao passar argumentos, estes valores sao usados.
 * ========================================================================= */
#define DEFAULT_THREADS     4
#define DEFAULT_INCREMENTS  10000000
#define DEFAULT_THRESHOLD   1000

/* ============================================================================
 * ESTRUTURAS DE DADOS
 * ========================================================================= */

/*
 * Estrutura para o contador local de cada thread.
 * Cada thread tem seu proprio contador, evitando contencao.
 *
 * CONCEITO IMPORTANTE - False Sharing:
 *   Se dois contadores locais ficarem na mesma "linha de cache" (64 bytes),
 *   o processador invalida o cache da outra CPU toda vez que uma escreve.
 *   O padding evita isso, garantindo que cada contador ocupe uma linha
 *   de cache inteira.
 */
typedef struct {
    long long local;
    char padding[64 - sizeof(long long)]; /* Evita false sharing */
} local_counter_t;

/* Variavel global compartilhada entre todas as threads */
long long global_counter = 0;

/* Mutex (trava) que protege o acesso ao global_counter */
pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

/* Ponteiro para o array de contadores locais (um por thread) */
local_counter_t *counters = NULL;

/* Parametros de configuracao (globais para acesso pelas threads) */
int num_threads;
int increments_per_thread;
int threshold;

/* ============================================================================
 * ABORDAGEM 1: CONTADOR TRADICIONAL
 * ---------------------------------------------------------------------------
 * COMO FUNCIONA:
 *   - Cada thread, a cada incremento, faz:
 *     1. Trava o mutex    (pthread_mutex_lock)
 *     2. Incrementa +1    (global_counter++)
 *     3. Destrava o mutex (pthread_mutex_unlock)
 *
 * PROBLEMA:
 *   - Com 8 threads fazendo 10 milhoes de incrementos cada, sao 80 milhoes
 *     de operacoes de lock/unlock. As threads ficam a maior parte do tempo
 *     ESPERANDO umas pelas outras (contencao alta).
 *
 * ANALOGIA:
 *   Imagine 8 pessoas querendo escrever em um UNICO caderno. Cada uma tem
 *   que pegar a caneta, escrever UM numero, e devolver. A fila e enorme!
 * ========================================================================= */
void *worker_traditional(void *arg) {
    (void)arg; /* Nao usa o argumento */

    for (int i = 0; i < increments_per_thread; i++) {
        pthread_mutex_lock(&global_lock);   /* Pega a "caneta" */
        global_counter++;                    /* Escreve +1 */
        pthread_mutex_unlock(&global_lock); /* Devolve a "caneta" */
    }
    return NULL;
}

/* ============================================================================
 * ABORDAGEM 2: CONTADOR ESCALONAVEL (APPROXIMATE COUNTER)
 * ---------------------------------------------------------------------------
 * COMO FUNCIONA:
 *   - Cada thread tem seu proprio contador local (sem mutex!)
 *   - A thread incrementa o local livremente
 *   - Quando o local atinge o threshold, faz "flush":
 *     1. Trava o mutex
 *     2. Soma o local no global
 *     3. Destrava o mutex
 *     4. Zera o local
 *   - No final, faz um flush dos incrementos restantes
 *
 * VANTAGEM:
 *   - Com threshold=1000, o mutex so e usado a cada 1000 incrementos
 *     (e nao a cada 1). Isso reduz a contencao em ~1000x!
 *
 * ANALOGIA:
 *   Agora cada pessoa tem seu PROPRIO rascunho. Ela anota 1000 numeros
 *   no rascunho, e so entao vai ao caderno principal somar tudo de uma vez.
 *   A fila fica MUITO menor!
 *
 * TRADE-OFF:
 *   - Threshold ALTO  = mais rapido, porem o valor global fica "atrasado"
 *                        durante a execucao (leitura intermediaria imprecisa)
 *   - Threshold BAIXO = mais preciso em leituras intermediarias, mas mais lento
 *   - O valor FINAL e sempre EXATO (flush final garante)
 * ========================================================================= */

/* Funcao de flush: transfere o valor local para o global */
void flush_local(int id) {
    pthread_mutex_lock(&global_lock);
    global_counter += counters[id].local;
    pthread_mutex_unlock(&global_lock);
    counters[id].local = 0;
}

/* Worker da abordagem escalonavel */
void *worker_scalable(void *arg) {
    int id = (int)(long)arg;

    for (int i = 0; i < increments_per_thread; i++) {
        counters[id].local++;  /* Incremento local: NENHUM mutex necessario! */

        if (counters[id].local >= threshold) {
            flush_local(id);   /* Flush quando atinge o limiar */
        }
    }

    /* Flush final: garante que incrementos restantes (< threshold) sejam somados */
    flush_local(id);
    return NULL;
}

/* ============================================================================
 * FUNCAO AUXILIAR: EXECUTAR UM TESTE
 * ---------------------------------------------------------------------------
 * Recebe um ponteiro para a funcao worker (tradicional ou escalonavel),
 * cria as threads, espera todas terminarem, e retorna o tempo decorrido.
 *
 * COMO MEDIMOS O TEMPO:
 *   Usamos clock_gettime com CLOCK_MONOTONIC. Esse relogio nunca "volta"
 *   (diferente do relogio de parede que pode ser ajustado pelo NTP).
 *   E a forma mais precisa de medir intervalos em Linux.
 * ========================================================================= */
double run_test(void *(*worker_fn)(void *), int is_scalable) {
    pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);
    if (!threads) {
        fprintf(stderr, "Erro: falha ao alocar memoria para threads.\n");
        exit(EXIT_FAILURE);
    }

    /* Reinicia o estado global */
    global_counter = 0;
    if (is_scalable && counters) {
        for (int i = 0; i < num_threads; i++) {
            counters[i].local = 0;
        }
    }

    /* Marca o tempo ANTES de criar as threads */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    /* Cria todas as threads */
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, worker_fn, (void *)(long)i);
    }

    /* Espera todas as threads terminarem (join) */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    /* Marca o tempo DEPOIS que todas terminaram */
    clock_gettime(CLOCK_MONOTONIC, &end);

    /* Calcula o tempo decorrido em segundos (com precisao de nanosegundos) */
    double elapsed = (end.tv_sec - start.tv_sec)
                   + (end.tv_nsec - start.tv_nsec) / 1e9;

    free(threads);
    return elapsed;
}

/* ============================================================================
 * FUNCAO AUXILIAR: IMPRIMIR LINHA SEPARADORA
 * ========================================================================= */
void print_separator(void) {
    printf("+-----------+--------------+-----------------+----------+---------+\n");
}

void print_header_threshold(void) {
    printf("\n");
    print_separator();
    printf("| Threshold |  Tempo (s)   |    Contagem     | Erro (%%) | Speedup |\n");
    print_separator();
}

void print_header_threads(void) {
    printf("\n");
    printf("+----------+--------------+--------------+---------+\n");
    printf("| Threads  | Tradic. (s)  | Escal. (s)   | Speedup |\n");
    printf("+----------+--------------+--------------+---------+\n");
}

/* ============================================================================
 * MAIN: PONTO DE ENTRADA DO PROGRAMA
 * ========================================================================= */
int main(int argc, char *argv[]) {

    /* ------------------------------------------------------------------
     * PASSO 1: Ler parametros da linha de comando
     * Se nao fornecidos, usa os valores padrao definidos no topo
     * ---------------------------------------------------------------- */
    num_threads         = (argc > 1) ? atoi(argv[1]) : DEFAULT_THREADS;
    increments_per_thread = (argc > 2) ? atoi(argv[2]) : DEFAULT_INCREMENTS;
    threshold           = (argc > 3) ? atoi(argv[3]) : DEFAULT_THRESHOLD;

    if (num_threads <= 0 || increments_per_thread <= 0 || threshold <= 0) {
        fprintf(stderr, "Erro: todos os parametros devem ser maiores que zero.\n");
        fprintf(stderr, "Uso: %s [threads] [incrementos] [threshold]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    long long expected = (long long)num_threads * increments_per_thread;

    /* ------------------------------------------------------------------
     * CABECALHO
     * ---------------------------------------------------------------- */
    printf("\n");
    printf("=========================================================\n");
    printf("   CONTADOR ESTATISTICO ESCALONAVEL - BENCHMARK COMPLETO\n");
    printf("=========================================================\n");
    printf("  Threads................: %d\n", num_threads);
    printf("  Incrementos por thread.: %d\n", increments_per_thread);
    printf("  Total esperado.........: %lld\n", expected);
    printf("  Threshold padrao.......: %d\n", threshold);
    printf("=========================================================\n");

    /* ------------------------------------------------------------------
     * PASSO 2: Alocar contadores locais
     * ---------------------------------------------------------------- */
    counters = malloc(sizeof(local_counter_t) * num_threads);
    if (!counters) {
        fprintf(stderr, "Erro: falha ao alocar contadores locais.\n");
        return EXIT_FAILURE;
    }

    /* ==================================================================
     * ETAPA A: COMPARACAO DIRETA (Tradicional vs Escalonavel)
     * ------------------------------------------------------------------
     * Roda cada abordagem uma vez e mostra a diferenca.
     * ================================================================ */
    printf("\n");
    printf("---------------------------------------------------------\n");
    printf("  ETAPA A: Comparacao Direta\n");
    printf("---------------------------------------------------------\n");

    /* A.1 - Tradicional */
    printf("\n  [1] Abordagem TRADICIONAL (mutex a cada incremento)...\n");
    double time_trad = run_test(worker_traditional, 0);
    long long count_trad = global_counter;
    printf("      Contagem final : %lld %s\n", count_trad,
           (count_trad == expected) ? "(CORRETO)" : "(ERRO!)");
    printf("      Tempo decorrido: %.6f segundos\n", time_trad);

    /* A.2 - Escalonavel */
    printf("\n  [2] Abordagem ESCALONAVEL (threshold = %d)...\n", threshold);
    double time_scal = run_test(worker_scalable, 1);
    long long count_scal = global_counter;
    printf("      Contagem final : %lld %s\n", count_scal,
           (count_scal == expected) ? "(CORRETO)" : "(ERRO!)");
    printf("      Tempo decorrido: %.6f segundos\n", time_scal);

    /* A.3 - Resultado */
    printf("\n  --- Resultado ---\n");
    if (time_scal > 0) {
        double speedup = time_trad / time_scal;
        printf("  Speedup = T_tradicional / T_escalonavel\n");
        printf("  Speedup = %.6f / %.6f\n", time_trad, time_scal);
        printf("  Speedup = %.2fx\n", speedup);
        printf("\n  >> A versao escalonavel foi %.2fx MAIS RAPIDA!\n", speedup);
    } else {
        printf("  (Tempo muito curto para calcular speedup)\n");
    }

    /* ==================================================================
     * ETAPA B: VARREDURA DE THRESHOLDS
     * ------------------------------------------------------------------
     * Objetivo: Mostrar como o valor do threshold afeta o tempo.
     *
     * CONCEITO:
     *   - Threshold = 1   -> comportamento quase igual ao tradicional
     *                        (flush a cada incremento = mutex a cada vez)
     *   - Threshold = 10   -> mutex a cada 10 incrementos
     *   - Threshold = 1000 -> mutex a cada 1000 incrementos
     *   - Threshold = 100000 -> mutex muito raramente
     *
     *   Quanto MAIOR o threshold, MENOS vezes o mutex e usado,
     *   e portanto MAIS RAPIDO o programa executa.
     * ================================================================ */
    printf("\n");
    printf("---------------------------------------------------------\n");
    printf("  ETAPA B: Impacto do Threshold no Desempenho\n");
    printf("---------------------------------------------------------\n");
    printf("  (Fixo: %d threads, %d incrementos/thread)\n",
           num_threads, increments_per_thread);

    int thresholds[] = {1, 10, 100, 1000, 10000, 100000};
    int num_th = sizeof(thresholds) / sizeof(thresholds[0]);

    print_header_threshold();

    for (int t = 0; t < num_th; t++) {
        threshold = thresholds[t];

        double elapsed = run_test(worker_scalable, 1);
        long long count = global_counter;
        double erro = ((double)(expected - count) / expected) * 100.0;
        double spd = (time_trad > 0 && elapsed > 0) ? time_trad / elapsed : 0;

        printf("| %9d | %12.6f | %15lld | %7.3f%% | %7.2fx |\n",
               threshold, elapsed, count, erro, spd);
    }
    print_separator();

    printf("\n  Observacao: O erro final e sempre 0%% porque o flush final\n");
    printf("  garante que todos os incrementos sejam somados ao global.\n");
    printf("  A 'imprecisao' do approximate counter so afeta leituras\n");
    printf("  intermediarias (durante a execucao), nao o resultado final.\n");

    /* ==================================================================
     * ETAPA C: VARREDURA DE THREADS
     * ------------------------------------------------------------------
     * Objetivo: Mostrar como o numero de threads afeta o desempenho
     * de cada abordagem.
     *
     * CONCEITO:
     *   - Na versao TRADICIONAL, mais threads = MAIS CONTENCAO
     *     (todos disputam o mesmo mutex) -> tempo AUMENTA
     *   - Na versao ESCALONAVEL, mais threads = MAIS PARALELISMO
     *     (cada um trabalha no seu local) -> tempo DIMINUI (ate certo ponto)
     * ================================================================ */
    printf("\n");
    printf("---------------------------------------------------------\n");
    printf("  ETAPA C: Impacto do Numero de Threads\n");
    printf("---------------------------------------------------------\n");
    printf("  (Fixo: %d incrementos/thread, threshold = %d)\n",
           increments_per_thread, DEFAULT_THRESHOLD);

    threshold = DEFAULT_THRESHOLD; /* Restaura threshold padrao */

    int thread_counts[] = {1, 2, 4, 8};
    int num_tc = sizeof(thread_counts) / sizeof(thread_counts[0]);

    print_header_threads();

    for (int t = 0; t < num_tc; t++) {
        num_threads = thread_counts[t];

        /* Realoca contadores para o novo numero de threads */
        free(counters);
        counters = malloc(sizeof(local_counter_t) * num_threads);
        if (!counters) {
            fprintf(stderr, "Erro: falha ao realocar contadores.\n");
            return EXIT_FAILURE;
        }

        double t_trad = run_test(worker_traditional, 0);
        double t_scal = run_test(worker_scalable, 1);
        double spd = (t_scal > 0) ? t_trad / t_scal : 0;

        printf("| %8d | %12.6f | %12.6f | %7.2fx |\n",
               num_threads, t_trad, t_scal, spd);
    }
    free(counters);
    return 0;
}
