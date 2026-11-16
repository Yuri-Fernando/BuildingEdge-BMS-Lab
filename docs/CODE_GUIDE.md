# Guia de estudo do código

## 1. Modelos e invariantes em C++

Comece por cpp/include/bms/core.hpp. Points contém as grandezas sem dependências de rede. Sample agrega ID, sequência, timestamp e estado do alarme. Essa separação permite testar conversão e protocolo sem sockets.

Ring<T> aloca a capacidade na criação. Ao encher, substitui a posição mais antiga e incrementa dropped. begin e count definem a ordem lógica, e snapshot cria uma cópia em ordem cronológica de inserção. O ring não é thread-safe; o gateway o utiliza no seu próprio ciclo.

Co2Alarm mantém um bit de estado. observe(1000) ativa, observe(950) conserva e observe(900) desativa. É essa memória entre chamadas que evita oscilação no entorno do limiar.

RetryBackoff aumenta a espera por falha até o patamar limitado da implementação e volta à condição inicial depois de uma leitura válida.

## 2. Conversões e bytes

cpp/src/core.cpp concentra o contrato puro. decode_registers exige seis registradores, transforma a temperatura em int16 e valida booleanos e faixas. encode_frame preenche offsets fixos com funções big-endian.

decode_modbus_reply trabalha sobre bytes já lidos. Confere tamanho e MBAP antes de acessar o payload. Isso evita que um pacote bem formado no TCP seja confundido com uma resposta válida à transação atual.

percentile_us ordena uma cópia da janela limitada. O cálculo não precisa reter todas as medições da execução.

## 3. Rede C++

cpp/include/bms/net.hpp declara Connection, ModbusClient e CollectorClient. Connection não pode ser copiada e fecha o descritor no destrutor. ModbusClient mantém TID e conexão persistente.

cpp/src/net.cpp implementa loops para leituras/escritas parciais. Uma resposta pode chegar em vários segmentos TCP. Erro de transporte fecha o socket; a próxima chamada estabelece nova conexão. Uma exceção de protocolo também fecha o socket Modbus para evitar seguir com fluxo desalinhado.

O connect e a resolução de nomes continuam dependentes do sistema operacional. O timeout configurado no socket não deve ser anunciado como um orçamento completo da cadeia.

## 4. Orquestração da aquisição

cpp/src/main.cpp valida argumentos, registra sinais e executa o ciclo. A sequência incrementa por leitura válida. Uma falha Modbus impede criar a amostra. Uma falha de transmissão ao coletor preserva a leitura local.

O JSONL permite observar a aquisição sem depender da IHM. O relatório final resume erros, evicções e latências. rounds=0 significa execução contínua; os testes usam número finito de tentativas.

## 5. Contrato e armazenamento Rust

rust/src/lib.rs implementa Frame::parse de modo independente. O tamanho deve ser exatamente 31 e a assinatura BMS1 deve corresponder. A conversão de cada campo informa endianess e tipo.

Store possui mapa ordenado dos últimos valores, ring global e mapa de Instant de recebimento. accept rejeita seq menor ou igual ao último valor do ID. Depois aplica o teto de IDs, faz a evicção necessária, registra o recebimento e atualiza os contadores.

ConnectionPermit usa compare_exchange_weak para reservar capacidade atomicamente. Se o limite foi atingido, não há thread adicional. O Drop devolve um slot.

## 6. Simulador

rust/src/bin/modbus_sim.rs usa uma sequência compartilhada para gerar um ciclo determinístico de CO₂, temperatura e estados. O servidor valida exatamente a requisição FC03 suportada e monta a resposta MBAP.

drop-every encerra conexões em requisições selecionadas. delay-ms insere atraso. Nenhuma das duas opções modifica equipamento ou rede externa.

## 7. Coletor HTTP/TCP

rust/src/bin/collector.rs mantém o Store em Arc<Mutex<_>> e separa listeners de ingestão e HTTP. Os dois têm limites próprios de conexões.

O worker de ingestão lê um frame inteiro por vez. O worker HTTP acumula headers até CRLFCRLF, com limite e prazo. Depois escolhe uma rota exata, produz o corpo e escreve Content-Length em bytes.

latest inclui idade monotônica e stale. history retorna a janela retida. metrics publica counters e gauges com HELP/TYPE. frame_json representa seq como string para evitar perda de precisão em JavaScript.

Os assets são incluídos em tempo de compilação. Alterar HTML/CSS/JS exige recompilar o binário.

## 8. Interface

web/index.html organiza quatro sections e a navegação. web/app.css define layout, estados, tabela e breakpoints. web/app.js mantém o último snapshot e o instante local da consulta.

isStale soma a idade informada pelo coletor ao tempo transcorrido no browser. Assim, pausar ou atrasar a próxima consulta não congela a medição como atual. Um erro de API invalida todas as leituras apresentadas.

O histórico retido alimenta o SVG e o CSV. A seleção do dispositivo usa IDs numéricos e o filtro da tabela trabalha por texto do ID. Os indicadores excluem valores sem validade.

## 9. Testes como roteiro de leitura

1. CTest: acompanhe cada assert de core até a função correspondente.
2. Cargo: confira parser, replay, retenção e liberação de capacidade.
3. integration.py: siga a criação de processos, portas temporárias, execução do gateway e comparação de contagens.
4. system_tests.py: observe como um cliente externo verifica o servidor Rust por TCP/HTTP, inclusive timestamp futuro.
5. fault_tests.py: examine cada corrupção de resposta Modbus e a comprovação de recuperação.
6. IHM: pause a aquisição, observe o estado stale e depois retome uma sessão válida.

## 10. Exercícios de evolução

- Projete BMS2 com session ID e uma estratégia explícita de migração.
- Acrescente deadline global de rede sem confundir timeout de uma leitura com tempo total.
- Meça o tempo de serialização de histórico na capacidade máxima antes de alterar o modelo de lock.
- Compare retenção global e por dispositivo com uma política de memória definida.
- Implemente ACK somente depois de definir duplicação, crash recovery e persistência.

Cada mudança no contrato deve atualizar emissor, receptor, testes, IHM e documentação na mesma revisão.
