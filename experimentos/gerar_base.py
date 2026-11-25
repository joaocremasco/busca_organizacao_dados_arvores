#!/usr/bin/env python3
"""Gera amostras iniciais ou cópias cíclicas sem guardar a base na memória."""
import argparse
import csv
import random
from pathlib import Path

SEED = 20261125


def create_variant(source, destination, wanted, seed):
    destination.parent.mkdir(parents=True, exist_ok=True)
    randomizer = random.Random(seed)
    sample = []
    record_count = 0
    with source.open(newline="", encoding="utf-8-sig") as original:
        reader = csv.reader(original)
        header = next(reader, None)
        if header is None:
            raise ValueError(f"CSV vazio: {source}")
        for row in reader:
            if not row:
                continue
            record_count += 1
            if len(sample) < wanted:
                sample.append((record_count, row))
            else:
                replacement = randomizer.randrange(record_count)
                if replacement < wanted:
                    sample[replacement] = (record_count, row)
    if record_count == 0:
        raise ValueError(f"A base não possui registros: {source}")
    sample.sort(key=lambda item: item[0])
    selected = [row for _, row in sample]
    while len(selected) < wanted:
        selected.append(randomizer.choice(sample)[1])
    randomizer.shuffle(selected)
    with destination.open("w", newline="", encoding="utf-8") as output:
        writer = csv.writer(output)
        writer.writerow(header)
        writer.writerows(selected)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--sizes", default="1000,10000,100000,500000")
    parser.add_argument("--seed", type=int, default=SEED)
    args = parser.parse_args()
    if not args.input.is_file():
        raise SystemExit(f"Base ausente: {args.input}")
    sizes = [int(value) for value in args.sizes.split(",")]
    for size in sizes:
        if size <= 0:
            raise SystemExit("Os tamanhos devem ser positivos")
        destination = args.output_dir / f"{args.input.stem}_{size}.csv"
        create_variant(args.input, destination, size, args.seed)
        print(f"{destination}: {size} registros; amostra aleatória, semente {args.seed}")


if __name__ == "__main__":
    main()
