# Evidências da revisão v0.2

Data da revisão: **16/11/2026**. Fonte oficial: pasta BuildingEdge BMS Lab no G:. Build e testes em Docker Linux a partir de cópia seletiva do código no disco local, com conferência SHA-256 dos arquivos. Notas pessoais, ZIP, credenciais e dados externos não fazem parte da cópia de validação.

## Ambiente

- Host Windows; Docker Desktop / Engine 29.8.0, containers Linux.
- Runtime de validação: Debian Bookworm, GCC 12.2.0, Rust 1.85 e Python 3.11 da imagem.
- Imagem C++ de produção do laboratório: GCC 14.3.0 no build, Debian Bookworm no runtime, libstdc++/libgcc estáticas.
- Toolchain Rust temporário no Windows: 1.100.0 GNU, usado para rustfmt/Clippy. O link nativo encontrou ausência de libpthread.a na distribuição MinGW instalada; os binários e a integração foram validados em Linux/Docker.
- As medições abaixo são de loopback dentro do container. Não medem rede física, browser, hardware de campo ou SLA de produção.

## Matriz executada

| Verificação | Resultado | Evidência |
|---|---|---|
| Cargo unit tests | 5 aprovados | validation-build.log |
| CTest Debug | 1 executável de contratos aprovado | validation-tests.log |
| CTest Release / GCC 14 | Aprovado durante build da imagem | compose-build.log |
| ASan + UBSan | CTest aprovado | fault-sanitizer-tests.log |
| Integração Python, normal e falha | 2 cenários aprovados | validation-tests.log |
| Integração Rust, normal e falha | 2 cenários aprovados | validation-tests.log |
| Contratos HTTP/TCP do coletor | 6 aprovados | validation-tests.log |
| Falhas Modbus com oráculo independente | 7 aprovadas | fault-sanitizer-tests.log |
| rustfmt / Clippy | Sem warnings após correções | verificação local; gate também no CI |
| Stack Compose | 3 serviços iniciados | compose-build.log |
| Navegação e capturas da IHM | Verificadas no browser desktop e móvel | images/ e relatório de browser |
| CI hospedado | Acompanhar commit publicado | workflow GitHub Actions |

Os testes de unidade C++ estão agrupados em um executável CTest; não são apresentados como dezenas de casos independentes.

## Integração medida

80 tentativas por cenário, espera de 5 ms entre tentativas bem-sucedidas, ring local com capacidade 12.

| Receptor de referência | Injeção | Leituras válidas | Erros Modbus | Erros de envio | Evicções locais | p50 / p95 / p99 (µs) |
|---|---|---:|---:|---:|---:|---|
| Python | nenhuma | 80 | 0 | 0 | 68 | 186 / 220 / 754 |
| Python | desconexão a cada 7 | 69 | 11 | 0 | 57 | 190 / 628 / 713 |
| Rust | nenhuma | 80 | 0 | 0 | 68 | 149 / 177 / 262 |
| Rust | desconexão a cada 7 | 69 | 11 | 0 | 57 | 149 / 372 / 389 |

Os percentis usam apenas leituras válidas da janela, excluindo falhas e backoff. A tabela caracteriza esta execução curta e não é uma comparação controlada de desempenho entre linguagens.

## O que os testes externos demonstram

- API vazia e assets servidos pelo processo Rust.
- Retenção com capacidade 2, evicção da amostra antiga e rejeição de replay.
- Aceitação de um frame fragmentado em várias escritas TCP.
- Rejeição de flags inválidas.
- Validade expirada mesmo com timestamp remoto futuro.
- Requisição HTTP fragmentada e rejeição de POST.
- Recuperação de capacidade após fechar conexões ociosas.
- Recuperação do gateway após TID/unidade incorretos, exceção Modbus, tamanho inválido e frame truncado.
- Grandeza fora da faixa não publicada como medição válida.
- Resposta TCP fragmentada aceita corretamente pelo gateway.

## O que não foi demonstrado

Hardware físico, certificação, instalações reais, ACK/persistência, autenticação/TLS, carga prolongada, tolerância a todos os padrões de DoS e deadline global de DNS/connect. Prometheus apresentou o alvo bms-collector em UP e o Grafana consultou séries reais de ingestão, throughput e retenção. A IHM própria não depende desses serviços.

## Reproduzir

```bash
docker build -f docker/Dockerfile.validation -t buildingedge-validation .
docker run --rm buildingedge-validation
```

Os logs públicos contêm somente processos do laboratório e dados sintéticos. As capturas representam sessões reais da aplicação; estados sem dados, stale e offline devem ser lidos conforme suas legendas.

## Reexecução final e artefatos

O comando Docker documentado foi reconstruído e executado novamente com todos os scripts incluídos: CTest, os quatro cenários de integração, seis contratos do coletor e sete cenários de falha passaram. Consulte [build final](evidence/final-validation-build.log) e [testes finais](evidence/final-validation-tests.log). A tabela de latência acima corresponde à primeira execução registrada, mantida para rastreabilidade.

Foram conferidas 11 imagens PNG, um CSV exportado com 512 linhas e 30 hashes de código/configuração. Os hashes registram os bytes do workspace comparados à cópia local de validação; o Git normaliza finais de linha para LF ao publicar. Logs de console tiveram somente finais de linha e espaços finais normalizados.

A verificação visual está em [browser-verification.md](evidence/browser-verification.md); a galeria completa está em [images](images/README.md).
