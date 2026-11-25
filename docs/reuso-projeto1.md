# Reuso da Parte 1

A implementação reutiliza `item.c`, `item.h`, `reader.c` e `reader.h` do repositório `Busca_Organizacao_Dados`. Esses módulos já cobrem cabeçalho, delimitador detectado, valores ausentes, aspas CSV, campos com delimitadores e a correção do cabeçalho do dataset `games`.

A interface do leitor foi estendida com `reader_last_offset`, que registra o offset em bytes antes de ler cada registro. O arquivo é aberto em modo binário para que os offsets coincidam com os bytes do arquivo. A aplicação destrói e recria o `ITEM_STORE` a cada linha indexada, evitando reter os registros completos da base.

A exportação da Parte 1 organiza registros por coleções em memória, então não foi reaproveitada para as buscas do Projeto 2. A busca usa os offsets e copia diretamente os registros CSV encontrados, preservando cabeçalho, delimitador e representação original.
