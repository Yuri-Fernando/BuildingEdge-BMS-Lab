# Segurança e limites de exposição

O projeto é destinado a loopback e redes de laboratório com dados sintéticos. Não é um gateway certificado nem um serviço pronto para uma rede predial de produção.

## Controles presentes

- Validação de envelope Modbus, valores BMS1, flags e sequência por dispositivo.
- Buffers limitados e teto de 128 dispositivos.
- Limites de 64 conexões de ingestão, 32 HTTP e 64 no simulador, com liberação de capacidade por RAII.
- HTTP com prazo absoluto de leitura, limite de headers e timeout de escrita.
- API somente de leitura; métodos diferentes de GET recebem 405 e rotas desconhecidas recebem 404.
- Assets da mesma origem, Content-Security-Policy restritiva, nosniff e respostas sem cache.
- Containers sem usuário root, filesystem somente leitura, capabilities removidas, no-new-privileges, limites de recursos e rotação de logs.
- Portas HTTP publicadas somente em 127.0.0.1; redes Docker internas separadas e rede bridge de supervisão para os serviços HTTP.
- Nenhum segredo necessário ao funcionamento do laboratório.

## Fronteiras e riscos restantes

O BMS1 não autentica emissor. Qualquer processo com acesso à ingestão pode enviar frames e ocupar IDs; validar valores não comprova identidade. Não existe TLS, autorização, rate limit por identidade ou proteção contra DoS distribuído.

O limite de conexões evita threads ilimitadas, mas não garante justiça entre clientes. Clientes ociosos ocupam slots até o timeout. A ingestão usa timeout por leitura, sem prazo absoluto para um frame inteiro. O servidor HTTP é uma implementação mínima, sem promessa de conformidade HTTP geral.

Rejeição de replay usa somente sequência e estado em memória. Reinício do coletor apaga essa informação. Reinício do gateway sem reset do coletor exige tratar a limitação de sequência descrita no protocolo.

O timestamp remoto não tem política de plausibilidade. A validade da tela usa o momento de recebimento, não uma medição autenticada da idade no sensor.

## Caminho para outro nível de exposição

Antes de ampliar a exposição: usar servidor HTTP mantido, definir identidade/mTLS e autorização, orçamentos completos de rede, quotas por dispositivo, versão de sessão, persistência com ACK e testes de carga/fuzzing. Esses itens são evolução de produto, não capacidades atuais.

Para relatar um problema, abra uma issue sem credenciais, dados pessoais ou informações de infraestrutura real.
