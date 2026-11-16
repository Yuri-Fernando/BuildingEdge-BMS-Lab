# Verificação da interface

Data: 16/11/2026. Ferramenta: Playwright em Chromium, aplicação real no loopback.

- As quatro telas foram abertas e suas headings/conteúdos verificados.
- Buscar ID 999 produziu ausência de correspondência; limpar a busca restaurou a linha do dispositivo.
- A exportação foi baixada pela própria interface e conferida como CSV com colunas de telemetria.
- Pausar consultas deixou a idade avançar; ao expirar, CO₂ foi apresentado como travessão.
- Pausar o container do gateway produziu "Dados desatualizados", 0/1 dispositivos atuais e indicadores sem valor.
- Parar o coletor produziu "API indisponível", mantendo a página já carregada.
- Reiniciar o coletor com gateway pausado produziu "Sem dados".
- Retomar o gateway restabeleceu amostra atual e resposta de API.
- Em viewport 390 × 844, a página não apresentou overflow horizontal global; a tabela usa scroll próprio.
- Uma única aba de navegação fica com aria-pressed=true.
- A versão final da IHM apresentou zero erros e zero warnings no console durante operação normal.

Os erros de rede durante a interrupção intencional da API são esperados. No Grafana 11.4, a navegação registrou aviso de meta tag depreciada e erro de instrumentação Performance.mark da própria biblioteca; os painéis receberam séries e o target permaneceu UP. Esses avisos não são contabilizados como erros da IHM BuildingEdge.

As capturas públicas estão em docs/images. Artefatos brutos de automação e o pacote original não são versionados.
