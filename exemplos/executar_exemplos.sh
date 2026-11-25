#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
for data in dados/asteroids.csv dados/games.csv; do
    if [ ! -f "$data" ]; then
        echo "Base ausente: $ROOT/$data. Consulte dados/README.md." >&2
        exit 1
    fi
done

./trabalho --mode index --reader nasa --input dados/asteroids.csv \
    --tree avl --key asteroid_designation --index indices/asteroid_designation.idx --limit 0
./trabalho --mode index --reader nasa --input dados/asteroids.csv \
    --tree rubro_negra --key year --index indices/asteroid_year.idx --limit 0
./trabalho --mode search --reader nasa --input dados/asteroids.csv \
    --index indices/asteroid_designation.idx --query consultas/asteroides_simples.txt \
    --output exemplos/saidas/asteroides_simples.csv
./trabalho --mode search --reader nasa --input dados/asteroids.csv \
    --index indices/asteroid_designation.idx --index indices/asteroid_year.idx \
    --query consultas/asteroides_complexa.txt \
    --output exemplos/saidas/asteroides_complexa.csv

./trabalho --mode index --reader games --input dados/games.csv \
    --tree avl --key AppID --index indices/games_appid.idx --limit 0
./trabalho --mode index --reader games --input dados/games.csv \
    --tree rubro_negra --key Name --index indices/games_name.idx --limit 0
./trabalho --mode search --reader games --input dados/games.csv \
    --index indices/games_appid.idx --query consultas/games_simples.txt \
    --output exemplos/saidas/games_simples.csv
./trabalho --mode search --reader games --input dados/games.csv \
    --index indices/games_appid.idx --index indices/games_name.idx \
    --query consultas/games_complexa.txt \
    --output exemplos/saidas/games_complexa.csv
