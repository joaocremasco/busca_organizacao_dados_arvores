# Projeto 2 — Árvores de busca

Implementação em C do Projeto 2 de SCC0202. ABB, AVL e rubro-negra são TADs distintos, cada um com tipo opaco, interface própria e módulo próprio. A CLI seleciona o TAD por uma fachada comum. Cada árvore guarda um nó por chave distinta e todos os offsets de registros repetidos.

## Compilar

```sh
make
```

O comando gera `trabalho`. O código não usa `goto`. As chaves são strings comparadas lexicograficamente, com igualdade exata nas consultas.

## Indexar

As bases do Projeto 1 ficam no repositório irmão `../Busca_Organizacao_Dados`:

```sh
./trabalho --mode index --reader nasa \
  --input ../Busca_Organizacao_Dados/asteroid_dataset_20251019.csv \
  --tree avl --key year --index indices/ano.avl.idx --limit 0
```

```sh
./trabalho --mode index --reader games \
  --input ../Busca_Organizacao_Dados/games.csv \
  --tree rubro_negra --key Genres --index indices/generos.rb.idx --limit 0
```

## Buscar

Cada linha de consulta usa `KEY|coluna|valor`. A busca complexa calcula a união dos resultados dos índices fornecidos.

```sh
./trabalho --mode search --reader nasa \
  --input ../Busca_Organizacao_Dados/asteroid_dataset_20251019.csv \
  --index indices/ano.avl.idx --query consultas/asteroides_ano.txt \
  --output resultados/anos.csv
```

Repita `--index` para as colunas de uma busca complexa. O índice registra o tipo de árvore, chave, caminho e metadados da base. O carregamento valida tamanho, data de modificação, dispositivo e inode antes de aceitar o índice. As buscas recuperam registros por offset e preservam o CSV original.

## Organização

- `src/abb.c`, `src/avl.c`, `src/rubro_negra.c`: TADs das três árvores.
- `src/tree.c`: fachada para selecionar a árvore.
- `src/item.c`, `src/reader.c`: modelo e leitor CSV adaptados do Projeto 1, com offsets em bytes.
- `src/tree_entry.c`: chave e coleção de offsets.
- `src/main.c`: CLI, persistência, consultas, exportação e métricas.
- `include/`: interfaces públicas; `consultas/`: exemplos; `docs/`: decisões de implementação.

O leitor processa um registro por vez e libera seu armazenamento temporário após a indexação. A persistência grava a topologia e os metadados de balanceamento, sem ponteiros de memória.
