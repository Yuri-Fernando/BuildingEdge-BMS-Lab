# Histórico de mudanças

## 0.2.0 — 2026-11-16

- IHM web servida pelo coletor Rust: visão geral, inventário, diagnósticos e contrato.
- Gráfico de CO₂, seleção por dispositivo, busca, pausa e exportação CSV do histórico.
- API de histórico, metadados de capacidade e idade monotônica da última amostra.
- Marcação de dados desatualizados e invalidação visual na perda da API.
- Limites de conexões nos dois serviços Rust, leitura HTTP fragmentada e headers de proteção.
- Validação do encoder C++ alinhada ao contrato do parser Rust.
- Testes de contrato HTTP/TCP e validação Docker reproduzível.
- README no padrão do portfólio, referências técnicas, manual operacional e evidências.
- Arquivos de planejamento pessoal e pacote ZIP excluídos da publicação.

## 0.1.0 — 2026-11-14

- Gateway C++17 Modbus TCP FC03, alarme com histerese e reconexão.
- Frame BMS1 com 31 bytes; coletor e simulador Rust.
- Testes do núcleo C++, integração com servidores Python, Docker e configuração de observabilidade.
