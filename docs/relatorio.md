# Relatorio de Resultados - Contador Estatistico Escalonavel

**Disciplina:** Sistemas Operacionais (ProjSO)  
**Grupo:** 3  
**Projeto:** Contador Estatistico Escalonavel (Approximate Counter)

---

## 1. Introducao

### O Problema

Em sistemas operacionais modernos, e muito comum que varias threads precisem **incrementar o mesmo contador** simultaneamente. Exemplos reais incluem:

- Contar pacotes de rede recebidos
- Contar operacoes de I/O (leitura/escrita em disco)
- Estatisticas de uso de CPU

A abordagem mais simples e usar um **mutex** (trava) para proteger o contador. Porem, quando muitas threads disputam o mesmo mutex, elas ficam a maior parte do tempo **esperando** umas pelas outras. Isso se chama **contencao** (contention).

### A Solucao: Approximate Counter (Contador Escalonavel)

A ideia e simples e poderosa:

1. Cada thread tem seu **proprio contador local** (sem mutex!)
2. A thread incrementa o local livremente
3. Quando o local atinge um **limite (threshold)**, ela transfere o valor para o global (com mutex)
4. No final, um **flush final** garante que o resultado e exato

```
  SEM ESCALONAMENTO (Tradicional):        COM ESCALONAMENTO:
  
  Thread 1 --lock--> [GLOBAL] <--lock-- Thread 1     Thread 1: [LOCAL=999] 
  Thread 2 --lock--> [GLOBAL] <--lock-- Thread 2     Thread 2: [LOCAL=742]
  Thread 3 --lock--> [GLOBAL] <--lock-- Thread 3     Thread 3: [LOCAL=500]
  Thread 4 --lock--> [GLOBAL] <--lock-- Thread 4     Thread 4: [LOCAL=888]
                                                            |
  Todas BRIGAM pelo mesmo mutex!         So fazem flush quando local >= threshold
  80 MILHOES de locks!                   Apenas 80 MIL locks! (threshold=1000)
```

---

## 2. Arquitetura do Sistema

### Diagrama de Funcionamento

```
         +----------------------------------+
         |        CONTADOR GLOBAL           |
         |     (protegido por MUTEX)        |
         |          valor: 80000000         |
         +--------^-------^-------^---------+
                  |       |       |
            flush |  flush|  flush|  (so quando local >= threshold)
                  |       |       |
    +-------------+--+ +--+----------+ +--+-------------+
    |   THREAD #1    | |   THREAD #2   | |   THREAD #3    |
    | local = 0..999 | | local = 0..999 | | local = 0..999 |
    | (sem mutex!)   | | (sem mutex!)   | | (sem mutex!)   |
    +----------------+ +----------------+ +----------------+
         |                  |                   |
    incrementa++       incrementa++        incrementa++
    (operacao            (operacao            (operacao
     local, rapida)       local, rapida)       local, rapida)
```

### Estruturas de Dados no Codigo

```c
// Cada thread tem um destes:
typedef struct {
    long long local;        // Contador local (sem mutex)
    char padding[56];       // Evita "false sharing" no cache
} local_counter_t;

// Compartilhado por todas:
long long global_counter;              // Valor total
pthread_mutex_t global_lock;           // Mutex que protege o global
```

**O que e False Sharing?**
Quando dois contadores locais ficam na mesma "linha de cache" (64 bytes do processador), o cache de um nucleo e invalidado toda vez que outro nucleo escreve no seu contador. O `padding` forca cada contador a ocupar uma linha de cache inteira, eliminando esse problema.

---

## 3. Como o Codigo Funciona

### Worker Tradicional (Lento)

```c
void *worker_traditional(void *arg) {
    for (int i = 0; i < increments_per_thread; i++) {
        pthread_mutex_lock(&global_lock);   // 1. Trava
        global_counter++;                    // 2. Incrementa
        pthread_mutex_unlock(&global_lock); // 3. Destrava
    }
    return NULL;
}
```

**Por que e lento?** Com 4 threads e 10M de incrementos cada, sao **40 milhoes** de operacoes de lock/unlock. As threads ficam "na fila" esperando.

### Worker Escalonavel (Rapido)

```c
void *worker_scalable(void *arg) {
    int id = (int)(long)arg;

    for (int i = 0; i < increments_per_thread; i++) {
        counters[id].local++;              // Incremento LOCAL (sem mutex!)

        if (counters[id].local >= threshold) {
            flush_local(id);               // Flush quando atinge o limite
        }
    }

    flush_local(id);  // Flush final: garante resultado exato
    return NULL;
}
```

**Por que e rapido?** Com threshold=1000, o mutex so e usado a cada 1000 incrementos. Com 4 threads e 10M cada, sao apenas **40 mil** locks (vs 40 milhoes!).

### Funcao de Flush

```c
void flush_local(int id) {
    pthread_mutex_lock(&global_lock);       // Trava
    global_counter += counters[id].local;   // Soma tudo de uma vez
    pthread_mutex_unlock(&global_lock);     // Destrava
    counters[id].local = 0;                 // Zera o local
}
```

---

## 4. Resultados Experimentais

### 4.1 Comparacao Direta: Tradicional vs Escalonavel

**Configuracao:** 4 threads, 5.000.000 incrementos/thread, threshold = 1000

| Abordagem    | Contagem Final | Tempo (s) | Correto? |
|--------------|----------------|-----------|----------|
| Tradicional  | 20.000.000     | 0.753     | Sim      |
| Escalonavel  | 20.000.000     | 0.003     | Sim      |

**Speedup = 0.753 / 0.003 = ~222x**

> A versao escalonavel foi mais de **200 vezes mais rapida** que a tradicional!

### 4.2 Impacto do Threshold no Desempenho

**Configuracao:** 4 threads, 5.000.000 incrementos/thread

| Threshold | Tempo (s) | Contagem    | Erro (%) | Speedup |
|-----------|-----------|-------------|----------|---------|
| 1         | 0.798     | 20.000.000  | 0.000%   | 0.94x   |
| 10        | 0.082     | 20.000.000  | 0.000%   | 9.19x   |
| 100       | 0.025     | 20.000.000  | 0.000%   | 29.65x  |
| 1.000     | 0.003     | 20.000.000  | 0.000%   | 236.96x |
| 10.000    | 0.002     | 20.000.000  | 0.000%   | 353.51x |
| 100.000   | 0.002     | 20.000.000  | 0.000%   | 347.82x |

**Analise:**

```
Tempo (s)
  |
  0.8 |  *  (threshold=1, quase igual ao tradicional)
  |   |
  0.6 |
  |   |
  0.4 |
  |   |
  0.2 |
  |   |
  0.0 |-----*--------*--------*--------*--------*-----> Threshold
       1    10      100     1000    10000   100000
             \       \       \       \       \
              9x     30x    237x    354x    348x   (Speedup)
```

**Observacoes importantes:**

1. **Threshold = 1**: Comportamento identico ao tradicional (flush a cada incremento = mutex a cada vez)
2. **Threshold = 10 a 1000**: Ganho exponencial de desempenho
3. **Threshold = 10000+**: Ganho satura (o custo ja nao e mais dominado pelo mutex)
4. **Erro sempre 0%**: O flush final garante correcao total

### 4.3 Impacto do Numero de Threads

**Configuracao:** 5.000.000 incrementos/thread, threshold = 1000

| Threads | Tradicional (s) | Escalonavel (s) | Speedup |
|---------|-----------------|-----------------|---------|
| 1       | 0.049           | 0.003           | 19.17x  |
| 2       | 0.418           | 0.003           | 147.61x |
| 4       | 0.768           | 0.004           | 211.49x |
| 8       | 1.694           | 0.009           | 178.93x |

**Analise:**

```
Tempo (s)               Tradicional: sobe (mais contencao)
  |                     /
  1.6 |               *
  |   |              /
  1.2 |             /
  |   |            /
  0.8 |       ----*
  |   |      /
  0.4 |  ---*            Escalonavel: quase constante!
  |   | /                ____________________________________
  0.0 |*--------------------------------------------------> Threads
      1       2       4       8
```

**Observacoes importantes:**

1. **Tradicional**: O tempo AUMENTA com mais threads (de 0.05s com 1 thread para 1.7s com 8 threads). Mais threads = mais contencao = mais lento!
2. **Escalonavel**: O tempo permanece quase constante (~0.003-0.009s), independente do numero de threads.
3. **Com 1 thread**: Speedup ja e 19x porque mesmo com 1 thread, a versao escalonavel evita o custo do lock/unlock a cada incremento.

---

## 5. Calculo do Speedup

O **Speedup** mede quantas vezes a versao escalonavel e mais rapida que a tradicional.

### Formula

```
                    Tempo da versao TRADICIONAL
    Speedup (S) = --------------------------------
                    Tempo da versao ESCALONAVEL
```

### Exemplo com nossos dados

```
    S = T_tradicional / T_escalonavel
    S = 0.753175 / 0.003389
    S = 222.26x
```

**Interpretacao**: A versao escalonavel completou a mesma tarefa **222 vezes mais rapido** que a tradicional.

### Por que o Speedup e tao alto?

| Fator                        | Tradicional    | Escalonavel (threshold=1000) |
|------------------------------|----------------|------------------------------|
| Operacoes de lock/unlock     | 20.000.000     | 20.000 (1000x menos!)        |
| Contencao no mutex           | Extrema        | Minima                       |
| Trabalho util por lock       | 1 incremento   | 1000 incrementos             |

---

## 6. Trade-off: Precisao vs Desempenho

### O que significa "Approximate Counter"?

O nome "approximate" (aproximado) se refere ao fato de que, **durante a execucao**, o valor do `global_counter` pode estar **atrasado** em relacao ao total real de incrementos ja feitos.

### Exemplo pratico

```
Situacao: 4 threads, threshold = 1000, cada uma ja fez 500 incrementos

  Thread 1: local = 500  (ainda nao fez flush)
  Thread 2: local = 500  (ainda nao fez flush)
  Thread 3: local = 500  (ainda nao fez flush)
  Thread 4: local = 500  (ainda nao fez flush)

  global_counter = 0     (ninguem atingiu o threshold ainda!)

  Valor REAL  = 500 + 500 + 500 + 500 = 2000
  Valor LIDO  = 0 (global_counter)
  DEFASAGEM   = 2000 incrementos "escondidos" nos locais
```

### Quando isso importa?

- **NAO importa** se voce so precisa do **resultado final** (flush final garante correcao)
- **Importa** se voce precisa ler o contador **durante** a execucao (ex: monitoramento em tempo real)

### O threshold controla esse trade-off

| Threshold | Precisao intermediaria | Desempenho | Defasagem maxima        |
|-----------|------------------------|------------|-------------------------|
| 1         | Perfeita               | Pessimo    | 0                       |
| 10        | Muito boa              | Bom        | num_threads * 9         |
| 100       | Boa                    | Muito bom  | num_threads * 99        |
| 1.000     | Moderada               | Excelente  | num_threads * 999       |
| 100.000   | Baixa                  | Maximo     | num_threads * 99.999    |

---

## 7. Conceitos-Chave para a Apresentacao

### 7.1 O que e um Mutex?

Um **mutex** (mutual exclusion) e uma "trava" que garante que apenas **uma thread por vez** possa executar uma secao critica. E como uma chave de banheiro: quem pega a chave entra, os outros esperam.

```c
pthread_mutex_lock(&lock);    // Pega a chave (ou espera)
// ... secao critica ...
pthread_mutex_unlock(&lock);  // Devolve a chave
```

### 7.2 O que e Contencao?

**Contencao** e quando varias threads competem pelo mesmo recurso (mutex). Quanto mais threads e mais frequente o acesso, maior a contencao e mais tempo as threads ficam paradas esperando.

### 7.3 O que e Race Condition?

Uma **race condition** ocorre quando o resultado depende da ordem de execucao das threads. Sem mutex, `global_counter++` por varias threads pode resultar em valores errados:

```
  Thread A le global_counter = 5
  Thread B le global_counter = 5
  Thread A escreve 6
  Thread B escreve 6   // DEVERIA ser 7! Perdeu um incremento!
```

### 7.4 O que e False Sharing?

Quando contadores de threads diferentes ficam na mesma **linha de cache** (64 bytes), qualquer escrita por uma thread invalida o cache das outras, mesmo sem compartilhar dados logicamente. O **padding** resolve isso.

### 7.5 Como medir tempo com precisao?

```c
struct timespec start, end;
clock_gettime(CLOCK_MONOTONIC, &start);  // Relogio que nunca volta
// ... codigo a medir ...
clock_gettime(CLOCK_MONOTONIC, &end);
double elapsed = (end.tv_sec - start.tv_sec) 
               + (end.tv_nsec - start.tv_nsec) / 1e9;
```

`CLOCK_MONOTONIC` e um relogio que nunca e ajustado pelo sistema. Diferente do "relogio de parede" que pode ser corrigido pelo NTP, este so avanca para frente - ideal para medir intervalos.

---

## 8. Aplicacoes no Mundo Real

Esta tecnica (per-CPU counters / approximate counters) e usada em:

1. **Kernel Linux**: Contadores de estatisticas de rede, disco e CPU usam per-CPU counters para evitar contencao em maquinas com muitos nucleos.
2. **Bancos de dados**: Contadores de metricas internas (queries executadas, cache hits, etc.)
3. **Servidores web**: Contagem de requisicoes por segundo sem gargalos de sincronizacao.
4. **Sistemas embarcados**: Onde o custo de um mutex pode ser proibitivo.

---

## 9. Conclusao

O Contador Estatistico Escalonavel demonstra que **reduzir a frequencia de acesso a recursos compartilhados** e uma das tecnicas mais eficazes para melhorar o desempenho em sistemas multithread.

| Metrica                | Resultado                     |
|------------------------|-------------------------------|
| Speedup maximo obtido  | ~350x (threshold=10000)       |
| Correcao do resultado  | 100% (flush final)            |
| Escalabilidade         | Excelente (tempo quase constante) |

A principal licao: **nao e necessario ter precisao perfeita a todo momento para ter um resultado final correto**. Esse principio (eventual consistency) e fundamental em sistemas distribuidos e operacionais modernos.

---

*Dados coletados em ambiente WSL2/Ubuntu com processador de 4+ nucleos.*  
*Compilado com GCC e flag -O3 (otimizacao maxima).*
