# Manual operacional

## Subir, observar e parar

```bash
docker compose up -d --build
docker compose ps
docker compose logs --tail 30 gateway collector
curl http://localhost:18080/healthz
curl http://localhost:18080/v1/latest
docker compose down
```

A primeira consulta pode retornar devices vazio enquanto o gateway conecta. O /healthz comprova resposta do processo, não uma leitura recente.

## Testar perda da aquisição

```bash
docker compose stop gateway
# aguarde mais de 3 segundos e observe a IHM
docker compose start gateway
```

O gateway volta à sequência inicial quando seu processo reinicia. Se o coletor já possui uma sequência maior, rejeitará a sequência reiniciada. Para iniciar uma sessão limpa:

```bash
docker compose stop gateway
docker compose restart collector
docker compose start gateway
```

Essa operação apaga o histórico em memória. Para ensaiar uma interrupção sem reiniciar a sequência, pode-se pausar/despausar o container do gateway:

```bash
docker compose pause gateway
docker compose unpause gateway
```

Durante a pausa, o coletor pode encerrar o socket ocioso. O transporte best effort pode perder uma amostra na retomada; a recuperação não deve ser interpretada como garantia de entrega.

## Simular falhas Modbus

Em execução nativa:

```bash
./rust/target/debug/modbus-sim --drop-every 7 --delay-ms 20
```

O contador de requisições é compartilhado pelo simulador. Cada sétima requisição encerra a conexão antes da resposta. O gateway registra o erro, aguarda o backoff e reconecta.

Os cenários de TID, unidade, exceção, length e truncamento são exercitados por scripts/fault_tests.py contra um servidor Python independente.

## Interpretar os estados da tela

| Estado | Significado | Ação |
|---|---|---|
| Conectando | Consulta inicial ainda não concluída | Aguardar a API |
| Sem dados | API respondeu; nenhum frame aceito | Conferir gateway, portas e logs |
| Coletor conectado | Há ao menos uma medição atual | Observar alarmes e contadores |
| Dados desatualizados | Sem recebimento válido há mais de 3 s | Investigar aquisição e ingestão |
| API indisponível | Fetch falhou ou expirou | Conferir coletor e conectividade |
| Atualização pausada | Operador interrompeu consultas | Retomar; validade continua expirando |

O badge não representa uma verificação de todos os sensores físicos. A tabela mostra validade individual.

## Diagnóstico

- **Rejeições crescem após restart:** provável sequência reiniciada. Faça uma sessão limpa ou espere a sequência ultrapassar a anterior.
- **Evicções crescem continuamente:** esperado quando o ring enche. Indica retenção limitada, não perda de todos os últimos valores.
- **HTTP responde, mas devices vazio:** serviço vivo sem dados aceitos. Verifique o caminho Modbus e o destino BMS1.
- **collector_errors aumenta:** o gateway percebeu falha de envio. Não há spool automático; consulte JSONL para a aquisição local.
- **Porta ocupada:** identifique o serviço local antes de mudar portas. As portas padrão do lab são 18080, 19090 e 13000.
- **Docker não monta os arquivos do G:** use checkout local para executar. Não copie .env, credenciais ou notas pessoais para a imagem.
- **Mudou web/ e a tela continua igual:** os assets são embutidos no binário. Recompile/recrie o coletor.

## Retenção e desligamento

Não há volumes de dados no núcleo. O histórico e os contadores pertencem ao processo Rust. O CSV é a exportação explícita do histórico consultado. Prometheus usa retenção de um dia na extensão; sem volume persistente, recriar seu container remove a série local.

As portas Modbus e BMS1 não são publicadas pelo Compose. Para diagnóstico, use os serviços na rede Docker, sem abrir portas de campo no host.

## Evidências

Registre comando, versão, código de saída, parâmetros e origem das medições. Capturas devem indicar dados sintéticos e estado real. Não use uma captura histórica como prova de disponibilidade atual.
