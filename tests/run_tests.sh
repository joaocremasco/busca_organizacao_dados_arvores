#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN=${BINARY:-$ROOT/trabalho}
ROUNDTRIP=${ROUNDTRIP:-$ROOT/build/test_roundtrip}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT HUP INT TERM

fail() {
    echo "ERRO: $*" >&2
    exit 1
}

python3 - "$ROOT/tests/dados/complex.csv" "$WORK" <<'PY'
from pathlib import Path
import sys
root = Path(sys.argv[1]).parent
work = Path(sys.argv[2])
data = (root / "complex.csv").read_bytes()
header = b"year,on_sentry_list," + b"p" * 83 + b"\n"
row1 = b"2028,true," + b"a" * 133 + b"\n"
row2 = b"2031,true," + b"b" * 153 + b"\n"
row3 = b"2028,false," + b"c" * 151 + b"\n"
assert len(header) == 104
assert len(header + row1) == 248
assert len(header + row1 + row2) == 412
(work / "scenario.csv").write_bytes(header + row1 + row2 + row3)
(work / "simple.query").write_text("KEY|group|a\nKEY|group|b\nKEY|group|a\n", encoding="utf-8")
(work / "union.query").write_text("KEY|year|2028\nKEY|on_sentry_list|true\n", encoding="utf-8")
(work / "missing.query").write_text("KEY|group|does-not-exist\n", encoding="utf-8")
(work / "scenario.expected").write_bytes(header + row1 + row2 + row3)
PY

python3 - "$ROOT/tests/dados/complex.csv" "$WORK/simple.query" "$WORK/simple.expected" <<'PY'
import csv
import io
from pathlib import Path
import sys

def split_records(data):
    start, quoted, i = 0, False, 0
    while i < len(data):
        byte = data[i]
        if byte == 34:
            if quoted and i + 1 < len(data) and data[i + 1] == 34:
                i += 1
            else:
                quoted = not quoted
        elif byte == 10 and not quoted:
            yield data[start:i + 1]
            start = i + 1
        i += 1
    if start < len(data):
        yield data[start:]

data = Path(sys.argv[1]).read_bytes()
records = list(split_records(data))
header = records[0]
queries = [line.strip().split("|", 2)[1:] for line in Path(sys.argv[2]).read_text().splitlines()]
header_fields = next(csv.reader(io.StringIO(header.decode(), newline="")))
positions = {name: i for i, name in enumerate(header_fields)}
selected = []
for record in records[1:]:
    fields = next(csv.reader(io.StringIO(record.decode(), newline="")))
    if not fields:
        continue
    if any(fields[positions[column]] == value for column, value in queries):
        selected.append(record)
Path(sys.argv[3]).write_bytes(header + b"".join(selected))
PY

for tree in abb avl rubro_negra; do
    "$BIN" --mode index --reader nasa --input "$ROOT/tests/dados/complex.csv" \
        --tree "$tree" --key group --index "$WORK/$tree-group.idx" --limit 0 >/dev/null
    "$BIN" --mode search --reader nasa --input "$ROOT/tests/dados/complex.csv" \
        --index "$WORK/$tree-group.idx" --query "$WORK/simple.query" \
        --output "$WORK/$tree.csv" >"$WORK/$tree.metrics"
    cmp "$WORK/simple.expected" "$WORK/$tree.csv" || fail "$tree: saída simples divergiu"
done
test "$(md5sum "$WORK/abb.csv" | cut -d' ' -f1)" = "$(md5sum "$WORK/avl.csv" | cut -d' ' -f1)" || fail "ABB e AVL divergiram"
test "$(md5sum "$WORK/abb.csv" | cut -d' ' -f1)" = "$(md5sum "$WORK/rubro_negra.csv" | cut -d' ' -f1)" || fail "ABB e RB divergiram"

"$BIN" --mode index --reader nasa --input "$ROOT/tests/dados/complex.csv" \
    --tree abb --key kind --index "$WORK/kind.idx" --limit 0 >/dev/null
printf 'KEY|group|a\nKEY|kind|y\n' >"$WORK/union-simple.query"
"$BIN" --mode search --reader nasa --input "$ROOT/tests/dados/complex.csv" \
    --index "$WORK/abb-group.idx" --index "$WORK/kind.idx" \
    --query "$WORK/union-simple.query" --output "$WORK/union.csv" >"$WORK/union.metrics"
grep -q '^MATCHED_OFFSETS=4$' "$WORK/union.metrics" || fail "contagem bruta da união incorreta"
grep -q '^OUTPUT_RECORDS=3$' "$WORK/union.metrics" || fail "deduplicação da união incorreta"

"$BIN" --mode index --reader nasa --input "$WORK/scenario.csv" --tree abb \
    --key year --index "$WORK/year.idx" --limit 0 >/dev/null
"$BIN" --mode index --reader nasa --input "$WORK/scenario.csv" --tree rubro_negra \
    --key on_sentry_list --index "$WORK/sentry.idx" --limit 0 >/dev/null
"$BIN" --mode search --reader nasa --input "$WORK/scenario.csv" \
    --index "$WORK/year.idx" --index "$WORK/sentry.idx" \
    --query "$WORK/union.query" --output "$WORK/scenario.csv.out" >"$WORK/scenario.metrics"
grep -q '^MATCHED_OFFSETS=4$' "$WORK/scenario.metrics" || fail "cenário de offsets: contagem bruta incorreta"
grep -q '^OUTPUT_RECORDS=3$' "$WORK/scenario.metrics" || fail "cenário de offsets: saída incorreta"
cmp "$WORK/scenario.expected" "$WORK/scenario.csv.out" || fail "cenário de offsets divergiu"

"$BIN" --mode search --reader nasa --input "$ROOT/tests/dados/complex.csv" \
    --index "$WORK/abb-group.idx" --query "$WORK/missing.query" \
    --output "$WORK/missing.csv" >"$WORK/missing.metrics"
grep -q '^STATUS=OK$' "$WORK/missing.metrics" || fail "chave inexistente falhou"
grep -q '^OUTPUT_RECORDS=0$' "$WORK/missing.metrics" || fail "chave inexistente teve resultados"

"$BIN" --mode index --reader nasa --input "$ROOT/tests/dados/blank_end.csv" \
    --tree avl --key group --index "$WORK/blank.idx" --limit 0 >/dev/null
printf 'KEY|group|b\n' >"$WORK/blank.query"
"$BIN" --mode search --reader nasa --input "$ROOT/tests/dados/blank_end.csv" \
    --index "$WORK/blank.idx" --query "$WORK/blank.query" \
    --output "$WORK/blank.csv" >"$WORK/blank.metrics"
grep -q '^OUTPUT_RECORDS=1$' "$WORK/blank.metrics" || fail "registro após linha vazia final não foi encontrado"
grep -q '^2,b$' "$WORK/blank.csv" || fail "busca após linha vazia devolveu registro errado"

"$BIN" --mode index --reader nasa --input "$ROOT/tests/dados/complex.csv" \
    --tree avl --key group --index "$WORK/unused.idx" --limit abc \
    >"$WORK/limit.out" 2>"$WORK/limit.err" && fail "--limit inválido foi aceito"
"$BIN" --mode search --reader nasa --input "$ROOT/tests/dados/complex.csv" \
    --index "$WORK/abb-group.idx" --index "$WORK/abb-group.idx" \
    --query "$WORK/simple.query" --output "$WORK/duplicate-column.csv" \
    >"$WORK/duplicate-column.out" 2>"$WORK/duplicate-column.err" &&
    fail "índices repetidos para a mesma coluna foram aceitos"

cp "$ROOT/tests/dados/complex.csv" "$WORK/changed.csv"
"$BIN" --mode search --reader nasa --input "$WORK/changed.csv" \
    --index "$WORK/abb-group.idx" --query "$WORK/simple.query" \
    --output "$WORK/invalid.csv" >"$WORK/invalid.metrics" 2>"$WORK/invalid.err" &&
    fail "índice incompatível foi aceito"
grep -q 'Índice incompatível' "$WORK/invalid.err" || fail "erro de incompatibilidade não foi explicado"

if command -v valgrind >/dev/null 2>&1 && [ "${SKIP_VALGRIND:-0}" != 1 ]; then
    valgrind --quiet --leak-check=full --error-exitcode=99 "$BIN" \
        --mode index --reader nasa --input "$ROOT/tests/dados/complex.csv" \
        --tree avl --key group --index "$WORK/vg.idx" --limit 0 >/dev/null
    valgrind --quiet --leak-check=full --error-exitcode=99 "$BIN" \
        --mode search --reader nasa --input "$ROOT/tests/dados/complex.csv" \
        --index "$WORK/vg.idx" --query "$WORK/simple.query" \
        --output "$WORK/vg.csv" >/dev/null
fi

python3 - "$WORK/ordered.csv" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
with path.open("w", newline="") as output:
    output.write("id,value\n")
    for i in range(20000):
        output.write(f"{i:05d},v{i}\n")
PY
for tree in abb avl; do
    "$BIN" --mode index --reader nasa --input "$WORK/ordered.csv" --tree "$tree" \
        --key id --index "$WORK/ordered-$tree.idx" --limit 0 >"$WORK/ordered-$tree.metrics"
    printf 'KEY|id|19999\n' >"$WORK/ordered.query"
    "$BIN" --mode search --reader nasa --input "$WORK/ordered.csv" \
        --index "$WORK/ordered-$tree.idx" --query "$WORK/ordered.query" \
        --output "$WORK/ordered-$tree.csv" >/dev/null
done
cmp "$WORK/ordered-abb.csv" "$WORK/ordered-avl.csv" || fail "ABB ordenada divergiu da AVL"

if [ -x "$ROUNDTRIP" ]; then
    "$ROUNDTRIP"
else
    fail "teste de roundtrip não foi compilado"
fi

echo "testes de integração: OK"
