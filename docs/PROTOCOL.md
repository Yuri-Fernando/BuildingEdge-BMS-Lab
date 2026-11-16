# Contratos de interface: Modbus TCP e BMS1

## Modbus TCP

Implementação experimental de **Modbus Application Protocol**, Function Code `0x03` (read holding registers). Não há escrita em relés ou controladores. Pedido de 12 bytes:

| Offset | Bytes | Campo | Requisito |
|---:|---:|---|---|
| 0 | 2 | Transaction ID | valor incrementado pelo gateway |
| 2 | 2 | Protocol ID | zero |
| 4 | 2 | Length | `6` (unit + PDU) |
| 6 | 1 | Unit ID | `1` |
| 7 | 1 | Function | `0x03` |
| 8 | 2 | Starting Address | `0` |
| 10 | 2 | Quantity | `6` |

Resposta esperada: 7 bytes MBAP (`length=15`), função `0x03`, byte count `12`, seis registradores big-endian. Antes de aceitar os pontos, o cliente valida o TID da requisição, Protocol ID zero, Unit ID, comprimento consistente, função, byte count e tamanho total. Timeouts, desconexões, exceções e malformed frames são rejeitados.

**Limite deliberado:** contrato estrito de seis registradores para um dispositivo sintético. Não há descoberta de mapa, FC04/FC06/FC16 ou unit IDs múltiplos no simulador. Os registradores são escalados conforme a tabela do README.

## Enlace binário BMS1 (C++17 → Rust)

Formato proprietariamente definido **para o laboratório**. Não é BACnet, MQTT ou padrão aberto. Total **31 bytes**, network-byte-order.

| Offset | Tamanho | Campo | Tipo |
|---:|---:|---|---|
| 0 | 4 | assinatura `BMS1` | ASCII |
| 4 | 2 | device_id | `uint16` |
| 6 | 8 | sequence | `uint64` |
| 14 | 8 | Unix timestamp | `uint64`, ms |
| 22 | 2 | temperatura | `int16`, 0,1 °C |
| 24 | 2 | CO₂ | `uint16`, ppm |
| 26 | 2 | RH | `uint16`, 0,1 % |
| 28 | 1 | flags | bit0 occupied; bit1 HVAC; bit2 CO₂ alarm |
| 29 | 2 | potência | `uint16`, W |

**Sem ACK:** `write_all` confirma envio ao kernel TCP, não persistência no coletor. Após desconexão pode ocorrer perda ou duplicação fora do controle do emissor. O coletor rejeita sequências repetidas dentro da mesma sessão de serviço. A versão de mensagem está na assinatura `BMS1`; mudanças incompatíveis exigem `BMS2`.

**Validação:** `device_id != 0`, `sequence > 0`, temperatura [-50, 125]°C, RH [0,100]%, CO₂ ≤10.000 ppm, potência ≤50.000 W e flags sem bits reservados. Em caso de alteração de contrato, atualizar os dois lados e testes diferenciais.

## API HTTP v0.2

Todas as rotas são GET, sem autenticação, na mesma origem da IHM. Respostas usam Cache-Control: no-store. Outros métodos retornam 405; rotas desconhecidas, 404.

| Rota | Resposta |
|---|---|
| / | IHM HTML |
| /app.css, /app.js | Assets embutidos no binário |
| /healthz | {"status":"ok"} — vida do processo HTTP |
| /v1/latest | Última amostra por ID, contadores e metadados |
| /v1/history | Array samples em ordem de aceitação, limitado à capacidade global |
| /metrics | Exposição textual Prometheus |

Exemplo ilustrativo de /v1/latest (valores variam em execução):

```json
{
  "devices": [{
    "id": 1, "seq": "42", "ts_ms": 1794844800000,
    "temp_c": 23.5, "co2_ppm": 1020, "rh_pct": 45.0,
    "occupied": true, "hvac_on": true, "co2_alarm": true,
    "watts": 500, "age_ms": 120, "stale": false
  }],
  "accepted": 42, "rejected": 0, "evicted": 0,
  "history_size": 42, "capacity": 512, "stale_after_ms": 3000
}
```

seq é string decimal para preservar uint64 em JavaScript. Timestamp continua numérico; o emissor normal usa Unix ms. O coletor não valida a plausibilidade do relógio remoto. age_ms mede o tempo desde a última aceitação usando Instant e stale indica idade maior que 3.000 ms.

O histórico inclui os campos da amostra sem age_ms/stale, pois uma amostra histórica não descreve o estado atual do dispositivo. A exportação CSV é realizada no browser a partir da última consulta bem-sucedida.

### Sessões e reinício

BMS1 não tem identificador de sessão. O gateway inicia sua sequência em 1 a cada processo. O coletor rejeita seq menor ou igual ao último número aceito para aquele ID; após restart do gateway, isso pode durar até ultrapassar a sequência anterior. Reiniciar o coletor cria uma sessão limpa e apaga dados e contadores.

O mapa de últimos valores e a proteção contra replay são mantidos até o coletor reiniciar, independentemente de evicções do histórico. A próxima versão de protocolo deve representar uma sessão explicitamente antes de prometer recuperação transparente de restart.

### Transporte e capacidade

Um socket pode transportar vários frames consecutivos. Cada frame ocupa exatamente 31 bytes; o coletor usa read_exact e encerra a conexão em EOF/timeout, sem aceitar um frame parcial. Não há ACK de aplicação.

O servidor limita conexões simultâneas por categoria. Ao atingir 64 conexões de ingestão ou 32 HTTP, encerra novas conexões dessa categoria. O simulador limita a 64 conexões. Não existe fila durável de requisições nem política por identidade.
