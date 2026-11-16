# Capturas reais da aplicação

Sessão de validação em 16/11/2026, com dados sintéticos e processos reais. Desktop: 1440 × 1000 de viewport; mobile: 390 × 844. As imagens são de página inteira e podem ter altura maior que o viewport. Os hashes e dimensões estão em [screenshots.json](../evidence/screenshots.json).

| Captura | O que mostra |
|---|---|
| [Visão geral](overview.png) | Indicadores atuais, histórico de CO₂ e caminho da telemetria |
| [Dispositivos](devices.png) | Últimos valores e validade por ID |
| [Diagnósticos](diagnostics.png) | Contadores reais e capacidade do histórico |
| [Contrato](protocol.png) | Mapa Modbus e frame BMS1 |
| [Mobile](mobile.png) | Layout responsivo da visão geral |
| [Pausa](paused.png) | Atualização pausada pelo operador |
| [Dados desatualizados](stale.png) | Gateway pausado de verdade, idade vencida e indicadores sem validade |
| [API indisponível](offline.png) | Coletor interrompido de verdade, com página já carregada |
| [Sem dados](empty.png) | Coletor reiniciado enquanto o gateway permanecia pausado |
| [Grafana](grafana.png) | Métricas reais e efeito dos ensaios de pausa/restart |
| [Prometheus](prometheus.png) | Alvo bms-collector em UP |

A pausa e o restart alteram contadores e séries. Por isso as capturas não descrevem o mesmo instante nem devem ser comparadas como snapshots atômicos. A sessão foi restaurada ao estado operacional depois dos ensaios.
