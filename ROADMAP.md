# Roadmap por critérios de aceitação

## v0.2 — operação e qualidade

- IHM de quatro telas, histórico, CSV e validade por recebimento.
- Conexões e retenção limitadas.
- Contratos HTTP/TCP e cenários independentes de falha Modbus.
- Containers, documentação e evidências visuais.
- Resultados executados em docs/EVIDENCE.md; CI acompanha o commit publicado.

## Próximas etapas

| Prioridade | Entrega | Gate |
|---|---|---|
| 1 | BMS2 com sessão | Reiniciar gateway sem bloquear nova sequência; migração testada |
| 2 | Deadline completo de rede | DNS/connect/read/write exercitados contra destino lento e indisponível |
| 3 | ACK e spool | Crash antes/depois do ACK, deduplicação e recuperação reproduzíveis |
| 4 | Paralelismo de dispositivos | Link lento isolado; p99, CPU e RSS para 1/10/100 IDs |
| 5 | Identidade e TLS | Dispositivo desconhecido rejeitado; credenciais rotacionadas |
| 6 | Fuzzing e carga longa | Corpus, parâmetros e limites registrados; nenhum crescimento sem limite |
| 7 | Adapters de campo | BACnet/KNX com biblioteca mantida e fixtures adequadas |
| 8 | Hardware edge | Ambiente e interfaces identificados; medições sem extrapolar resultados do host |

As etapas não são promessas de capacidade atual. Nenhuma integração comercial ou certificação é presumida.
