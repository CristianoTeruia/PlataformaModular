#!/usr/bin/env python3
"""Verifica a integridade de um CSV exportado do datalogger e gera um relatorio de texto (evidencia).

Uso:
  python tools/verificar_csv.py arquivo.csv
  python tools/verificar_csv.py arquivo.csv --esperado 4360     # quantidade informada pelo STM32 (EXPORT_END)
  python tools/verificar_csv.py arquivo.csv --periodo 10 --tolerancia 2

Verifica: contagem, mascaras de sensores, ordem e duplicatas de horario, intervalos entre amostras (lacunas),
datas suspeitas (ano 2000 = RTC nao ajustado), coerencia entre mascara e campos, valores zerados e fora de faixa.
Os limites de faixa estao no dicionario FAIXAS abaixo e sao impressos no relatorio; ajuste conforme os datasheets.
"""
import argparse
import csv
import statistics
import sys
from datetime import datetime
from pathlib import Path

# Colunas de cada sensor (bit da mascara). Deve acompanhar Core/Src/sensor_registry.c.
SENSORES = {
    0: ("AHT", ["aht_present", "aht_temp_c", "aht_humidity_pct"]),
    1: ("BMP280", ["bmp_pressure"]),
    2: ("MPU6500", ["accel_x_g", "accel_y_g", "accel_z_g", "gyro_x_dps", "gyro_y_dps", "gyro_z_dps"]),
    3: ("BH1750", ["bh_lux"]),
    4: ("ENS160", ["ens_aqi", "ens_tvoc_ppb", "ens_eco2_ppm", "ens_validity"]),
}
# Faixas para "fora do esperado". Fontes: faixa de operacao dos sensores (AHT20: -40 a 85 C, 0 a 100 %;
# BMP280: 300 a 1100 hPa), faixas configuradas no firmware (MPU6500: +-2 g e +-250 dps) e a conversao do
# BH1750 (contagem de 16 bits / 1,2). EDITE se necessario.
FAIXAS = {
    "aht_temp_c": (-40.0, 85.0), "aht_humidity_pct": (0.0, 100.0), "bmp_pressure": (30000.0, 110000.0),
    "accel_x_g": (-2.0, 2.0), "accel_y_g": (-2.0, 2.0), "accel_z_g": (-2.0, 2.0),
    "gyro_x_dps": (-250.0, 250.0), "gyro_y_dps": (-250.0, 250.0), "gyro_z_dps": (-250.0, 250.0),
    "bh_lux": (0.0, 54612.5),
}
# Grupos cujos valores TODOS iguais a zero indicam leitura invalida (nao inclui BH1750: escuro = 0 lux e valido).
GRUPOS_ZERO = {
    "AHT": ["aht_temp_c", "aht_humidity_pct"],
    "BMP280": ["bmp_pressure"],
    "MPU6500": ["accel_x_g", "accel_y_g", "accel_z_g", "gyro_x_dps", "gyro_y_dps", "gyro_z_dps"],
}


def numero(texto):
    try:
        return float(texto)
    except (TypeError, ValueError):
        return None


def verificar(caminho, periodo, tolerancia, esperado):
    linhas = []
    def out(texto=""):
        linhas.append(texto)
        print(texto)

    with open(caminho, "r", encoding="utf-8-sig", newline="") as f:
        leitor = csv.DictReader(f)
        colunas = leitor.fieldnames or []
        registros = list(leitor)
    avisos = []
    out(f"Arquivo: {caminho}")
    out(f"Gerado em: {datetime.now():%Y-%m-%d %H:%M:%S}")
    out(f"Colunas ({len(colunas)}): {', '.join(colunas)}")
    n = len(registros)
    out(f"Registros de dados: {n}")
    if n == 0:
        out("ATENCAO: arquivo sem registros.")
        return linhas, ["arquivo sem registros"]
    if esperado is not None:
        ok = (n == esperado)
        out(f"Quantidade informada pelo STM32: {esperado} -> {'confere' if ok else 'DIVERGE'}")
        if not ok:
            avisos.append(f"quantidade diverge da informada pelo STM32 ({n} x {esperado})")

    # --- mascaras ---
    out("\n[Mascara de sensores]")
    contagem = {}
    for r in registros:
        contagem[r["sensor_mask"]] = contagem.get(r["sensor_mask"], 0) + 1
    for m, c in sorted(contagem.items(), key=lambda kv: -kv[1]):
        try:
            v = int(m)
            nomes = [SENSORES[b][0] for b in SENSORES if v & (1 << b)]
        except ValueError:
            nomes = ["?"]
        out(f"  mascara {m:>3}: {c:>6} registros ({', '.join(nomes) or 'nenhum'})")

    # --- coerencia mascara x campos ---
    incoerentes = 0
    for r in registros:
        try:
            v = int(r["sensor_mask"])
        except ValueError:
            incoerentes += 1
            continue
        for bit, (nome, cols) in SENSORES.items():
            campos = [c for c in cols if c in r and c != "aht_present"]
            if not campos:
                continue
            tem_dado = any((r[c] or "").strip() != "" for c in campos)
            if bool(v & (1 << bit)) != tem_dado:
                incoerentes += 1
                break
    out(f"Registros com mascara incoerente com os campos preenchidos: {incoerentes}")
    if incoerentes:
        avisos.append(f"{incoerentes} registros com mascara incoerente")

    # --- horarios ---
    out("\n[Horarios]")
    tempos, invalidos = [], 0
    for r in registros:
        try:
            tempos.append(datetime.strptime(r["timestamp"].strip(), "%Y-%m-%d %H:%M:%S"))
        except (ValueError, KeyError, AttributeError):
            tempos.append(None)
            invalidos += 1
    out(f"Horarios ilegiveis: {invalidos}")
    if invalidos:
        avisos.append(f"{invalidos} horarios ilegiveis")
    validos = [t for t in tempos if t is not None]
    if validos:
        out(f"Primeiro: {validos[0]}   Ultimo: {validos[-1]}   Duracao: {validos[-1] - validos[0]}")
        ano2000 = sum(1 for t in validos if t.year == 2000)
        out(f"Registros com ano 2000 (data padrao do firmware = RTC nao ajustado): {ano2000}")
        if ano2000:
            avisos.append(f"{ano2000} registros com ano 2000 (RTC nao ajustado)")
    diffs, fora_ordem, duplicados = [], 0, 0
    ant = None
    for t in tempos:
        if t is None:
            continue
        if ant is not None:
            d = (t - ant).total_seconds()
            if d < 0:
                fora_ordem += 1
            elif d == 0:
                duplicados += 1
            diffs.append(d)
        ant = t
    out(f"Horario retrocedeu (fora de ordem ou RTC reajustado para tras): {fora_ordem}")
    out(f"Horarios repetidos em sequencia (diferenca 0 s): {duplicados}")
    linhas_iguais = len(registros) - len({tuple(r.values()) for r in registros})
    out(f"Linhas totalmente identicas (duplicatas exatas): {linhas_iguais}")
    if fora_ordem:
        avisos.append(f"{fora_ordem} retrocessos de horario")
    if duplicados or linhas_iguais:
        avisos.append(f"{duplicados} horarios repetidos / {linhas_iguais} linhas duplicadas")
    normais = [d for d in diffs if abs(d - periodo) <= tolerancia]
    curtos = [d for d in diffs if 0 <= d < periodo - tolerancia]
    SALTO = 86400.0  # acima de 24 h nao e lacuna de amostragem: e salto do relogio (RTC ajustado/perdido)
    lacunas = [(i, d) for i, d in enumerate(diffs) if periodo + tolerancia < d <= SALTO]
    saltos = [(i, d) for i, d in enumerate(diffs) if d > SALTO]
    out(f"Periodo esperado: {periodo} s (+- {tolerancia} s)")
    out(f"Intervalos dentro do esperado: {len(normais)} de {len(diffs)}")
    out(f"Intervalos curtos (< periodo): {len(curtos)}")
    out(f"Lacunas (> periodo, ate 24 h): {len(lacunas)}")
    out(f"Saltos de relogio (> 24 h entre registros seguidos): {len(saltos)}")
    if saltos:
        avisos.append(f"{len(saltos)} saltos de relogio (> 24 h) entre registros seguidos")
    if lacunas:
        faltantes = sum(max(0, round(d / periodo) - 1) for _, d in lacunas)
        out(f"Amostras estimadas ausentes nas lacunas: {faltantes}")
        out("Maiores lacunas:")
        validos_idx = [i for i, t in enumerate(tempos) if t is not None]
        for i, d in sorted(lacunas, key=lambda x: -x[1])[:5]:
            t0 = tempos[validos_idx[i]]
            out(f"  apos {t0}: {d:.0f} s (~{max(0, round(d / periodo) - 1)} amostras)")
        avisos.append(f"{len(lacunas)} lacunas, ~{faltantes} amostras ausentes")
    serie_continua = bool(diffs) and bool(validos) and fora_ordem == 0 and not saltos
    if not serie_continua and diffs:
        out("Estatisticas de duracao e intervalo medio nao calculadas: o horario retrocedeu ou saltou.")
    if serie_continua:
        dur = (validos[-1] - validos[0]).total_seconds()
        out(f"Registros esperados pela duracao ({dur:.0f} s / {periodo} s + 1): {int(dur // periodo) + 1} | encontrados: {len(validos)}")
        if len(normais) >= 3:
            out(f"Mediana dos intervalos: {statistics.median(diffs):.1f} s (o RTC tem resolucao de 1 s)")
        if len(validos) >= 2 and dur > 0:
            medio = dur / (len(validos) - 1)
            sem_lacunas = [d for d in diffs if 0 < d <= periodo + tolerancia]
            out(f"Intervalo medio global (duracao / (n-1)): {medio:.3f} s ({(medio / periodo - 1) * 100:+.2f} % do periodo nominal)")
            if sem_lacunas:
                m2 = sum(sem_lacunas) / len(sem_lacunas)
                out(f"Intervalo medio sem as lacunas: {m2:.3f} s ({(m2 / periodo - 1) * 100:+.2f} %)")

    # --- valores ---
    out("\n[Valores]")
    out("Faixas usadas (edite em FAIXAS): " + "; ".join(f"{k} {v[0]:g}..{v[1]:g}" for k, v in FAIXAS.items()))
    for col in colunas:
        if col in ("sensor_mask", "timestamp", "aht_present"):
            continue
        valores = [numero(r.get(col)) for r in registros]
        presentes = [v for v in valores if v is not None]
        vazios = len(valores) - len(presentes)
        if not presentes:
            out(f"  {col:<18} sem dados (vazio em {vazios})")
            continue
        zeros = sum(1 for v in presentes if v == 0)
        txt = (f"  {col:<18} n={len(presentes):>6} vazios={vazios:>6} min={min(presentes):>10.3f} "
               f"max={max(presentes):>10.3f} media={statistics.fmean(presentes):>10.3f} zeros={zeros}")
        if col in FAIXAS:
            lo, hi = FAIXAS[col]
            fora = sum(1 for v in presentes if v < lo or v > hi)
            txt += f" fora_da_faixa={fora}"
            if fora:
                avisos.append(f"{col}: {fora} valores fora da faixa {lo:g}..{hi:g}")
        out(txt)
    for nome, cols in GRUPOS_ZERO.items():
        if not all(c in colunas for c in cols):
            continue
        z = 0
        for r in registros:
            vs = [numero(r.get(c)) for c in cols]
            if all(v is not None for v in vs) and all(v == 0 for v in vs):
                z += 1
        out(f"Registros com TODAS as colunas do {nome} em zero: {z}")
        if z:
            avisos.append(f"{z} registros do {nome} com todas as colunas em zero")

    out("\n[Resumo]")
    if avisos:
        out("ATENCAO - pontos a investigar:")
        for a in avisos:
            out(f"  - {a}")
    else:
        out("Nenhum problema encontrado pelas verificacoes acima.")
    out("Observacao: este relatorio verifica a coerencia do CSV; nao prova que os valores correspondem a grandezas fisicas reais.")
    return linhas, avisos


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv")
    ap.add_argument("--periodo", type=float, default=10.0, help="periodo de amostragem em s (padrao 10)")
    ap.add_argument("--tolerancia", type=float, default=2.0, help="tolerancia do periodo em s (padrao 2)")
    ap.add_argument("--esperado", type=int, help="quantidade de registros informada pelo STM32 (EXPORT_END)")
    ap.add_argument("--saida", help="arquivo do relatorio (padrao: <csv>_verificacao.txt)")
    a = ap.parse_args()
    linhas, avisos = verificar(a.csv, a.periodo, a.tolerancia, a.esperado)
    destino = Path(a.saida) if a.saida else Path(a.csv).with_name(Path(a.csv).stem + "_verificacao.txt")
    destino.write_text("\n".join(linhas) + "\n", encoding="utf-8")
    print(f"\nRelatorio salvo em: {destino}")
    sys.exit(1 if avisos else 0)


if __name__ == "__main__":
    main()
