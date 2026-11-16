# Referências técnicas e decisões de implementação

Revisão em 16/11/2026. Foram consultadas fontes primárias; as decisões abaixo são adaptações de engenharia ao escopo deste laboratório.

| Fonte oficial | O que orientou | Aplicação |
|---|---|---|
| [Modbus TCP/IP Implementation Guide V1.0b](https://www.modbus.org/docs/Modbus_Messaging_Implementation_Guide_V1_0b.pdf) | Estrutura MBAP, transaction identifier e separação TCP/PDU | Validação explícita de transaction ID, protocolo, length, unidade e byte count; leitura exata em TCP |
| [Rust TcpStream](https://doc.rust-lang.org/std/net/struct.TcpStream.html) | Leituras podem retornar apenas parte do fluxo; timeouts são configuráveis | HTTP acumula headers fragmentados, limita tamanho e impõe prazo absoluto de 3 s; ingestão usa read_exact |
| [Prometheus: metric and label naming](https://prometheus.io/docs/practices/naming/) | Contadores com total, unidades explícitas e cardinalidade controlada | Contadores de ingestão e evicção; idade da amostra em segundos; label device limitada a 128 dispositivos |
| [Cargo Clippy](https://doc.rust-lang.org/cargo/commands/cargo-clippy.html) | Análise estática complementar aos testes | Gate de lint no CI e revisão do código Rust |
| [Rustup: installation methods](https://rust-lang.github.io/rustup/installation/other.html) | Instalação GNU isolada e no-modify-path | Toolchain temporário, instalador verificado por SHA-256, sem alterar o PATH permanente |

## Decisões

1. **IHM servida pelo coletor.** HTML, CSS e JavaScript são embutidos no binário Rust. Não há serviço frontend separado nem dependências npm no produto. O browser consulta apenas a mesma origem.
2. **Validade depende do recebimento.** Um relógio monotônico mede a idade desde a última aceitação por dispositivo. Um timestamp futuro enviado pelo cliente não mantém uma medição artificialmente atual.
3. **Limites explícitos.** Máximo de 64 conexões de ingestão, 32 HTTP e 64 no simulador. Permits são liberados por RAII. HTTP usa deadline absoluto, limite de headers e tempo máximo de escrita.
4. **Histórico consultável e limitado.** O mesmo ring global do coletor alimenta a IHM e a exportação. Não existe banco escondido; reiniciar o processo apaga os dados.
5. **Semântica de entrega documentada.** Enviar bytes com sucesso não equivale a ACK de aplicação. O sistema mantém transporte best effort e não promete persistência nem exactly once.
6. **Compatibilidade do BMS1 preservada.** Nenhum byte do frame foi movido. A validação do encoder C++ foi alinhada ao parser Rust. A sequência é string no JSON para preservar os 64 bits em JavaScript.
7. **Dados sintéticos identificados.** As capturas mostram a aplicação real conectada aos processos do laboratório; não representam uma instalação predial física.

## Alternativas consideradas

- SPA com React: adicionaria bundler, dependências e processo de build sem necessidade para as quatro telas atuais.
- Banco de séries temporais: útil para retenção durável, mas mudaria o contrato operacional e a infraestrutura; evolução separada.
- Tokio/servidor HTTP de terceiros: indicado para maior escala ou HTTP público. O servidor atual é intencionalmente restrito ao laboratório e possui limites mensuráveis.
- BMS2 com session ID: resolveria reinício de sequência de forma explícita. Foi mantido no roadmap para evitar quebrar o frame BMS1 sem uma migração de contrato.

## Publicação de portas Docker

A [documentação oficial de publicação de portas](https://docs.docker.com/engine/network/port-publishing/) distingue redes internas, gateway e NAT. Nesta execução, PortBindings existia, mas NetworkSettings.Ports estava vazio no coletor ligado apenas à rede interna. A inclusão de managementnet bridge permitiu acesso ao host sem publicar Modbus/BMS1. A solução foi verificada por navegador e API.
