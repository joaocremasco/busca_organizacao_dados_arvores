#!/usr/bin/env python3
"""Cria consultas repetíveis com valores presentes, ausentes e repetidos."""
import argparse
import csv
import random
from pathlib import Path

SEED = 20261125
COUNTS = (1, 10, 100, 1000)


def read_keys(path, columns):
    found = {column: set() for column in columns}
    with path.open(newline="", encoding="utf-8-sig") as source:
        rows = csv.reader(source)
        header = next(rows, None)
        if header is None:
            raise ValueError(f"CSV vazio: {path}")
        if "DiscountDLC count" in header:
            position = header.index("DiscountDLC count")
            header[position:position + 1] = ["Discount", "DLC count"]
        indexes = {column: header.index(column) for column in columns}
        for row in rows:
            if not row:
                continue
            for column, position in indexes.items():
                if position < len(row):
                    value = row[position]
                    if "\n" not in value and "\r" not in value:
                        found[column].add(value)
    for column, values in found.items():
        if not values:
            raise ValueError(f"Nenhum valor consultável encontrado na coluna '{column}'")
    return {column: sorted(values) for column, values in found.items()}


def make_values(values, count, randomizer, label):
    output = []
    last_existing = None
    for i in range(count):
        if i % 4 == 1:
            candidate = f"__INEXISTENTE_{label}_{i}__"
            while candidate in values:
                candidate += "X"
            output.append(candidate)
        elif i % 4 == 2 and last_existing is not None:
            output.append(last_existing)
        else:
            last_existing = randomizer.choice(values)
            output.append(last_existing)
    return output


def write_query(path, columns, keys):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as output:
        for i, key in enumerate(keys):
            column = columns[i % len(columns)]
            output.write(f"KEY|{column}|{key}\n")


def make_complex_values(columns, keys, count, randomizer):
    output = []
    previous = {}
    for i in range(count):
        column = columns[i % len(columns)]
        values = keys[column]
        if i % 4 == 1:
            value = f"__INEXISTENTE_COMPLEX_{column}_{i}__"
            while value in values:
                value += "X"
        elif i % 4 == 2 and column in previous:
            value = previous[column]
        else:
            value = randomizer.choice(values)
        output.append(value)
        previous[column] = value
    return output


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=root / "dados/asteroids.csv")
    parser.add_argument("--simple-column", required=True)
    parser.add_argument("--complex-columns", required=True,
                        help="duas colunas separadas por vírgula")
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    if not args.input.is_file():
        raise SystemExit(f"Base ausente: {args.input}. Consulte dados/README.md.")
    complex_columns = args.complex_columns.split(",")
    if len(complex_columns) < 2 or len(set(complex_columns)) != len(complex_columns):
        raise SystemExit("A consulta complexa exige colunas distintas")
    keys = read_keys(args.input, [args.simple_column] + complex_columns)
    randomizer = random.Random(SEED)
    print(f"Semente fixa: {SEED}")
    print("Seleção: valores presentes aleatórios, valores __INEXISTENTE_*__ e repetição do valor anterior.")
    for count in COUNTS:
        simple = make_values(keys[args.simple_column], count, randomizer, "SIMPLE")
        complex_values = make_complex_values(complex_columns, keys, count, randomizer)
        write_query(args.output_dir / f"simples_{count}.txt",
                    [args.simple_column], simple)
        write_query(args.output_dir / f"complexas_{count}.txt",
                    complex_columns, complex_values)


if __name__ == "__main__":
    main()
