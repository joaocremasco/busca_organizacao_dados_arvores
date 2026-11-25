CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -O2 -MMD -MP
CPPFLAGS ?= -Iinclude
BUILD := build
SOURCES := src/util.c src/timer.c src/item.c src/reader.c src/abb.c src/avl.c src/rubro_negra.c src/tree.c src/index_file.c src/cli.c src/query.c src/offset_set.c src/fetch.c src/app.c src/main.c
OBJECTS := $(SOURCES:src/%.c=$(BUILD)/%.o)
TREE_OBJECTS := $(BUILD)/util.o $(BUILD)/item.o $(BUILD)/abb.o $(BUILD)/avl.o $(BUILD)/rubro_negra.o $(BUILD)/tree.o
ASAN_CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: all clean
all: trabalho

.PHONY: test asan asan-test
test: trabalho $(BUILD)/test_roundtrip
	./tests/run_tests.sh

asan: trabalho_asan

asan-test: trabalho_asan $(BUILD)/test_roundtrip_asan
	ASAN_OPTIONS=detect_leaks=0 BINARY=./trabalho_asan ROUNDTRIP=./$(BUILD)/test_roundtrip_asan SKIP_VALGRIND=1 ./tests/run_tests.sh

trabalho: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/test_roundtrip: tests/test_roundtrip.c $(TREE_OBJECTS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

$(BUILD)/test_roundtrip_asan: tests/test_roundtrip.c src/util.c src/item.c src/abb.c src/avl.c src/rubro_negra.c src/tree.c
	$(CC) $(CPPFLAGS) $(ASAN_CFLAGS) $^ -o $@

trabalho_asan: $(SOURCES) $(wildcard include/*.h)
	$(CC) $(CPPFLAGS) $(ASAN_CFLAGS) $(SOURCES) -o $@

clean:
	rm -rf $(BUILD) trabalho trabalho_asan

-include $(OBJECTS:.o=.d)
