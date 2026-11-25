# Projeto 2 — Árvores de busca

Implementação em C99 do Projeto 2 de SCC0202. ABB, AVL e rubro-negra são TADs separados, com tipos opacos, interfaces próprias e persistência da topologia. Cada nó guarda uma chave distinta e todos os offsets em bytes dos registros correspondentes.

## Dados

Coloque `asteroid_dataset_20251019.csv` como `dados/asteroids.csv` e o CSV do Projeto 1 como `dados/games.csv`. Veja [dados/README.md](dados/README.md). Os arquivos grandes não são versionados.

As chaves são strings comparadas por igualdade exata. Assim, `2020`, `2020.0` e `02020` são valores diferentes. `Genres` em games contém listas como `Action,Indie`; consultar `Action` não encontra esse valor composto.

## Compilar e testar

```sh
make
make test
make asan-test
```

`make test` compila e executa os testes funcionais e de roundtrip; também roda indexação e busca sob Valgrind quando disponível. `make asan-test` roda a suíte com AddressSanitizer e UndefinedBehaviorSanitizer.

## Indexar e buscar

Asteroides: índices em duas colunas e busca complexa pela união:

```sh
./trabalho --mode index --reader nasa --input dados/asteroids.csv \
  --tree avl --key asteroid_designation --index indices/designation.idx --limit 0
./trabalho --mode index --reader nasa --input dados/asteroids.csv \
  --tree rubro_negra --key year --index indices/year.idx --limit 0
./trabalho --mode search --reader nasa --input dados/asteroids.csv \
  --index indices/designation.idx --query consultas/asteroides_simples.txt \
  --output resultados/asteroides_simples.csv
./trabalho --mode search --reader nasa --input dados/asteroids.csv \
  --index indices/designation.idx --index indices/year.idx \
  --query consultas/asteroides_complexa.txt --output resultados/asteroides_complexa.csv
```

Games: busca simples e complexa:

```sh
./trabalho --mode index --reader games --input dados/games.csv \
  --tree avl --key AppID --index indices/appid.idx --limit 0
./trabalho --mode index --reader games --input dados/games.csv \
  --tree rubro_negra --key Name --index indices/name.idx --limit 0
./trabalho --mode search --reader games --input dados/games.csv \
  --index indices/appid.idx --query consultas/games_simples.txt \
  --output resultados/games_simples.csv
./trabalho --mode search --reader games --input dados/games.csv \
  --index indices/appid.idx --index indices/name.idx \
  --query consultas/games_complexa.txt --output resultados/games_complexa.csv
```

As consultas usam `KEY|coluna|valor`. Na busca complexa, os resultados formam uma união e são deduplicados pelo offset. A saída preserva os bytes do cabeçalho e dos registros. `./exemplos/executar_exemplos.sh` executa os mesmos quatro fluxos e grava saídas em `exemplos/saidas/`.

O formato do índice é versão 3: inteiros de largura fixa em little-endian, nome da base, chave, tamanho, modificação em segundos, contagem de registros e FNV-1a do cabeçalho e das bordas do CSV. A árvore persiste os offsets e a topologia; a AVL também persiste alturas e a rubro-negra persiste cores. A carga valida os metadados e os offsets antes de usar o índice.

`TREE_HEIGHT` conta nós: árvore vazia tem altura 0 e folha altura 1. Rotações duplas contam duas rotações elementares. Recolors contam mudanças efetivas de cor; a cor inicial do nó novo não conta. `READ_MS` e `INSERT_MS` cronometrados por registro têm o pequeno custo das chamadas ao relógio.

## Estrutura

- `src/abb.c`, `src/avl.c`, `src/rubro_negra.c`: TADs de árvore.
- `src/reader.c`: leitor CSV com campos reutilizados, campos ausentes e linhas vazias ignoradas.
- `src/item.c`: chave e offsets do registro indexado.
- `src/index_file.c`, `src/query.c`, `src/fetch.c`, `src/offset_set.c`: persistência, consultas, leitura direta e união de resultados.
- `experimentos/`: scripts, medições reais, resumos estatísticos e gráficos.
- `relatorio/`: escolhas de implementação, complexidade e análise dos resultados medidos.

As medições incluídas foram feitas nas bases reais, com amostras de 1.000 e 10.000 registros, dez repetições e semente `20261125`. Para reproduzi-las, disponibilize as duas bases em `dados/` e execute:

```sh
python3 experimentos/executar.py --sizes 1000,10000 --repetitions 10
python3 experimentos/analise.py
```

O protocolo padrão também oferece 100.000 e 500.000 registros; esses tamanhos não fazem parte dos resultados atualmente incluídos. Os CSVs brutos, resumos, ambiente, testes pareados e gráficos estão em `experimentos/dados/`, `relatorio/` e `experimentos/graficos/`.
