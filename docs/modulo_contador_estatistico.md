---

# 🎓 **CURSO COMPLETO — PROJETO 3**
# **Contador Estatístico Escalonável (Approximate Counter)**

---

# 📘 **Módulo 1 — Introdução ao Problema**

### 🎯 Objetivos de aprendizagem
Ao final deste módulo, o aluno será capaz de:

- Entender por que contadores compartilhados sofrem com contenção.
- Explicar o conceito de **contadores aproximados**.
- Compreender como contadores locais reduzem o uso de mutex.
- Identificar cenários reais onde esse padrão é usado (ex.: kernel Linux).

### 📚 Conteúdo
- O problema do contador global com mutex
- Contenção em sistemas multicore
- Solução: **contadores locais + flush periódico**
- Trade-off: precisão vs desempenho
- Aplicações reais:
  - Estatísticas de rede
  - Contadores de sistema de arquivos
  - Métricas de servidores

### 📝 Atividade rápida
Explique por que incrementar um contador global com mutex pode ser mais lento que processar dados em paralelo.

---

# 📘 **Módulo 2 — Arquitetura do Sistema**

### 🎯 Objetivos
- Visualizar a arquitetura completa do contador escalonável.
- Entender o papel do contador global e dos contadores locais.
- Compreender o mecanismo de flush.

### 📚 Conteúdo
- Componentes principais:
  - Contador global
  - Contadores locais (um por thread)
  - Threshold
  - Mutex global
- Funcionamento:
  - Cada thread incrementa seu contador local
  - Quando atinge o threshold → flush
  - Mutex é usado apenas no flush
- Benefícios:
  - Redução drástica de contenção
  - Escalabilidade linear com número de threads

### 🧠 Conceitos-chave
- Contenção
- Threshold
- Flush
- Escalabilidade
- Aproximação estatística

---

# 📘 **Módulo 3 — Diagramas Explicativos**

Inclui:

- Arquitetura geral
- Fluxo de incremento local
- Fluxo de flush
- Interação entre threads e contador global

*(Os diagramas serão integrados ao PDF/PPT quando você solicitar.)*

---

# 📘 **Módulo 4 — Implementação em C (Explicada Passo a Passo)**

### 🎯 Objetivos
- Entender cada parte do código.
- Saber como implementar contadores locais.
- Saber como sincronizar o flush corretamente.

### 📚 Conteúdo
- Estrutura `local_counter_t`
- Variáveis globais
- Função `flush_local()`
- Worker threads incrementando localmente
- Flush final ao término da thread
- Comparação com contador tradicional

### 🧩 Exercício guiado
Implemente uma versão que:

- Usa threshold de 1000
- Cria 8 threads
- Cada thread faz 10 milhões de incrementos
- Compara o tempo com um contador global tradicional

---

# 📘 **Módulo 5 — Exercícios Práticos**

### 🧪 Exercício 1 — Threshold adaptativo
Implemente um threshold que aumenta quando há muita contenção e diminui quando há pouca.

### 🧪 Exercício 2 — Contador de máximo
Adapte o contador para calcular o máximo observado.

### 🧪 Exercício 3 — Contador por CPU real
Use `sched_getcpu()` para criar contadores por núcleo físico.

### 🧪 Exercício 4 — Estatísticas de servidor
Use o contador para medir requisições por segundo em um servidor multithread.

---

# 📘 **Módulo 6 — Projeto Final**

### 🎓 Desafio
Implemente um **contador escalonável completo** que:

- Possui contadores locais por thread
- Usa threshold configurável
- Faz flush automático
- Mede o tempo de execução
- Compara com contador tradicional
- Gera relatório de speedup

### 🎯 Critérios de avaliação
- Correção
- Escalabilidade
- Clareza do código
- Robustez
- Precisão aceitável

---

# 📘 **Módulo 7 — Perguntas de Revisão**

1. Por que contadores globais causam contenção?
2. Como contadores locais reduzem o uso de mutex?
3. O que é threshold?
4. Por que o contador final é aproximado?
5. Quando ocorre o flush?
6. Como medir o speedup do contador escalonável?
7. Em que situações a precisão exata não é necessária?

---
