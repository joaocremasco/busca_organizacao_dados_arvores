CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -O2
CPPFLAGS ?= -Iinclude

.PHONY: all clean
all: trabalho

# Adicione aqui os módulos das árvores, leitores, persistência e busca.
trabalho:
	@echo "Projeto 2 ainda sem implementação; adicione os fontes em src/ e atualize o Makefile."

clean:
	rm -rf build trabalho
