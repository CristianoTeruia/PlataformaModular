#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Plota os dados de um CSV exportado pela Plataforma Embarcada Modular.
Uso:
    python plotar_dados_sensores.py
    python plotar_dados_sensores.py "dados_sensores_20261025_005923.csv"
Dependência:
    python -m pip install matplotlib
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


TIME_COLUMN_CANDIDATES = ("timestamp", "datetime", "data_hora", "datahora", "time")
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
}


def escolher_csv():
    try:
        import tkinter as tk
        from tkinter import filedialog
        root = tk.Tk()
        root.withdraw()
        root.attributes("-topmost", True)
        nome = filedialog.askopenfilename(
            title="Selecione o CSV dos sensores",
            filetypes=[("Arquivos CSV", "*.csv"), ("Todos os arquivos", "*.*")]
        )
        root.destroy()
        return Path(nome) if nome else None
    except Exception:
        return None


def ler_csv(caminho):
    # utf-8-sig remove eventual BOM; DictReader mantém os nomes do cabeçalho.
    with caminho.open("r", newline="", encoding="utf-8-sig") as f:
        amostra = f.read(4096)
        f.seek(0)
        try:
            dialect = csv.Sniffer().sniff(amostra, delimiters=",;\\t")
        except csv.Error:
            dialect = csv.excel
        reader = csv.DictReader(f, dialect=dialect)
        if not reader.fieldnames:
            raise ValueError("O arquivo não tem cabeçalho.")
        campos = [c.strip() for c in reader.fieldnames]
        rows = []
        for row in reader:
            if not row:
                continue
            rows.append({(k.strip() if k else k): (v.strip() if isinstance(v, str) else v)
                         for k, v in row.items()})
    return campos, rows


def converter_numero(valor):
    if valor is None:
        return None
    s = str(valor).strip()
    if not s:
        return None
    # Aceita decimal com vírgula quando não há ponto decimal.
    if "," in s and "." not in s:
        s = s.replace(",", ".")
    try:
        return float(s)
    except ValueError:
        return None


def converter_data(valor):
    s = str(valor or "").strip()
    if not s:
        return None
    formatos = (
        "%Y-%m-%d %H:%M:%S",
        "%Y-%m-%d %H:%M:%S.%f",
        "%d/%m/%Y %H:%M:%S",
        "%d/%m/%Y %H:%M",
        "%Y-%m-%dT%H:%M:%S",
        "%Y-%m-%dT%H:%M:%S.%f",
    )
    for fmt in formatos:
        try:
            return datetime.strptime(s, fmt)
        except ValueError:
            pass
    try:
        return datetime.fromisoformat(s)
    except ValueError:
        return None


def main():
    caminho = Path(sys.argv[1]).expanduser() if len(sys.argv) > 1 else escolher_csv()
    if not caminho:
        print("Nenhum arquivo selecionado.")
        return
    if not caminho.is_file():
        print(f"Arquivo não encontrado: {caminho}")
        input("Pressione Enter para sair...")
        return

    try:
        campos, rows = ler_csv(caminho)
    except Exception as exc:
        print(f"Não foi possível ler o CSV: {exc}")
        input("Pressione Enter para sair...")
        return

    time_col = next((c for c in campos if c.lower() in TIME_COLUMN_CANDIDATES), None)
    if time_col is None:
        print("Não encontrei a coluna de data/hora (por exemplo, 'timestamp').")
        print("Colunas encontradas:", ", ".join(campos))
        input("Pressione Enter para sair...")
        return

    # Cada linha só é mantida se tiver uma data/hora válida.
    datas = []
    valid_rows = []
    for row in rows:
        dt = converter_data(row.get(time_col))
        if dt is not None:
            datas.append(dt)
            valid_rows.append(row)
    if not valid_rows:
        print("Não encontrei datas/horários válidos no CSV.")
        input("Pressione Enter para sair...")
        return

    series = []
    for col in campos:
        if col.lower() == time_col.lower() or col.lower() in IGNORE_COLUMNS:
            continue
        values = [converter_numero(row.get(col)) for row in valid_rows]
        if any(v is not None for v in values):
            series.append((col, values))

    if not series:
        print("Não encontrei colunas numéricas para plotar.")
        input("Pressione Enter para sair...")
        return

    # Separadores verticais pontilhados nos limites entre dias.
    day_changes = []
    previous_day = datas[0].date()
    for i, dt in enumerate(datas[1:], start=1):
        if dt.date() != previous_day:
            day_changes.append((i, dt.date()))
            previous_day = dt.date()

    n = len(series)
    fig, axes = plt.subplots(
        nrows=n, ncols=1, sharex=True,
        figsize=(13, max(3.0, 2.35 * n)),
        constrained_layout=True
    )
    if n == 1:
        axes = [axes]

    cmap = plt.get_cmap("tab10")
    for idx, (ax, item) in enumerate(zip(axes, series)):
        col, values = item
        label, ylabel = LABELS.get(col, (col.replace("_", " ").capitalize(), col))
        color = cmap(idx % 10)
        # NaN cria lacunas onde o sensor não forneceu valor, em vez de inventar dados.
        plotted = [float("nan") if v is None else v for v in values]
        ax.plot(datas, plotted, color=color, linewidth=1.15, marker=None, label=label)
        ax.set_ylabel(ylabel, color=color, fontsize=9)
        ax.tick_params(axis="y", labelcolor=color, labelsize=8)
        ax.grid(True, axis="y", linestyle=":", alpha=0.45)
        ax.set_title(label, loc="left", fontsize=10, pad=3)
        for index, day in day_changes:
            ax.axvline(datas[index], color="gray", linestyle="--", linewidth=1.0, alpha=0.8)
        # Pequeno espaço visual para não colar a linha nas bordas.
        ax.margins(x=0.01)

    axes[-1].xaxis.set_major_formatter(mdates.DateFormatter("%d/%m %H:%M"))
    axes[-1].xaxis.set_major_locator(mdates.AutoDateLocator())
    plt.setp(axes[-1].get_xticklabels(), rotation=35, ha="right", fontsize=8)
    fig.suptitle(f"Leituras dos sensores — {caminho.name}", fontsize=13)

    if day_changes:
        dias = sorted({dt.date() for dt in datas})
        print("Dias encontrados:", ", ".join(d.strftime("%d/%m/%Y") for d in dias))
        print("Linhas pontilhadas marcam a mudança de dia.")
    else:
        print("Os dados têm apenas um dia; não foi necessário separar dias.")

    print(f"Arquivo: {caminho}")
    print(f"Leituras com data válida: {len(valid_rows)}")
    print(f"Variáveis plotadas: {len(series)}")
    print("Feche a janela do gráfico para encerrar.")
    plt.show()


if __name__ == "__main__":
    main()
