---

# 📄 **PROJETO 3 — Contador Estatístico Escalonável (Approximate Counter)**
### Diagramas em ASCII / Texto
### Estilo: Minimalista Moderno

---

## **1. Arquitetura Geral do Approximate Counter**

```
+-----------------------------------------------------------+
|                 Approximate Counter                       |
+-----------------------------------------------------------+
|  Contador Global (G)                                      |
|        ^                                                  |
|        | (merge)                                          |
|  +-----------+   +-----------+   +-----------+            |
|  | Thread 1  |   | Thread 2  |   | Thread 3  |   ...      |
|  | Local C1  |   | Local C2  |   | Local C3  |            |
|  +-----------+   +-----------+   +-----------+            |
|        |             |              |                     |
|        +-------------+--------------+                     |
|                      |                                    |
|                Redução periódica                          |
+-----------------------------------------------------------+
```

---

## **2. Fluxo de Incremento Local**

```
Thread T:
    local_count++
    if local_count >= THRESHOLD:
        atomic_add(global_count, local_count)
        local_count = 0
```

---

## **3. Ciclo de Vida da Redução**

```
+----------------------+
| Incrementos locais   |
+----------+-----------+
           |
           v
+----------------------+
| Threshold atingido   |
+----------+-----------+
           |
           v
+----------------------+
| Merge no global      |
+----------+-----------+
           |
           v
+----------------------+
| Zera contador local  |
+----------------------+
```
