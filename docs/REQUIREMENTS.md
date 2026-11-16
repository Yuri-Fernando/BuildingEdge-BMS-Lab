# Requisitos técnicos e rastreabilidade

Escopo: laboratório de telemetria predial com processos independentes, dados sintéticos e interfaces de rede reais. Os requisitos são próprios do projeto.

| ID | Requisito verificável | Implementação | Verificação |
|---|---|---|---|
| FR-01 | Ler os seis holding registers por FC03 | cpp/src/net.cpp | CTest; integration.py |
| FR-02 | Rejeitar MBAP, TID, unidade, função e tamanho incompatíveis | decode_modbus_reply | CTest; fault_tests.py |
| FR-03 | Rejeitar grandezas e booleanos fora da faixa | decode_registers / Frame::parse | CTest; Cargo; fault_tests.py |
| FR-04 | Aplicar histerese 1.000/900 ppm | Co2Alarm | CTest; integração |
| FR-05 | Reconectar após desconexão | ModbusClient / RetryBackoff | integração normal/falha |
| FR-06 | Serializar e interpretar BMS1 sem alterar offsets | encode_frame / Frame::parse | CTest; Cargo; integração Rust |
| FR-07 | Rejeitar replay por ID | Store::accept | Cargo; system_tests.py |
| FR-08 | Expor último valor, histórico, saúde e métricas | collector.rs | contratos HTTP/TCP |
| FR-09 | Invalidar dados antigos por tempo de recebimento | Store::age_ms / IHM | teste com timestamp futuro; teste stale; browser |
| FR-10 | Exportar histórico consultado em CSV | web/app.js | browser e inspeção do download |
| FR-11 | Oferecer navegação e busca responsivas | web/ | browser desktop e móvel |
| NFR-01 | Limitar memória histórica e IDs | Rings / MAX_DEVICES | Cargo; testes de retenção |
| NFR-02 | Limitar workers de rede | ConnectionPermit | Cargo; recuperação de slots HTTP |
| NFR-03 | Ler headers HTTP fragmentados com limite e deadline | read_request | system_tests.py |
| NFR-04 | Compilar os serviços e executar testes de modo reproduzível | Dockerfile.validation / CI | build e logs |
| NFR-05 | Documentar falhas e limites sem extrapolar resultados | README / docs | revisão de documentação |
| NFR-06 | Não publicar anotações pessoais ou pacote original | .gitignore / seleção do commit | revisão dos arquivos versionados |

## Fora do escopo

Controle de HVAC, certificação, hardware físico, BACnet/KNX, persistência, multitenancy, autenticação e disponibilidade de produção. Evoluções com critério de aceitação estão em ROADMAP.md.

## Evidência de execução

A existência do teste não implica sua aprovação em todos os ambientes. Consulte EVIDENCE.md para os resultados efetivamente executados e o CI do commit publicado para a validação hospedada.
