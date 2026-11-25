#!/usr/bin/env python3
"""Resume medições, calcula estatísticas pareadas e cria gráficos opcionais."""
import argparse
import csv
import math
import statistics
import html
import shutil
import subprocess
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TREES = ("abb", "avl", "rubro_negra")
BUILD_METRICS = ("BUILD_MS", "INSERT_COMPARISONS", "ROTATIONS", "RECOLORS", "TREE_HEIGHT")
SEARCH_METRICS = ("SEARCH_MS", "SEARCH_COMPARISONS", "LOAD_INDEX_MS",
                  "FETCH_MS", "WRITE_MS", "TOTAL_MS")


def latex_escape(value):
    text = str(value)
    replacements = {
        "\\": r"\textbackslash{}", "&": r"\&", "%": r"\%",
        "$": r"\$", "#": r"\#", "_": r"\_\allowbreak{}", "{": r"\{",
        "}": r"\}",
    }
    for character, replacement in replacements.items():
        text = text.replace(character, replacement)
    return text


def load_csv(path):
    if not path.is_file():
        raise SystemExit(f"Dados ausentes: {path}. Execute experimentos/executar.py primeiro.")
    with path.open(newline="", encoding="utf-8") as source:
        return list(csv.DictReader(source))


def number(row, field):
    try:
        return float(row[field])
    except (KeyError, TypeError, ValueError):
        return None


def mean_sd(values):
    average = statistics.mean(values)
    deviation = statistics.stdev(values) if len(values) > 1 else 0.0
    return average, deviation


def save_plot(plt, path):
    plt.savefig(path.with_suffix(".png"))
    plt.savefig(path.with_suffix(".pdf"))


def grouped_summary(rows, keys, metrics):
    groups = defaultdict(list)
    for row in rows:
        group = tuple(row[key] for key in keys)
        for metric in metrics:
            value = number(row, metric)
            if value is not None:
                groups[group + (metric,)].append(value)
    result = []
    for group_metric, values in sorted(groups.items()):
        average, deviation = mean_sd(values)
        row = dict(zip(keys, group_metric[:-1]))
        row.update({"metric": group_metric[-1], "mean": average,
                    "stddev": deviation, "runs": len(values)})
        result.append(row)
    by_config = defaultdict(dict)
    for row in result:
        config = tuple(row[key] for key in keys if key != "tree")
        by_config[(config, row["metric"])][row["tree"]] = row["mean"]
    for row in result:
        config = tuple(row[key] for key in keys if key != "tree")
        reference = by_config[(config, row["metric"])].get("abb")
        row["reference"] = "abb"
        row["diff_pct"] = "" if reference in (None, 0) else (row["mean"] - reference) / reference * 100
    return result


def paired_t_tests(rows):
    groups = defaultdict(dict)
    config_names = ("dataset", "size", "search_type", "query_count")
    for row in rows:
        config = tuple(row[name] for name in config_names)
        key = (row["tree"], int(row["repetition"]))
        value = number(row, "SEARCH_MS")
        if value is not None:
            groups[config][key] = value
    configs = sorted(groups)
    selected = sorted(set((configs[0], configs[-1]))) if len(configs) >= 2 else configs
    results = []
    critical = 2.262
    scipy_t = None
    try:
        from scipy.stats import t as scipy_t
    except ImportError:
        pass
    for config in selected:
        for first, second in (("abb", "avl"), ("avl", "rubro_negra")):
            reps = sorted(set(rep for tree, rep in groups[config] if tree == first) &
                          set(rep for tree, rep in groups[config] if tree == second))
            diffs = [groups[config][(first, rep)] - groups[config][(second, rep)]
                     for rep in reps]
            if len(diffs) < 2:
                continue
            average = statistics.mean(diffs)
            deviation = statistics.stdev(diffs)
            t_value = 0.0 if deviation == 0 and average == 0 else (
                math.copysign(math.inf, average) if deviation == 0 else
                average / (deviation / math.sqrt(len(diffs))))
            p_value = ""
            if scipy_t is not None:
                p_value = float(2 * scipy_t.sf(abs(t_value), len(diffs) - 1))
            results.append({"config": config, "pair": f"{first}-{second}",
                            "repetitions": len(diffs), "mean_difference_ms": average,
                            "sd_difference_ms": deviation, "t": t_value,
                            "critical_t_10": critical if len(diffs) == 10 else "NA",
                            "reject_h0": abs(t_value) > critical if len(diffs) == 10 else "NA",
                            "p_value": p_value})
    return results


def write_summary(path, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    fields = list(rows[0]) if rows else ["dataset", "size", "tree", "metric", "mean", "stddev", "runs"]
    with path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def write_t_tests(path, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as output:
        for row in rows:
            config = ", ".join(str(value) for value in row["config"])
            output.write(f"Configuração: {latex_escape(config)}\\\\\n")
            output.write(f"Par: {latex_escape(row['pair'])}; r={row['repetitions']}; t={row['t']:.4g}; ")
            output.write(f"t crítico={row['critical_t_10']}; rejeita H0={row['reject_h0']}\\\\\n")
            output.write(f"Diferença média={row['mean_difference_ms']:.6g} ms; ")
            output.write(f"desvio={row['sd_difference_ms']:.6g} ms")
            if row["p_value"] != "":
                output.write(f"; p={row['p_value']:.6g}")
            output.write("\n")


def latex_table(path, rows, metrics):
    with path.open("w", encoding="utf-8") as output:
        output.write("\\scriptsize\n\\begin{longtable}{p{4cm}p{1.4cm}p{2.6cm}rr}\n\\hline\n")
        output.write("Base/configuração & Árvore & Métrica & Média & Desvio\\\\\n\\hline\n\\endfirsthead\n")
        output.write("\\hline\nBase/configuração & Árvore & Métrica & Média & Desvio\\\\\n\\hline\n\\endhead\n")
        for row in rows:
            if row["metric"] not in metrics:
                continue
            label = (f"{row.get('dataset', '')}, {row.get('size', '')}, "
                     f"{row.get('key', '')}, {row.get('search_type', '')}, "
                     f"{row.get('query_count', '')}")
            output.write(f"{latex_escape(label)} & {latex_escape(row.get('tree', ''))} & "
                         f"{latex_escape(row['metric'])} & "
                         f"{row['mean']:.6g} & {row['stddev']:.6g}\\\\\n")
        output.write("\\hline\n\\end{longtable}\n\\normalsize\n")


def svg_plot(path, title, x_label, y_label, series):
    width, height = 1000, 620
    left, right, top, bottom = 100, 35, 75, 145
    points = [point for _, values in series for point in values]
    if not points:
        return
    x_values = [float(x) for x, _ in points]
    y_values = [float(y) for _, y in points]
    x_min, x_max = min(x_values), max(x_values)
    y_min, y_max = min(y_values), max(y_values)
    if x_min == x_max:
        x_min -= 1
        x_max += 1
    if y_min == y_max:
        y_min -= 1
        y_max += 1
    plot_width = width - left - right
    plot_height = height - top - bottom
    colors = ("#2457a7", "#d14a32", "#23834a", "#8b55a1", "#d18b16", "#198d9a")
    lines = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
             '<rect width="100%" height="100%" fill="white"/>',
             f'<text x="{width/2}" y="34" text-anchor="middle" font-family="sans-serif" font-size="20">{html.escape(title)}</text>',
             f'<path d="M{left},{top} V{height-bottom} H{width-right}" fill="none" stroke="#333"/>']
    for tick in range(6):
        ratio = tick / 5
        x = left + ratio * plot_width
        y = height - bottom - ratio * plot_height
        xv = x_min + ratio * (x_max - x_min)
        yv = y_min + ratio * (y_max - y_min)
        lines.append(f'<path d="M{x:.1f},{top} V{height-bottom}" stroke="#ddd"/>')
        lines.append(f'<path d="M{left},{y:.1f} H{width-right}" stroke="#ddd"/>')
        lines.append(f'<text x="{x:.1f}" y="{height-bottom+24}" text-anchor="middle" font-family="sans-serif" font-size="12">{xv:.4g}</text>')
        lines.append(f'<text x="{left-12}" y="{y+4:.1f}" text-anchor="end" font-family="sans-serif" font-size="12">{yv:.4g}</text>')
    lines.extend((f'<text x="{left+plot_width/2}" y="{height-35}" text-anchor="middle" font-family="sans-serif" font-size="15">{html.escape(x_label)}</text>',
                  f'<text transform="translate(25 {top+plot_height/2}) rotate(-90)" text-anchor="middle" font-family="sans-serif" font-size="15">{html.escape(y_label)}</text>'))
    for index, (label, values) in enumerate(series):
        color = colors[index % len(colors)]
        mapped = []
        for x_value, y_value in values:
            x = left + (float(x_value) - x_min) / (x_max - x_min) * plot_width
            y = height - bottom - (float(y_value) - y_min) / (y_max - y_min) * plot_height
            mapped.append((x, y))
        if len(mapped) > 1:
            path_data = "M" + " L".join(f"{x:.1f},{y:.1f}" for x, y in mapped)
            lines.append(f'<path d="{path_data}" fill="none" stroke="{color}" stroke-width="2"/>')
        for x, y in mapped:
            lines.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3" fill="{color}"/>')
        legend_x = left + (index % 3) * 285
        legend_y = height - 90 + (index // 3) * 20
        lines.append(f'<path d="M{legend_x},{legend_y} h24" stroke="{color}" stroke-width="3"/>')
        lines.append(f'<text x="{legend_x+30}" y="{legend_y+5}" font-family="sans-serif" font-size="12">{html.escape(label)}</text>')
    lines.append("</svg>")
    path.write_text("\n".join(lines), encoding="utf-8")
    converter = shutil.which("rsvg-convert")
    if converter:
        subprocess.run([converter, "-f", "pdf", "-o", str(path.with_suffix(".pdf")), str(path)],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def make_svg_plots(construction, searches, output_dir):
    output_dir.mkdir(parents=True, exist_ok=True)
    build_rows = grouped_summary(construction, ["dataset", "size", "tree", "key"], BUILD_METRICS)
    search_rows = grouped_summary(searches,
        ["dataset", "size", "search_type", "query_count", "tree"], SEARCH_METRICS)
    datasets = sorted({row["dataset"] for row in build_rows})
    for dataset in datasets:
        keys = sorted({row["key"] for row in build_rows if row["dataset"] == dataset})
        for key in keys:
            rows = [row for row in build_rows if row["dataset"] == dataset and row["key"] == key]
            series = []
            for tree in TREES:
                values = sorted((int(row["size"]), row["mean"]) for row in rows
                                if row["tree"] == tree and row["metric"] == "BUILD_MS")
                if values:
                    series.append((tree, values))
            svg_plot(output_dir / f"construcao_{dataset}_{key}.svg",
                     f"Construção — {dataset}, chave {key}", "Registros", "BUILD_MS (ms)", series)
            for metric in ("INSERT_COMPARISONS", "ROTATIONS", "RECOLORS", "TREE_HEIGHT"):
                series = []
                for tree in TREES:
                    values = []
                    for point in rows:
                        if point["tree"] != tree or point["metric"] != metric:
                            continue
                        build_ms = next(item["mean"] for item in rows
                            if item["tree"] == tree and item["size"] == point["size"]
                            and item["metric"] == "BUILD_MS")
                        values.append((build_ms, point["mean"]))
                    if values:
                        series.append((tree, sorted(values)))
                svg_plot(output_dir / f"operacoes_{metric.lower()}_{dataset}_{key}.svg",
                         f"{metric} e construção — {dataset}, chave {key}",
                         "BUILD_MS (ms)", metric, series)
    for dataset in sorted({row["dataset"] for row in search_rows}):
        for search_type in ("simple", "complex"):
            rows = [row for row in search_rows if row["dataset"] == dataset
                    and row["search_type"] == search_type and row["metric"] == "SEARCH_MS"]
            series = []
            for tree in TREES:
                values = sorted((int(row["query_count"]), row["mean"]) for row in rows
                                if row["tree"] == tree)
                if values:
                    series.append((tree, values))
            svg_plot(output_dir / f"busca_{search_type}_{dataset}.svg",
                     f"Busca {search_type} — {dataset}; média de 10 repetições",
                     "Chaves consultadas", "SEARCH_MS (ms)", series)
            for metric in ("LOAD_INDEX_MS", "FETCH_MS", "WRITE_MS", "TOTAL_MS"):
                metric_rows = [row for row in search_rows if row["dataset"] == dataset
                               and row["search_type"] == search_type and row["metric"] == metric]
                metric_series = []
                for tree in TREES:
                    for size in sorted({row["size"] for row in metric_rows}):
                        values = sorted((int(row["query_count"]), row["mean"])
                            for row in metric_rows if row["tree"] == tree and row["size"] == size)
                        if values:
                            metric_series.append((f"{tree}, N={size}", values))
                svg_plot(output_dir / f"custo_{metric.lower()}_{search_type}_{dataset}.svg",
                         f"{metric} — {search_type}, {dataset}; ms", "Chaves consultadas",
                         metric, metric_series)


def make_plots(construction, searches, output_dir):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        make_svg_plots(construction, searches, output_dir)
        print("matplotlib ausente; gráficos SVG foram gerados com a biblioteca padrão.")
        return
    output_dir.mkdir(parents=True, exist_ok=True)
    build_summary = grouped_summary(construction, ["dataset", "size", "tree", "key"],
                                    BUILD_METRICS)
    search_summary = grouped_summary(searches,
        ["dataset", "size", "search_type", "query_count", "tree"],
        ["SEARCH_MS", "LOAD_INDEX_MS", "FETCH_MS", "WRITE_MS", "TOTAL_MS"])
    for dataset in sorted({row["dataset"] for row in build_summary}):
        dataset_rows = [row for row in build_summary if row["dataset"] == dataset]
        for key in sorted({row["key"] for row in dataset_rows}):
            rows = [row for row in dataset_rows if row["key"] == key]
            for tree in TREES:
                values = sorted((int(row["size"]), row["mean"], row["stddev"])
                                for row in rows if row["tree"] == tree and
                                row["metric"] == "BUILD_MS")
                if values:
                    plt.errorbar([v[0] for v in values], [v[1] for v in values],
                                 yerr=[v[2] for v in values], marker="o", label=tree)
            plt.title(f"Construção — {dataset}, chave {key}; 10 repetições")
            plt.xlabel("Registros na base")
            plt.ylabel("BUILD_MS (ms)")
            plt.legend()
            plt.tight_layout()
            save_plot(plt, output_dir / f"construcao_{dataset}_{key}")
            plt.close()
            for metric in ("INSERT_COMPARISONS", "ROTATIONS", "RECOLORS", "TREE_HEIGHT"):
                for tree in TREES:
                    points = [row for row in rows if row["tree"] == tree and
                              row["metric"] == metric]
                    build_times = [next(item["mean"] for item in rows
                                        if item["tree"] == tree and
                                        item["size"] == point["size"] and
                                        item["metric"] == "BUILD_MS") for point in points]
                    if points:
                        plt.plot(build_times, [point["mean"] for point in points],
                                 marker="o", label=tree)
                plt.title(f"{metric} e construção — {dataset}, chave {key}")
                plt.xlabel("BUILD_MS (ms)")
                plt.ylabel(metric)
                plt.legend()
                plt.tight_layout()
                save_plot(plt, output_dir / f"operacoes_{metric.lower()}_{dataset}_{key}")
                plt.close()
    for search_type in ("simple", "complex"):
        for dataset in sorted({row["dataset"] for row in search_summary
                               if row["search_type"] == search_type}):
            rows = [row for row in search_summary if row["dataset"] == dataset and
                    row["search_type"] == search_type and row["metric"] == "SEARCH_MS"]
            for tree in TREES:
                values = sorted((int(row["query_count"]), row["mean"], row["stddev"])
                                for row in rows if row["tree"] == tree)
                if values:
                    plt.errorbar([v[0] for v in values], [v[1] for v in values],
                                 yerr=[v[2] for v in values], marker="o", label=tree)
            plt.title(f"Busca {search_type} — {dataset}; média por 10 repetições")
            plt.xlabel("Chaves consultadas")
            plt.ylabel("SEARCH_MS (ms)")
            plt.legend()
            plt.tight_layout()
            save_plot(plt, output_dir / f"busca_{search_type}_{dataset}")
            plt.close()
    for metric in ("LOAD_INDEX_MS", "FETCH_MS", "WRITE_MS", "TOTAL_MS"):
        rows = [row for row in search_summary if row["metric"] == metric]
        if not rows:
            continue
        labels = [f"{r['dataset']}:{r['search_type']}:{r['query_count']}:{r['tree']}" for r in rows]
        plt.bar(range(len(rows)), [r["mean"] for r in rows])
        plt.xticks(range(len(rows)), labels, rotation=90, fontsize=7)
        plt.title(f"Custo de arquivo e total — {metric}; milissegundos")
        plt.ylabel("Tempo (ms)")
        plt.tight_layout()
        save_plot(plt, output_dir / f"custo_{metric.lower()}")
        plt.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-dir", type=Path, default=ROOT / "experimentos/dados")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "experimentos/graficos")
    args = parser.parse_args()
    construction = load_csv(args.data_dir / "construcao.csv")
    searches = load_csv(args.data_dir / "busca.csv")
    build_summary = grouped_summary(construction,
        ["dataset", "size", "tree", "key"], BUILD_METRICS)
    search_summary = grouped_summary(searches,
        ["dataset", "size", "search_type", "query_count", "tree"], SEARCH_METRICS)
    write_summary(args.data_dir / "resumo_construcao.csv", build_summary)
    write_summary(args.data_dir / "resumo_busca.csv", search_summary)
    tests = paired_t_tests(searches)
    write_t_tests(args.data_dir / "testes_pareados.tex", tests)
    report_dir = ROOT / "relatorio"
    report_dir.mkdir(exist_ok=True)
    latex_table(report_dir / "tabela_construcao.tex", build_summary, BUILD_METRICS)
    latex_table(report_dir / "tabela_busca.tex", search_summary, SEARCH_METRICS)
    make_plots(construction, searches, args.output_dir)
    if not tests:
        print("Ainda não há configurações com pares completos suficientes para teste t.")
    else:
        print(f"Teste t pareado: {len(tests)} comparações salvas em {args.data_dir / 'testes_pareados.tex'}")


if __name__ == "__main__":
    main()
