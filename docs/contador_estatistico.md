---

# ✅ **PROJETO 3 — Contador Estatístico Escalonável (Aproximado)**

---

## 🎯 **Objetivo**
Criar um sistema de contagem **altamente escalonável**, inspirado em técnicas usadas em kernels modernos (como o Linux), onde:

- Cada thread mantém um **contador local** (por CPU/thread).
- Existe um **contador global**, mas ele **não é atualizado a cada incremento**.
- Threads só transferem seus valores locais para o global quando atingem um **limite (threshold)**.
- Isso reduz contenção em mutexes e melhora desempenho em sistemas com muitos núcleos.

Esse padrão é conhecido como:

- **Per-CPU counters**
- **Approximate counters**
- **Scalable counters**

---

# 🧱 **Arquitetura do Sistema**

### **Componentes**
1. **Contadores locais (um por thread)**
   - Armazenam incrementos temporários
   - Não precisam de mutex
   - São rápidos e escaláveis

2. **Contador global**
   - Atualizado apenas quando necessário
   - Protegido por mutex

3. **Threshold**
   - Quando o contador local atinge esse valor, ele é “flushado” para o global

4. **Threads trabalhadoras**
   - Executam incrementos massivos
   - Cada thread incrementa seu contador local
   - Periodicamente atualizam o global

---

# 📊 **Diagrama de Arquitetura (ASCII)**

```
          +---------------------------+
          |       Contador Global     |
          |   (mutex + valor total)   |
          +-------------+-------------+
                        ^
                        | flush
                        |
        ------------------------------------------------
        |                      |                       |
        v                      v                       v
+---------------+     +---------------+       +---------------+
| Thread #1     |     | Thread #2     |       | Thread #3     |
| local_count=7 |     | local_count=9 |       | local_count=3 |
| threshold=10  |     | threshold=10  |       | threshold=10  |
+-------+-------+     +-------+-------+       +-------+-------+
        |                     |                       |
        | increment           | increment             | increment
        v                     v                       v
local_count++         local_count++           local_count++
```

Quando `local_count >= threshold`, ocorre:

```
local_count -> global_count
local_count = 0
```

---

# 🧩 **Fluxo de Execução**

1. Inicializar contador global = 0
2. Criar N threads
3. Cada thread:
   - Incrementa seu contador local
   - Se atingir threshold → flush para global
4. Ao final, somar todos os contadores locais restantes
5. Exibir valor final

---

# 🧪 **Código Inicial em C (Skeleton)**
*(POSIX threads, contadores locais + global)*

```c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define THREADS 8
#define INCREMENTS 10000000
#define THRESHOLD 1000

typedef struct {
    long long local;
} local_counter_t;

long long global_counter = 0;
pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

local_counter_t counters[THREADS];

void flush_local(int id) {
    pthread_mutex_lock(&global_lock);
    global_counter += counters[id].local;
    pthread_mutex_unlock(&global_lock);

    counters[id].local = 0;
}

void *worker(void *arg) {
    int id = (int)(long)arg;

    for (int i = 0; i < INCREMENTS; i++) {
        counters[id].local++;

        if (counters[id].local >= THRESHOLD)
            flush_local(id);
    }

    // flush final
    flush_local(id);
    return NULL;
}

int main() {
    pthread_t threads[THREADS];

    for (int i = 0; i < THREADS; i++)
        counters[i].local = 0;

    for (int i = 0; i < THREADS; i++)
        pthread_create(&threads[i], NULL, worker, (void *)(long)i);

    for (int i = 0; i < THREADS; i++)
        pthread_join(threads[i], NULL);

    printf("Contagem final aproximada: %lld\n", global_counter);
    return 0;
}
```

---

# 🛠️ **Roteiro Completo de Implementação**

### **1. Criar estrutura de contador local**
- Um array com um contador por thread
- Não precisa de mutex

### **2. Criar contador global**
- Protegido por mutex
- Atualizado apenas ocasionalmente

### **3. Definir threshold**
- Valor típico: 1000, 10.000, 50.000
- Quanto maior o threshold:
  - Menos precisão
  - Mais desempenho

### **4. Implementar função `flush_local()`**
- Trava mutex
- Soma local → global
- Zera local

### **5. Worker threads**
- Loop de incrementos
- Incrementa local
- Se atingir threshold → flush

### **6. Flush final**
- Após o loop, cada thread envia o restante

### **7. Medir desempenho**
Comparar:

- Contador com mutex em cada incremento
- Contador escalonável com threshold

A diferença pode ser enorme em máquinas com muitos núcleos.

### **8. Melhorias opcionais**
- Afinidade de CPU (per-CPU real)
- Threshold adaptativo
- Contadores lock-free
- Uso de `__thread` ou `thread_local`

---
