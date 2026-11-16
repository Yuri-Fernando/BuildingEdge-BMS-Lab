# Revisão técnica v0.2

Escopo: código C++ e Rust, contrato Modbus/BMS1, scripts, empacotamento, observabilidade, interface, testes e documentação publicável. Brainstorms, handoff e anotações pessoais não foram tratados como requisitos do produto.

## Diagnóstico e correções

| Achado | Implicação | Correção | Evidência |
|---|---|---|---|
| Encoder C++ menos restritivo que parser Rust | Emissor podia produzir um frame rejeitado pelo receptor | Validação de ID, sequência e potência alinhada | Regressões CTest e integração |
| Leitura HTTP feita com uma única chamada read | Requisição válida fragmentada podia virar 404 | Acúmulo de headers com deadline e limite | Contrato HTTP fragmentado aprovado |
| Uma thread por conexão sem teto | Consumo de recursos sem limite explícito | Permits separados de ingestão/HTTP/simulador | Cargo e recuperação de slots HTTP |
| Última medição sem idade de recebimento | Risco de apresentar valor antigo como atual | Instant por ID, age_ms e validade da IHM | Timestamp futuro, pausa e captura stale |
| Histórico retido sem endpoint de consulta | Operador não conseguia inspecionar a janela | /v1/history, gráfico e CSV | Contrato e download de 512 linhas |
| Coletor Rust não validado na entrega anterior | Interoperabilidade permanecia uma hipótese | Build real e integração C++/Rust | Cargo, quatro cenários e logs |
| Runtime C++ de imagem com versão potencialmente incompatível | Build não garantia execução na imagem final | Runtime C++/GCC estático na imagem | Gateway funcionando no Compose |
| HTTP não alcançável com a rede original neste Docker | Stack adquiria dados, mas a tela não abria | Bridge de supervisão e publicação loopback | Browser, Prometheus UP e Grafana |
| Gateway somente com rodadas finitas | Stack encerraria a aquisição ao atingir o limite | rounds=0 para operação contínua | Compose em execução e encerramento por sinal |
| Testes de falha concentrados em desconexão | Lacunas no caminho de rede do parser | Sete cenários TCP independentes | fault-sanitizer-tests.log |
| Documentação com contexto de vaga e status desatualizado | Portfólio não descrevia o produto de modo independente | Requisitos próprios, README e guias profissionais | Revisão de arquivos versionados |
| Materiais pessoais junto ao código | Risco de publicar notas e pacote de origem | Ignore explícito e inspeção do staging | Git ls-files e status |

## Resultado de engenharia

O laboratório demonstra aquisição por rede real, validação independente em duas linguagens, recuperação após falhas e apresentação conservadora da validade. A arquitetura permanece pequena: três processos no núcleo e dois serviços opcionais de observabilidade.

O frontend não é um mockup. As quatro telas consomem o coletor real. Os números dos screenshots saem do simulador pelo gateway C++ e pelo parser Rust. Os estados de falha foram produzidos interrompendo processos/consultas.

## Pendências priorizadas

1. Sessão de gateway e recuperação pós-restart: BMS1 preserva replay por sequência, mas não distingue uma nova execução do emissor.
2. Confiabilidade da entrega: ACK, spool, recuperação após crash e deduplicação exigem projeto próprio.
3. Prazo global da rede C++: DNS/connect e timeout por operação não formam um deadline end-to-end.
4. Escala e resistência: limites são explícitos, mas faltam testes prolongados e matriz de muitos dispositivos.
5. Exposição pública: autenticação, TLS, quotas por identidade e servidor HTTP mantido.
6. Persistência e política temporal: banco, expiração de IDs e plausibilidade de timestamps.
7. Hardware e conformidade: nenhum resultado local substitui validação com equipamento e protocolo de fabricante.

## Leitura dos resultados

Os detalhes e o ambiente estão em EVIDENCE.md. CI hospedado, execução em container e execução nativa são evidências distintas. O projeto é apresentado como laboratório funcional, com limitações conhecidas, sem alegação de certificação ou prontidão para operação predial crítica.
