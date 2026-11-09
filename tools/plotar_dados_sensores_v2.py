#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Gera gráficos agrupados para o CSV da Plataforma Embarcada Modular.

Os gráficos seguem os dados do CSV: só é criado o gráfico de um sensor se ele
tiver pelo menos um valor no arquivo (AHT20, BMP280, MPU6500 e BH1750). Sensores
sem dados não geram janela nem painel vazio.

Uso:
    python plotar_dados_sensores_v2.py
    python plotar_dados_sensores_v2.py "dados_sensores.csv"
Dependência: python -m pip install matplotlib
"""
import csv
import sys
from pathlib import Path
from datetime import datetime

try:
    import matplotlib.pyplot as plt
    import matplotlib.dates as mdates
except ImportError:
    print("A biblioteca matplotlib não está instalada.")
    print("Instale com: python -m pip install matplotlib")
    input("Pressione Enter para sair...")
    raise SystemExit(1)

TIME_COLUMNS = {"timestamp", "datetime", "data_hora", "datahora", "time"}
IGNORE_COLUMNS = {"sensor_mask", "aht_present", "bmp_present", "mpu_present"}
LABELS = {
    "aht_temp_c": ("Temperatura AHT20", "Temperatura (°C)"),
    "aht_humidity_pct": ("Umidade AHT20", "Umidade (%)"),
    "bmp_pressure": ("Pressão atmosférica BMP280", "Pressão (unidade do CSV)"),
    "accel_x_g": ("Aceleração X", "Aceleração (g)"),
    "accel_y_g": ("Aceleração Y", "Aceleração (g)"),
    "accel_z_g": ("Aceleração Z", "Aceleração (g)"),
    "gyro_x_dps": ("Giroscópio X", "Velocidade angular (°/s)"),
    "gyro_y_dps": ("Giroscópio Y", "Velocidade angular (°/s)"),
    "gyro_z_dps": ("Giroscópio Z", "Velocidade angular (°/s)"),
    "bh_lux": ("Luminosidade BH1750", "Iluminância (lux)"),
}

# Colunas de cada sensor, na ordem em que o firmware as escreve.
SENSOR_COLUMNS = (
    ("AHT20", ("aht_temp_c", "aht_humidity_pct")),
    ("BMP280", ("bmp_pressure",)),
    ("MPU6500", ("accel_x_g", "accel_y_g", "accel_z_g", "gyro_x_dps", "gyro_y_dps", "gyro_z_dps")),
    ("BH1750", ("bh_lux",)),
)


def escolher_csv():
    try:
        import tkinter as tk
        from tkinter import filedialog
        root = tk.Tk()
        root.withdraw()
        root.attributes("-topmost", True)
        nome = filedialog.askopenfilename(
            title="Selecione o CSV dos sensores",
            filetypes=[("Arquivos CSV", "*.csv"), ("Todos os arquivos", "*.*")],
        )
        root.destroy()
        return Path(nome) if nome else None
    except Exception:
        return None


def ler_csv(path):
    with path.open("r", newline="", encoding="utf-8-sig") as f:
        sample = f.read(4096)
        f.seek(0)
        try:
            dialect = csv.Sniffer().sniff(sample, delimiters=",;\t")
        except csv.Error:
            dialect = csv.excel
        reader = csv.DictReader(f, dialect=dialect)
        if not reader.fieldnames:
            raise ValueError("O arquivo não tem cabeçalho.")
        fields = [name.strip() for name in reader.fieldnames]
        rows = []
        for row in reader:
            if row:
                rows.append({(k.strip() if k else k): (v.strip() if isinstance(v, str) else v)
                             for k, v in row.items()})
    return fields, rows


def number(value):
    if value is None or not str(value).strip():
        return None
    s = str(value).strip()
    if "," in s and "." not in s:
        s = s.replace(",", ".")
    try:
        return float(s)
    except ValueError:
        return None


def parse_datetime(value):
    s = str(value or "").strip()
    for fmt in ("%Y-%m-%d %H:%M:%S", "%Y-%m-%d %H:%M:%S.%f",
                "%d/%m/%Y %H:%M:%S", "%d/%m/%Y %H:%M",
                "%Y-%m-%dT%H:%M:%S", "%Y-%m-%dT%H:%M:%S.%f"):
        try:
            return datetime.strptime(s, fmt)
        except ValueError:
            pass
    try:
        return datetime.fromisoformat(s)
    except ValueError:
        return None


def setup_time_axis(ax, dates, day_changes):
    # No máximo 7 marcas para evitar que os horários se sobreponham.
    locator = mdates.AutoDateLocator(minticks=4, maxticks=7)
    ax.xaxis.set_major_locator(locator)
    ax.xaxis.set_major_formatter(mdates.DateFormatter("%d/%m\n%H:%M"))
    ax.tick_params(axis="x", labelsize=9, pad=6)
    ax.set_xlabel("Data (dia/mês) e horário (hora:minuto)")
    for index, _day in day_changes:
        ax.axvline(dates[index], color="gray", linestyle=":", linewidth=1.2, alpha=0.9, zorder=0)
    ax.margins(x=0.015)


def plot_series(ax, dates, values, **kwargs):
    """Traça a série; None vira lacuna (NaN) em vez de dado inventado.

    Pontos isolados (vizinhos sem valor) não formam linha, então recebem um
    marcador pequeno para continuarem visíveis, por exemplo quando um sensor
    foi ligado só por alguns registros.
    """
    y = [float("nan") if v is None else v for v in values]
    (line,) = ax.plot(dates, y, linewidth=1.15, **kwargs)
    iso_x, iso_y = [], []
    for k, v in enumerate(values):
        if v is None:
            continue
        antes = k > 0 and values[k - 1] is not None
        depois = k + 1 < len(values) and values[k + 1] is not None
        if not antes and not depois:
            iso_x.append(dates[k])
            iso_y.append(v)
    if iso_x:
        ax.plot(iso_x, iso_y, linestyle="none", marker="o", markersize=3.5,
                color=line.get_color())
    return line


def draw_lines(ax, dates, values_by_name, colors, ylabel, title):
    for name, values in values_by_name:
        label, _unit = LABELS.get(name, (name.replace("_", " ").capitalize(), name))
        plot_series(ax, dates, values, label=label, color=colors[name])
    ax.set_ylabel(ylabel)
    ax.set_title(title, loc="left", fontsize=11, pad=5)
    ax.grid(True, axis="y", linestyle=":", alpha=0.5)
    ax.legend(loc="best", fontsize=9, ncol=3)


def draw_temp_humidity(ax, dates, series, colors):
    """Temperatura e/ou umidade do AHT20, conforme o que existir no CSV."""
    has_t = "aht_temp_c" in series
    has_h = "aht_humidity_pct" in series
    if has_t:
        plot_series(ax, dates, series["aht_temp_c"], color=colors["aht_temp_c"],
                    label="Temperatura AHT20 (°C)")
        ax.set_ylabel("Temperatura (°C)", color=colors["aht_temp_c"])
        ax.tick_params(axis="y", labelcolor=colors["aht_temp_c"])
    if has_h:
        # Com temperatura presente, a umidade usa o eixo Y da direita;
        # sozinha, usa o eixo principal.
        ax_h = ax.twinx() if has_t else ax
        plot_series(ax_h, dates, series["aht_humidity_pct"], color=colors["aht_humidity_pct"],
                    label="Umidade AHT20 (%)")
        ax_h.set_ylabel("Umidade (%)", color=colors["aht_humidity_pct"])
        ax_h.tick_params(axis="y", labelcolor=colors["aht_humidity_pct"])
        handles, labels = ax.get_legend_handles_labels()
        if has_t:
            h2, l2 = ax_h.get_legend_handles_labels()
            handles, labels = handles + h2, labels + l2
        ax.legend(handles, labels, loc="best", fontsize=9)
    else:
        ax.legend(loc="best", fontsize=9)
    ax.set_title("Temperatura e umidade" if has_t and has_h
                 else "Temperatura AHT20" if has_t else "Umidade AHT20",
                 loc="left", fontsize=11)
    ax.grid(True, axis="y", linestyle=":", alpha=0.5)


def main():
    path = Path(sys.argv[1]).expanduser() if len(sys.argv) > 1 else escolher_csv()
    if not path:
        print("Nenhum arquivo selecionado.")
        return
    if not path.is_file():
        print(f"Arquivo não encontrado: {path}")
        input("Pressione Enter para sair...")
        return
    try:
        fields, rows = ler_csv(path)
    except Exception as exc:
        print(f"Não foi possível ler o CSV: {exc}")
        input("Pressione Enter para sair...")
        return

    time_col = next((c for c in fields if c.lower() in TIME_COLUMNS), None)
    if not time_col:
        print("Não encontrei a coluna de data/hora (ex.: timestamp).")
        print("Colunas encontradas:", ", ".join(fields))
        input("Pressione Enter para sair...")
        return

    dates, valid_rows = [], []
    for row in rows:
        dt = parse_datetime(row.get(time_col))
        if dt is not None:
            dates.append(dt)
            valid_rows.append(row)
    if not dates:
        print("Não encontrei datas/horários válidos no CSV.")
        input("Pressione Enter para sair...")
        return

    series = {}
    for col in fields:
        if col.lower() == time_col.lower() or col.lower() in IGNORE_COLUMNS:
            continue
        vals = [number(row.get(col)) for row in valid_rows]
        if any(v is not None for v in vals):
            series[col] = vals

    # Separadores verticais pontilhados quando a data muda.
    day_changes = []
    previous = dates[0].date()
    for i, dt in enumerate(dates[1:], start=1):
        if dt.date() != previous:
            day_changes.append((i, dt.date()))
            previous = dt.date()

    colors = {
        "aht_temp_c": "tab:red", "aht_humidity_pct": "tab:blue", "bmp_pressure": "tab:green",
        "accel_x_g": "tab:red", "accel_y_g": "tab:green", "accel_z_g": "tab:blue",
        "gyro_x_dps": "tab:red", "gyro_y_dps": "tab:green", "gyro_z_dps": "tab:blue",
        "bh_lux": "tab:orange",
    }
    for name in series:
        colors.setdefault(name, f"C{len(colors) % 10}")

    plotted = set()   # colunas que ganharam gráfico
    windows = []      # descrição das janelas abertas, para o resumo

    # Janela 1: ambiente (AHT20 e BMP280). Só entram os painéis que têm dados:
    # temperatura/umidade (eixos Y distintos quando ambas existem) e pressão.
    th_present = [c for c in ("aht_temp_c", "aht_humidity_pct") if c in series]
    p_present = "bmp_pressure" in series
    panels = []
    if th_present:
        panels.append("th")
    if p_present:
        panels.append("p")
    if panels:
        fig, axes = plt.subplots(len(panels), 1, sharex=True,
                                 figsize=(15, 8 if len(panels) == 2 else 5), squeeze=False)
        axes = list(axes[:, 0])
        for ax, kind in zip(axes, panels):
            if kind == "th":
                draw_temp_humidity(ax, dates, series, colors)
            else:
                draw_lines(ax, dates, [("bmp_pressure", series["bmp_pressure"])], colors,
                           "Pressão (unidade do CSV)", "Pressão atmosférica BMP280")
            setup_time_axis(ax, dates, day_changes)
        fig.suptitle(f"Sensores ambientais — {path.name}", fontsize=13)
        fig.tight_layout(rect=(0, 0, 1, 0.96))
        plotted.update(th_present)
        if p_present:
            plotted.add("bmp_pressure")
        windows.append("ambiente (AHT20/BMP280)")

    # Janelas 2 e 3: MPU6500, três eixos por grandeza com unidades compatíveis.
    for title, cols, ylabel in (
        ("Aceleração — MPU6500", ("accel_x_g", "accel_y_g", "accel_z_g"), "Aceleração (g)"),
        ("Velocidade angular — MPU6500", ("gyro_x_dps", "gyro_y_dps", "gyro_z_dps"), "Velocidade angular (°/s)"),
    ):
        present = [(c, series[c]) for c in cols if c in series]
        if not present:
            continue
        fig, ax = plt.subplots(figsize=(15, 5.5))
        draw_lines(ax, dates, present, colors, ylabel, title)
        setup_time_axis(ax, dates, day_changes)
        fig.suptitle(f"{title} — {path.name}", fontsize=13)
        fig.tight_layout(rect=(0, 0, 1, 0.94))
        plotted.update(c for c, _ in present)
        windows.append(title.split(" — ")[0].lower() + " (MPU6500)")

    # Janela 4: iluminância do BH1750, somente se houver leituras.
    if "bh_lux" in series:
        fig, ax = plt.subplots(figsize=(15, 5.5))
        draw_lines(ax, dates, [("bh_lux", series["bh_lux"])], colors,
                   "Iluminância (lux)", "Luminosidade — BH1750")
        setup_time_axis(ax, dates, day_changes)
        fig.suptitle(f"Luminosidade — BH1750 — {path.name}", fontsize=13)
        fig.tight_layout(rect=(0, 0, 1, 0.94))
        plotted.add("bh_lux")
        windows.append("luminosidade (BH1750)")

    if day_changes:
        days = sorted({dt.date() for dt in dates})
        print("Dias encontrados:", ", ".join(d.strftime("%d/%m/%Y") for d in days))
        print("Linhas verticais pontilhadas indicam a mudança de dia.")
    else:
        print("Os dados têm apenas um dia; não foi necessário separar dias.")
    print(f"Arquivo: {path}")
    print(f"Leituras com data válida: {len(valid_rows)}")
    print("Sensores no CSV:")
    for sensor, cols in SENSOR_COLUMNS:
        n = sum(1 for i in range(len(valid_rows))
                if any(c in series and series[c][i] is not None for c in cols))
        print(f"  {sensor}: {f'{n} leituras' if n else 'sem dados (gráfico omitido)'}")
    nao_plotadas = [c for c in series if c not in plotted]
    if nao_plotadas:
        print("Colunas com dados e sem gráfico neste script:", ", ".join(nao_plotadas))
    if not windows:
        print("Nenhum sensor com dados para plotar.")
        input("Pressione Enter para sair...")
        return
    print("Janelas abertas:", "; ".join(windows) + ".")
    print("No eixo horizontal, cada marca mostra dia/mês e, abaixo, hora:minuto.")
    print("Feche as janelas dos gráficos para encerrar.")
    plt.show()

if __name__ == "__main__":
    main()
