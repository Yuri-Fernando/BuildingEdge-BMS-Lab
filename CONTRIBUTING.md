# Contribuição

Use mudanças pequenas e verificáveis. O produto tem C++17, Rust 2021 e frontend sem dependências externas.

1. Leia docs/PROTOCOL.md antes de alterar tipos, faixas ou offsets.
2. Mantenha compatibilidade BMS1 ou proponha uma nova versão com estratégia de migração.
3. Execute CTest, Cargo, integração nos modos Python/Rust e os testes de contrato/falhas.
4. Rode cargo fmt e cargo clippy sem warnings.
5. Confira a IHM em desktop e mobile se alterar web/ ou JSON.
6. Registre evidência de novas capacidades; diferencie teste local, container e CI.
7. Atualize CHANGELOG.md e os documentos afetados.

Nunca inclua credenciais, dados de uma instalação real, caches de build, notas pessoais ou imagens que não correspondam à aplicação executada. O projeto não exige serviços pagos.
