# 📊 Projeto: Contador Estatístico Escalonável (Approximate Counter)

Este repositório contém a implementação de um **Contador Estatístico Escalonável**, um projeto desenvolvido para a disciplina de Sistemas Operacionais (ProjSO). O objetivo é explorar técnicas de redução de contenção em sistemas multithread, utilizando contadores locais e um contador global com atualização periódica.

---

## 🚀 O que deve ser Implementado

O sistema deve seguir uma arquitetura de contagem aproximada para minimizar o uso de travas (mutexes) e maximizar a escalabilidade em sistemas multicore.

1.  **Contadores Locais**: Cada thread deve manter seu próprio contador de incrementos.
2.  **Contador Global**: Uma variável única que armazena a soma total, protegida por um `pthread_mutex_t`.
3.  **Mecanismo de Threshold (Limite)**:
    *   Definir um valor limite (ex: 1000 ou 10.000).
    *   A thread incrementa apenas o contador local.
    *   Ao atingir o threshold, a thread deve realizar um "flush" para o global.
4.  **Função de Flush**: Responsável por somar o valor local ao global e zerar o local.
5.  **Comparativo de Desempenho**:
    *   Implementar uma versão "tradicional" (mutex em cada incremento).
    *   Implementar a versão "escalonável".
    *   Medir o tempo de execução de ambas.

---

## 📦 O que deve ser Entregue

A entrega deve consistir nos seguintes itens:

*   **Código Fonte (`.c`)**: Código limpo, comentado e funcional utilizando a biblioteca `pthread`.
*   **Makefile**: Para facilitar a compilação do projeto.
*   **Relatório de Resultados**:
    *   Tabela/Gráfico comparando o tempo de execução entre o contador tradicional e o escalonável.
    *   Análise do impacto do valor do **Threshold** no desempenho e na precisão.
    *   Cálculo do **Speedup** obtido.

---

## 🎤 O que deve ser Apresentado

Na apresentação do projeto, os seguintes pontos devem ser abordados:

1.  **Explicação da Arquitetura**: Como os contadores locais reduzem a contenção?
2.  **Demonstração do Código**: Mostrar as partes principais (loop de incremento, flush e sincronização).
3.  **Análise de Dados**: Apresentar os tempos medidos e explicar por que a versão escalonável é mais rápida em sistemas multicore.
4.  **Trade-offs**: Discutir a relação entre Precisão vs. Desempenho.

---

## ✅ Critérios de Avaliação

O projeto será avaliado com base nos seguintes pilares:

*   **Correção**: O contador final (após o flush final de todas as threads) deve refletir o número total correto de incrementos.
*   **Escalabilidade**: O sistema deve apresentar ganho de performance real ao aumentar o número de threads em relação ao contador tradicional.
*   **Clareza do Código**: Organização, nomes de variáveis e comentários.
*   **Robustez**: Tratamento correto de threads e mutexes para evitar *race conditions* ou *deadlocks*.
*   **Relatório**: Qualidade da análise estatística e visualização dos dados.

---

## 🛠️ Como Executar (Exemplo)

```bash
# Compilar o projeto
gcc -O3 main.c -o contador -lpthread

# Executar
./contador <numero_threads> <incrementos_por_thread> <threshold>
```

---
*Este projeto faz parte do Grupo 3 da disciplina de Sistemas Operacionais.*
