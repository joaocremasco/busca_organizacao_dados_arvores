# Decisões da implementação

- A leitura na indexação é sequencial, uma linha por vez. Cada `ITEM_STORE` temporário é liberado após a inserção.
- As chaves são armazenadas como texto; a igualdade usada nas consultas é exata. Os offsets são `uint64_t` em bytes.
- Um nó representa cada chave distinta e contém um vetor de offsets para preservar ocorrências repetidas.
- O formato persistido é binário versionado e registra árvore, coluna, tipo de chave (`string`), caminho da base, tamanho, data de modificação, dispositivo e inode, contagens, offsets, filhos, alturas e cores. Nenhum endereço de memória é gravado. O carregamento valida esses metadados antes de aceitar o índice.
- O formato binário usa a representação nativa de inteiros do sistema que construiu o índice. Para outra arquitetura, o índice deve ser reconstruído.
- A busca combina os índices por união e deduplica offsets; depois ordena offsets para exportar os registros na ordem original do dataset.
- `TREE_HEIGHT` usa altura em nós: árvore vazia tem altura 0 e folha tem altura 1.
- `ROTATIONS` conta cada rotação elementar, inclusive os dois passos de uma rotação dupla. `RECOLORS` conta alterações efetivas de cor.
