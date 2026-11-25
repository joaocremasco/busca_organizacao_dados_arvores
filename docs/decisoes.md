# Decisões da implementação

- A leitura da base é sequencial durante a indexação. O leitor reutiliza um buffer de linha e um vetor de campos; cada chamada fornece os campos até a leitura seguinte. Linhas totalmente vazias são ignoradas.
- As chaves são armazenadas como texto e comparadas por igualdade exata. Por isso, `2020`, `2020.0` e `02020` são valores diferentes. Os offsets são posições em bytes no CSV.
- Cada nó representa uma chave distinta e contém todos os offsets em que ela aparece.
- O índice binário usa versão 3, inteiros de largura fixa em little-endian, tamanho e `mtime` em segundos da base, nome base, contagens e FNV-1a de 64 bits do cabeçalho, dos primeiros 4 KiB e dos últimos 4 KiB. Nenhum endereço de memória é persistido.
- Na busca, os resultados de todas as chaves são unidos, deduplicados por offset e ordenados para exportar registros na ordem original.
- `TREE_HEIGHT` conta nós: árvore vazia tem altura 0 e folha tem altura 1.
- `ROTATIONS` conta cada rotação elementar; uma rotação dupla conta 2. `RECOLORS` conta apenas mudanças efetivas de cor; a cor inicial do nó novo não conta.
