# Projeto 2 — Árvores de busca

Implementação em C do Projeto 2 de SCC0202. O programa deverá indexar as bases do Projeto 1 com ABB, AVL ou rubro-negra, persistir os índices e executar buscas simples e complexas por offsets.

## Organização

- `include/trabalho/`: interfaces dos TADs e módulos.
- `src/`: árvores, leitores, persistência, busca e CLI.
- `tests/`: testes funcionais e de integração.
- `consultas/`: consultas de exemplo.
- `dados/`: instruções ou links para as bases (evite versionar bases grandes).
- `indices/` e `resultados/`: artefatos gerados localmente.
- `experimentos/`: dados brutos, scripts, gráficos e relatório.
- `docs/`: decisões de projeto e formato do índice.

O Projeto 1 está em `../projeto1`. Reaproveite/adapte seus módulos de leitura CSV e exportação conforme necessário, mantendo a implementação do Projeto 2 independente.

## Compilação e execução

O enunciado exige que `make` produza o executável `trabalho`. Exemplos de uso estarão disponíveis aqui quando a CLI estiver implementada.
