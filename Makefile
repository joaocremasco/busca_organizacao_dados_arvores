CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -O2
CPPFLAGS ?= -Iinclude
BUILD := build
SOURCES := src/item.c src/reader.c src/tree_entry.c src/abb.c src/avl.c src/rubro_negra.c src/tree.c src/main.c
OBJECTS := $(SOURCES:src/%.c=$(BUILD)/%.o)

.PHONY: all clean
all: trabalho

trabalho: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) trabalho
