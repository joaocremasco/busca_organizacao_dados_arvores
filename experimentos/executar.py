#!/usr/bin/env python3
"""Executa as medições pareadas nas duas bases e três árvores."""
import argparse
import csv
import hashlib
import os
import platform
import subprocess
import sys
import tempfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TREES = ("abb", "avl", "rubro_negra")
CONFIGS = {
    "asteroids": ("nasa", "dados/asteroids.csv", "year", "on_sentry_list"),
    "games": ("games", "dados/games.csv", "Genres", "Release date"),
}
METRICS_INDEX = ("INPUT_RECORDS", "INDEXED_RECORDS", "DISTINCT_KEYS", "TREE_HEIGHT",
                 "INSERT_COMPARISONS", "ROTATIONS", "RECOLORS", "READ_MS",
                 "INSERT_MS", "BUILD_MS", "SAVE_INDEX_MS")
METRICS_SEARCH = ("INDEX_COUNT", "QUERY_KEYS", "SEARCH_COMPARISONS",
                  "MATCHED_OFFSETS", "OUTPUT_RECORDS", "LOAD_INDEX_MS",
                  "SEARCH_MS", "DEDUP_MS", "FETCH_MS", "WRITE_MS", "TOTAL_MS")


def run_metrics(command):
    result = subprocess.run(command, cwd=ROOT, check=True, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    values = {}
    for line in result.stdout.splitlines():
        if "=" in line:
            name, value = line.split("=", 1)
            values[name] = value
    if values.get("STATUS") != "OK":
        raise RuntimeError("O executável não concluiu a operação: " + " ".join(command))
    return values


def write_rows(path, rows, columns):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=columns)
        writer.writeheader()
        writer.writerows(rows)


def environment_file(path):
    commands = (("uname -a", ["uname", "-a"]),
                ("cc --version", ["cc", "--version"]),
                ("lscpu", ["lscpu"]))
    lines = [f"Data: {datetime.now().astimezone().isoformat()}",
             f"Sistema: {platform.platform()}",
             "Flags do Makefile: -std=c99 -Wall -Wextra -Wpedantic -O2"]
    for label, command in commands:
        try:
            text = subprocess.run(command, check=False, text=True,
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT).stdout
        except OSError as error:
            text = str(error)
        lines.extend((f"\n[{label}]", text.strip()))
    memory = Path("/proc/meminfo")
    if memory.is_file():
        lines.extend(("\n[/proc/meminfo]", memory.read_text(encoding="utf-8").strip()))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def make_queries(source, directory, simple_column, complex_columns):
    command = [sys.executable, str(ROOT / "experimentos/gerar_consultas.py"),
               "--input", str(source), "--simple-column", simple_column,
               "--complex-columns", ",".join(complex_columns),
               "--output-dir", str(directory)]
    subprocess.run(command, cwd=ROOT, check=True)


def index_command(binary, reader, input_path, tree, column, index_path):
    return run_metrics([str(binary), "--mode", "index", "--reader", reader,
                        "--input", str(input_path), "--tree", tree, "--key", column,
                        "--index", str(index_path), "--limit", "0"])


def search_command(binary, reader, input_path, indexes, query, output):
    command = [str(binary), "--mode", "search", "--reader", reader,
               "--input", str(input_path)]
    for index in indexes:
        command.extend(("--index", str(index)))
    command.extend(("--query", str(query), "--output", str(output)))
    return run_metrics(command)


def digest(path):
    value = hashlib.md5()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def run_dataset(binary, name, config, sizes, repetitions, construction, searches):
    reader, relative, primary, secondary = config
    source = ROOT / relative
    if not source.is_file():
        raise FileNotFoundError(f"Base ausente: {source}. Consulte dados/README.md.")
    with tempfile.TemporaryDirectory(prefix=f"aed1_{name}_") as temporary:
        work = Path(temporary)
        bases = work / "bases"
        subprocess.run([sys.executable, str(ROOT / "experimentos/gerar_base.py"),
                        "--input", str(source), "--output-dir", str(bases),
                        "--sizes", ",".join(map(str, sizes))], check=True)
        for size in sizes:
            input_path = bases / f"{source.stem}_{size}.csv"
            query_dir = work / f"queries_{size}"
            make_queries(input_path, query_dir, primary, [primary, secondary])
            for repetition in range(repetitions):
                order = [TREES[(repetition + position) % len(TREES)]
                         for position in range(len(TREES))]
                for tree in order:
                    primary_idx = work / f"{name}_{size}_{repetition}_{tree}_{primary}.idx"
                    secondary_idx = work / f"{name}_{size}_{repetition}_{tree}_{secondary}.idx"
                    for column, index_path in ((primary, primary_idx),
                                               (secondary, secondary_idx)):
                        metrics = index_command(binary, reader, input_path, tree,
                                                column, index_path)
                        row = {"dataset": name, "size": size, "repetition": repetition,
                               "tree": tree, "key": column}
                        row.update({key: metrics.get(key, "") for key in METRICS_INDEX})
                        construction.append(row)
                for search_type, counts, columns in (
                        ("simple", (1, 10, 100, 1000), (primary,)),
                        ("complex", (10, 100, 1000), (primary, secondary))):
                    for count in counts:
                        query_path = query_dir / (
                            f"simples_{count}.txt" if search_type == "simple"
                            else f"complexas_{count}.txt")
                        expected_hashes = {}
                        for tree in order:
                            indexes = [work / f"{name}_{size}_{repetition}_{tree}_{column}.idx"
                                       for column in columns]
                            output = work / f"{name}_{size}_{repetition}_{tree}_{search_type}_{count}.csv"
                            metrics = search_command(binary, reader, input_path,
                                                     indexes, query_path, output)
                            expected_hashes[tree] = digest(output)
                            row = {"dataset": name, "size": size, "repetition": repetition,
                                   "tree": tree, "search_type": search_type,
                                   "query_count": count}
                            row.update({key: metrics.get(key, "") for key in METRICS_SEARCH})
                            searches.append(row)
                        if len(set(expected_hashes.values())) != 1:
                            raise RuntimeError(f"Saídas diferentes: {name}, {size}, {search_type}, {count}")
                        print(f"Saídas iguais: {name}, {size}, {search_type}, {count}, repetição {repetition + 1}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sizes", default="1000,10000,100000,500000")
    parser.add_argument("--repetitions", type=int, default=10)
    parser.add_argument("--datasets", default="asteroids,games")
    parser.add_argument("--binary", type=Path, default=ROOT / "trabalho")
    args = parser.parse_args()
    if not args.binary.is_file():
        raise SystemExit(f"Executável ausente: {args.binary}. Rode make.")
    sizes = [int(size) for size in args.sizes.split(",")]
    if args.repetitions <= 0 or any(size <= 0 for size in sizes):
        raise SystemExit("Repetições e tamanhos devem ser positivos")
    construction, searches = [], []
    for name in args.datasets.split(","):
        if name not in CONFIGS:
            raise SystemExit(f"Base desconhecida: {name}")
        run_dataset(args.binary.resolve(), name, CONFIGS[name], sizes,
                    args.repetitions, construction, searches)
    data_dir = ROOT / "experimentos/dados"
    index_columns = ["dataset", "size", "repetition", "tree", "key", *METRICS_INDEX]
    search_columns = ["dataset", "size", "repetition", "tree", "search_type",
                      "query_count", *METRICS_SEARCH]
    write_rows(data_dir / "construcao.csv", construction, index_columns)
    write_rows(data_dir / "busca.csv", searches, search_columns)
    environment_file(data_dir / "ambiente.txt")
    print(f"Medições registradas em {data_dir}")


if __name__ == "__main__":
    main()
