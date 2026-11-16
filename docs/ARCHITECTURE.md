# Arquitetura e decisões

## Fronteiras do sistema

O simulador, o gateway e o coletor são processos independentes. O gateway usa Modbus TCP na rede de campo e BMS1 na rede de aplicação. A interface é servida pelo mesmo processo que detém o histórico; não há proxy nem serviço frontend adicional. Campo e aplicação usam redes internas; coletor, Prometheus e Grafana também participam de uma rede bridge de supervisão para publicação HTTP no loopback.

| Recurso | Limite |
|---|---|
| Ring C++ de observações | 1–65.536 entradas, configurável |
| Janela C++ de latência | 2.048 entradas |
| Histórico Rust | 1–65.536 entradas globais |
| Dispositivos conhecidos | 128 IDs |
| Workers de ingestão | 64 |
| Workers HTTP | 32 |
| Workers do simulador | 64 |
| Headers HTTP | leitura interrompida ao ultrapassar 8.192 bytes; chunk de até 512 bytes |
| Prazo de leitura HTTP | 3 s absolutos desde o início |
| Timeout de escrita HTTP | 3 s por operação |
| Timeout ocioso de ingestão | 3 s por operação de leitura |

Os limites de worker se aplicam separadamente à ingestão e à API. Ao saturar uma categoria, novas conexões são encerradas. Um permit acompanha a thread e é devolvido quando o worker termina, inclusive durante unwind.

O limite do histórico é global, não por dispositivo. O mapa dos últimos valores preserva uma entrada por ID, mesmo depois que suas amostras saem do ring. Um dispositivo desatualizado continua conhecido e ocupa um dos 128 IDs até reiniciar o coletor.

## Ciclo de aquisição

```text
conectar / reutilizar socket
  → FC03: unidade 1, início 0, quantidade 6
  → read_exact(MBAP) → validar tamanho → read_exact(PDU)
  → conferir envelope → converter e validar grandezas
  → histerese CO₂ → Sample → ring local
  → BMS1 → coletor (best effort)
  → JSONL → espera configurada
```

Em uma falha Modbus, nenhuma amostra é produzida. O socket é fechado e uma próxima tentativa usa backoff. A espera é posterior à tentativa; o polling não é um scan periódico de prazo absoluto.

Uma falha de envio ao coletor incrementa collector_errors, mas não desfaz uma leitura Modbus válida. O ring local é uma janela de observação; não é spool de reenvio.

## Modelo do coletor

Frame::parse valida tamanho, magic, bits reservados e faixas. Store::accept confere sequência monotônica e limite de dispositivos, registra Instant::now e atualiza ring/último valor/contadores sob Mutex.

A API gera um snapshot consistente sob o mesmo lock. A escrita na rede ocorre depois que o lock é liberado. O histórico é serializado enquanto o lock está retido; capacidade máxima pode ampliar esse tempo. A configuração padrão usa 512 entradas.

Os campos timestamp_ms e seq são dados enviados pelo gateway. A idade usa Instant do receptor, evitando que um relógio remoto incorreto mantenha um valor artificialmente atual.

## Modelo da interface

- Assets embutidos com include_str!, sem arquivos estáticos externos obrigatórios no runtime.
- Fetch da mesma origem a cada segundo, com timeout de 2,5 s e sem consultas sobrepostas.
- Indicadores agregam somente dispositivos com idade ≤3 s e API acessível.
- Pausar interrompe as consultas; a idade continua avançando.
- Na perda da API, a tela mantém o contexto histórico e remove validade dos números atuais.
- CSV exporta o último histórico consultado; não solicita nem cria dados adicionais.
- IDs vêm de uint16, valores numéricos são formatados e não há conteúdo HTML controlado por texto de usuário.

## ADRs

| ADR | Decisão | Motivo e trade-off |
|---|---|---|
| 001 | C++ na aquisição, Rust na simulação/coleta | Fronteiras explícitas; exige testar o contrato entre linguagens |
| 002 | BMS1 binário fixo | 31 bytes e offsets estáveis; protocolo próprio sem interoperabilidade geral |
| 003 | Modbus FC03 restrito | Escopo verificável; não implementa todo o Modbus nem mapas de fabricante |
| 004 | Medir antes de prometer desempenho | Percentis com janela e unidade explícitas; sem SLA ou hard real-time |
| 005 | IHM sem framework | Pequeno produto com quatro telas; implementação e build simples |
| 006 | Validade por recebimento monotônico | Independência do relógio do emissor; não mede idade desde o sensor físico |
| 007 | Workers limitados por permits | Evita crescimento ilimitado; conexões excedentes são encerradas sem fila durável |
| 008 | Preservar BMS1 | Evita migração incompatível; reinício de sequência permanece uma limitação explícita |
| 009 | Runtime C++ estático na imagem | Compatibilidade da libstdc++ entre GCC 14 e Debian Bookworm; binário maior |

## Riscos e evolução

A resolução DNS e o connect do cliente C++ não têm um orçamento global independente do sistema operacional. Leituras/excritas usam timeout por operação; isso não equivale a um deadline end-to-end.

Não há ACK, WAL, session ID, identidade de dispositivo ou TLS. Para evoluir a um serviço de rede confiável, esses contratos devem ser projetados e testados antes de ampliar o escopo. Os limites de concorrência atuais não constituem um teste de resistência de longa duração.
