# Relatório de Resultados — Contador Estatístico Escalonável

**Disciplina:** Sistemas Operacionais (ProjSO)  
**Grupo:** 3  
**Projeto:** Contador Estatístico Escalonável (Approximate Counter)

---

## 1. Introdução

### O Problema

Em sistemas operacionais modernos, é muito comum que várias threads precisem **incrementar o mesmo contador** simultaneamente. Exemplos reais incluem:

- Contar pacotes de rede recebidos
- Contar operações de I/O (leitura/escrita em disco)
- Estatísticas de uso de CPU

A abordagem mais simples é usar um **mutex** (trava de exclusão mútua) para proteger o contador. Porém, quando muitas threads disputam o mesmo mutex, elas ficam a maior parte do tempo **esperando** umas pelas outras. Esse fenômeno se chama **contenção** (*contention*).

### A Solução: Approximate Counter (Contador Escalonável)

A ideia é simples e poderosa:

1. Cada thread tem seu **próprio contador local** (sem mutex!)
2. A thread incrementa o local livremente
3. Quando o local atinge um **limite (threshold)**, ela transfere o valor para o global (com mutex)
4. No final, um **flush final** garante que o resultado é exato

```
  SEM ESCALONAMENTO (Tradicional):        COM ESCALONAMENTO:
  
  Thread 1 --lock--> [GLOBAL] <--lock-- Thread 1     Thread 1: [LOCAL=999] 
  Thread 2 --lock--> [GLOBAL] <--lock-- Thread 2     Thread 2: [LOCAL=742]
  Thread 3 --lock--> [GLOBAL] <--lock-- Thread 3     Thread 3: [LOCAL=500]
  Thread 4 --lock--> [GLOBAL] <--lock-- Thread 4     Thread 4: [LOCAL=888]
                                                            |
  Todas BRIGAM pelo mesmo mutex!         Só fazem flush quando local >= threshold
  80 MILHÕES de locks!                   Apenas 80 MIL locks! (threshold=1000)
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
            flush |  flush|  flush|  (só quando local >= threshold)
                  |       |       |
    +-------------+--+ +--+----------+ +--+-------------+
    |   THREAD #1    | |   THREAD #2   | |   THREAD #3    |
    | local = 0..999 | | local = 0..999 | | local = 0..999 |
    | (sem mutex!)   | | (sem mutex!)   | | (sem mutex!)   |
    +----------------+ +----------------+ +----------------+
         |                  |                   |
    incrementa++       incrementa++        incrementa++
    (operação            (operação            (operação
     local, rápida)       local, rápida)       local, rápida)
```

### Estruturas de Dados no Código

```c
// Cada thread tem um destes:
typedef struct {
    long long local;        // Contador local (sem mutex)
    char padding[56];       // Evita "false sharing" no cache
} local_counter_t;

// Compartilhado por todas as threads:
long long global_counter;              // Valor total acumulado
pthread_mutex_t global_lock;           // Mutex que protege o global
```

**O que é False Sharing?**

Processadores modernos não acessam a memória byte a byte — eles carregam blocos de **64 bytes** chamados **linhas de cache**. Se dois contadores locais de threads diferentes ficarem na mesma linha de cache, o processador invalida o cache de um núcleo toda vez que outro núcleo escreve no seu contador, mesmo que sejam variáveis independentes. O `padding` força cada contador a ocupar uma linha de cache inteira, eliminando esse problema.

---

## 3. Como o Código Funciona

### Worker Tradicional (Lento)

```c
void *worker_traditional(void *arg) {
    for (int i = 0; i < increments_per_thread; i++) {
        pthread_mutex_lock(&global_lock);   // 1. Trava o mutex
        global_counter++;                    // 2. Incrementa
        pthread_mutex_unlock(&global_lock); // 3. Destrava o mutex
    }
    return NULL;
}
```

**Por que é lento?** Com 4 threads e 10 milhões de incrementos cada, são **40 milhões** de operações de lock/unlock. As threads ficam "na fila" esperando a vez de acessar o contador — é como 4 pessoas tentando escrever no mesmo caderno com uma única caneta.

### Worker Escalonável (Rápido)

```c
void *worker_scalable(void *arg) {
    int id = (int)(long)arg;

    for (int i = 0; i < increments_per_thread; i++) {
        counters[id].local++;              // Incremento LOCAL (sem mutex!)

        if (counters[id].local >= threshold) {
            flush_local(id);               // Flush quando atinge o limiar
        }
    }

    flush_local(id);  // Flush final: garante resultado exato
    return NULL;
}
```

**Por que é rápido?** Com threshold=1000, o mutex só é usado a cada 1000 incrementos. Com 4 threads e 10M cada, são apenas **40 mil** locks (vs 40 milhões!). É como cada pessoa anotar 1000 números no seu próprio rascunho, e só depois ir ao caderno principal somar tudo de uma vez.

### Função de Flush

```c
void flush_local(int id) {
    pthread_mutex_lock(&global_lock);       // Trava o mutex
    global_counter += counters[id].local;   // Soma tudo de uma vez
    pthread_mutex_unlock(&global_lock);     // Destrava o mutex
    counters[id].local = 0;                 // Zera o contador local
}
```

O flush é a operação-chave: ela transfere o valor acumulado do contador local para o global. Note que o mutex só é usado aqui — e como isso acontece com pouca frequência, a contenção é mínima.

---

## 4. Resultados Experimentais

### 4.1 Comparação Direta: Tradicional vs Escalonável

**Configuração:** 4 threads, 5.000.000 incrementos/thread, threshold = 1000

| Abordagem    | Contagem Final | Tempo (s) | Correto? |
|--------------|----------------|-----------|----------|
| Tradicional  | 20.000.000     | 0,753     | ✅ Sim   |
| Escalonável  | 20.000.000     | 0,003     | ✅ Sim   |

**Speedup = 0,753 / 0,003 ≈ 222x**

> A versão escalonável foi mais de **200 vezes mais rápida** que a tradicional, mantendo o resultado final **100% correto**.

### 4.2 Impacto do Threshold no Desempenho

**Configuração:** 4 threads, 5.000.000 incrementos/thread

| Threshold | Tempo (s) | Contagem    | Erro (%) | Speedup vs Tradicional |
|-----------|-----------|-------------|----------|------------------------|
| 1         | 0,798     | 20.000.000  | 0,000%   | 0,94x                  |
| 10        | 0,082     | 20.000.000  | 0,000%   | 9,19x                  |
| 100       | 0,025     | 20.000.000  | 0,000%   | 29,65x                 |
| 1.000     | 0,003     | 20.000.000  | 0,000%   | 236,96x                |
| 10.000    | 0,002     | 20.000.000  | 0,000%   | 353,51x                |
| 100.000   | 0,002     | 20.000.000  | 0,000%   | 347,82x                |

**Análise Gráfica:**

```
Tempo (s)
  │
  0.8 │  ●  ← threshold=1 (quase igual ao tradicional!)
  │   │
  0.6 │
  │   │
  0.4 │
  │   │
  0.2 │
  │   │    ● ← threshold=10
  0.0 │─────●────────●────────●────────●──────────→ Threshold
       1    10      100     1000    10000   100000
             │       │       │       │       │
            9x     30x    237x    354x    348x   (Speedup)
```

**Observações importantes:**

1. **Threshold = 1**: Comportamento idêntico ao tradicional (flush a cada incremento = mutex a cada vez). Speedup < 1x porque tem o overhead extra da estrutura de contadores locais.
2. **Threshold = 10 a 1000**: Ganho exponencial de desempenho — cada aumento de 10x no threshold reduz drasticamente a contenção.
3. **Threshold ≥ 10000**: O ganho **satura**. O custo já não é dominado pelo mutex; outros fatores (acesso à memória, overhead do loop) passam a ser o gargalo.
4. **Erro sempre 0%**: O flush final garante correção total do resultado.

### 4.3 Impacto do Número de Threads

**Configuração:** 5.000.000 incrementos/thread, threshold = 1000

| Threads | Tradicional (s) | Escalonável (s) | Speedup |
|---------|-----------------|-----------------|---------|
| 1       | 0,049           | 0,003           | 19,17x  |
| 2       | 0,418           | 0,003           | 147,61x |
| 4       | 0,768           | 0,004           | 211,49x |
| 8       | 1,694           | 0,009           | 178,93x |

**Análise Gráfica:**

```
Tempo (s)               Tradicional: SOBE (mais contenção)
  │                     ╱
  1.6 │               ●
  │   │              ╱
  1.2 │             ╱
  │   │            ╱
  0.8 │       ────●
  │   │      ╱
  0.4 │  ───●            Escalonável: quase CONSTANTE!
  │   │ ╱                ─── ── ── ── ── ── ── ── ── ──
  0.0 │●──────────────────────────────────────────────→ Threads
      1       2       4       8
```

**Observações importantes:**

1. **Tradicional**: O tempo **AUMENTA** com mais threads (de 0,05s com 1 thread para 1,7s com 8). Mais threads = mais contenção no mutex = mais lento! Isso é contraintuitivo — mais paralelismo deveria ser mais rápido, mas a contenção anula o ganho.
2. **Escalonável**: O tempo permanece quase constante (~0,003–0,009s), independente do número de threads. A contenção é tão baixa que adicionar threads quase não impacta.
3. **Com 1 thread**: O speedup já é 19x porque mesmo com 1 thread, a versão escalonável evita o custo do lock/unlock a cada incremento (o mutex é caro mesmo sem contenção).

---

## 5. Cálculo do Speedup

O **Speedup** mede quantas vezes a versão escalonável é mais rápida que a tradicional.

### Fórmula

```
                    Tempo da versão TRADICIONAL
    Speedup (S) = ─────────────────────────────────
                    Tempo da versão ESCALONÁVEL
```

### Exemplo com os dados obtidos

```
    S = T_tradicional / T_escalonável
    S = 0,753175 / 0,003389
    S ≈ 222,26x
```

**Interpretação**: A versão escalonável completou a mesma tarefa **222 vezes mais rápido** que a tradicional, com o mesmo resultado final correto.

### Por que o Speedup é tão alto?

| Fator                           | Tradicional    | Escalonável (threshold=1000) |
|---------------------------------|----------------|------------------------------|
| Operações de lock/unlock        | 20.000.000     | 20.000 (**1000x menos!**)    |
| Contenção no mutex              | Extrema        | Mínima                       |
| Trabalho útil por lock          | 1 incremento   | 1000 incrementos             |
| Tempo gasto esperando o mutex   | ~99% do tempo  | ~0,1% do tempo               |

---

## 6. Trade-off: Precisão vs. Desempenho

### O que significa "Approximate Counter"?

O nome "approximate" (aproximado) refere-se ao fato de que, **durante a execução**, o valor do `global_counter` pode estar **defasado** em relação ao total real de incrementos já realizados. Os incrementos "escondidos" nos contadores locais ainda não foram transferidos.

### Exemplo prático

```
Situação: 4 threads, threshold = 1000, cada uma já fez 500 incrementos

  Thread 1: local = 500  (ainda não fez flush)
  Thread 2: local = 500  (ainda não fez flush)
  Thread 3: local = 500  (ainda não fez flush)
  Thread 4: local = 500  (ainda não fez flush)

  global_counter = 0     (ninguém atingiu o threshold ainda!)

  Valor REAL  = 500 + 500 + 500 + 500 = 2000 incrementos feitos
  Valor LIDO  = 0 (global_counter)
  DEFASAGEM   = 2000 incrementos "escondidos" nos locais
```

### Quando isso importa?

- **NÃO importa** se você só precisa do **resultado final** → o flush final garante correção total
- **Importa** se você precisa ler o contador **durante** a execução (ex: monitoramento em tempo real, barras de progresso)

### O threshold controla esse trade-off

| Threshold | Precisão intermediária | Desempenho | Defasagem máxima            |
|-----------|------------------------|------------|-----------------------------|
| 1         | Perfeita (sempre exato)| Péssimo    | 0                           |
| 10        | Muito boa              | Bom        | num_threads × 9             |
| 100       | Boa                    | Muito bom  | num_threads × 99            |
| 1.000     | Moderada               | Excelente  | num_threads × 999           |
| 100.000   | Baixa                  | Máximo     | num_threads × 99.999        |

> **Dica para a apresentação:** A defasagem máxima é `num_threads × (threshold - 1)`. Com 8 threads e threshold=1000, o valor global pode estar até 7.992 incrementos "atrasado" durante a execução — mas o resultado **final** é sempre exato.

---

## 7. Conceitos-Chave para a Apresentação

### 7.1 O que é um Mutex?

Um **mutex** (*mutual exclusion*) é um mecanismo de sincronização que garante que apenas **uma thread por vez** possa executar uma seção crítica do código. Funciona como a chave de um banheiro: quem pega a chave entra, os outros esperam na fila.

```c
pthread_mutex_lock(&lock);    // Pega a chave (ou espera na fila)
// ... seção crítica ...       // Só uma thread por vez aqui dentro
pthread_mutex_unlock(&lock);  // Devolve a chave
```

### 7.2 O que é Contenção?

**Contenção** (*contention*) ocorre quando múltiplas threads competem pelo mesmo recurso (neste caso, o mutex). Quanto mais threads e mais frequente o acesso ao mutex, maior a contenção e mais tempo as threads ficam paradas esperando.

**Analogia:** Imagine 8 caixas de supermercado (threads) que precisam carimbar cada produto em um único carimbo (mutex). Se cada produto precisa de um carimbo, a fila é enorme. Se cada caixa acumula 1000 produtos e carimba tudo de uma vez, a fila praticamente desaparece.

### 7.3 O que é Race Condition?

Uma **race condition** (condição de corrida) ocorre quando o resultado de um programa depende da **ordem de execução** das threads, que é imprevisível. Sem mutex, operações como `global_counter++` por várias threads podem resultar em valores incorretos:

```
  Thread A lê global_counter = 5
  Thread B lê global_counter = 5    ← leu o valor ANTES de A escrever!
  Thread A escreve 6
  Thread B escreve 6                ← DEVERIA ser 7! Perdeu um incremento!
```

O mutex previne isso garantindo que apenas uma thread leia e escreva por vez.

### 7.4 O que é False Sharing?

**False sharing** é um problema de desempenho causado pela arquitetura de cache dos processadores. O cache funciona em blocos de 64 bytes (linhas de cache). Se variáveis de threads diferentes ficam na mesma linha de cache:

1. Thread A escreve na sua variável → invalida a linha de cache inteira
2. Thread B precisa recarregar a linha de cache do zero, mesmo que sua variável não tenha sido alterada
3. Isso cria um "ping-pong" de cache entre os núcleos, degradando o desempenho

O **padding** (preenchimento com bytes extras) resolve isso, garantindo que cada variável fique sozinha na sua própria linha de cache.

### 7.5 Como medir tempo com precisão?

```c
struct timespec start, end;
clock_gettime(CLOCK_MONOTONIC, &start);  // Relógio que nunca volta
// ... código a medir ...
clock_gettime(CLOCK_MONOTONIC, &end);
double elapsed = (end.tv_sec - start.tv_sec) 
               + (end.tv_nsec - start.tv_nsec) / 1e9;
```

- **`CLOCK_MONOTONIC`**: Relógio que **nunca** é ajustado pelo sistema (diferente do relógio de parede, que pode ser corrigido pelo NTP). Só avança para frente — ideal para medir intervalos de tempo.
- **Precisão**: Nanosegundos (1 ns = 0,000000001 segundos).

---

## 8. Aplicações no Mundo Real

Esta técnica (per-CPU counters / approximate counters) é amplamente utilizada na indústria:

| Aplicação                | Como usa                                           |
|--------------------------|----------------------------------------------------|
| **Kernel Linux**         | Contadores de estatísticas de rede, disco e CPU usam per-CPU counters para evitar contenção em máquinas com dezenas/centenas de núcleos |
| **Bancos de dados**      | Contadores de métricas internas (queries executadas, cache hits, conexões ativas) |
| **Servidores web (Nginx, Apache)** | Contagem de requisições por segundo sem gargalos de sincronização |
| **Sistemas embarcados**  | Onde o custo de um mutex pode ser proibitivo em termos de energia e latência |
| **Jogos e motores gráficos** | Contadores de frames, partículas e entidades sendo processadas em paralelo |


## 9. Conclusão

O Contador Estatístico Escalonável demonstra que **reduzir a frequência de acesso a recursos compartilhados** é uma das técnicas mais eficazes para melhorar o desempenho em sistemas multithread.

| Métrica                      | Resultado                              |
|------------------------------|----------------------------------------|
| Speedup máximo obtido        | ~354x (threshold=10.000)               |
| Correção do resultado final  | 100% em todos os testes (flush final)  |
| Escalabilidade               | Excelente (tempo quase constante com mais threads) |

### Lições aprendidas

1. **Menos locks = mais desempenho.** Reduzir a frequência de acesso ao mutex de 40 milhões para 40 mil vezes resultou em um speedup de 222x.

2. **O resultado final pode ser exato mesmo com leituras intermediárias imprecisas.** O flush final garante que nenhum incremento se perde.

3. **Mais threads nem sempre é melhor.** Na abordagem tradicional, adicionar threads **piora** o desempenho por causa da contenção. A arquitetura do programa importa mais que a quantidade de threads.

4. **Existe um ponto ótimo para o threshold.** Aumentar de 1 para 1000 dá ganhos enormes. Depois de 10.000, o ganho satura — não adianta aumentar indefinidamente.

5. **Esta técnica é usada no mundo real.** O kernel Linux, bancos de dados e servidores web usam variações desta abordagem diariamente.

---

*Dados coletados em ambiente WSL2/Ubuntu com processador de 4+ núcleos.*  
*Compilado com GCC e flag `-O3` (otimização máxima).*
