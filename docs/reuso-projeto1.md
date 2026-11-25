# Reuso da Parte 1

O Projeto 2 reaproveita e adapta o leitor CSV da Parte 1, incluindo detecção do delimitador, campos entre aspas, valores ausentes e a correção do cabeçalho unido de `games.csv`. O leitor abre a base em modo binário e mede cada registro antes da leitura para devolver seu offset em bytes.

A API mantém um buffer de linha e um vetor de campos reutilizáveis, sem criar um objeto completo por registro. A indexação consulta uma coluna pelo índice calculado antes do laço e insere sua chave e offset na árvore; assim, só ficam na memória as estruturas do índice e o registro corrente.

A exportação da Parte 1 organizava registros em coleções em memória e não foi reaproveitada. A busca do Projeto 2 usa os offsets persistidos para posicionar-se diretamente na base e copia os registros originais, preservando cabeçalho, delimitador, aspas e quebras de linha.
