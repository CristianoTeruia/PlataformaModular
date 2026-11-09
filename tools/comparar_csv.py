#!/usr/bin/env python3
"""Verifica se dois CSVs exportados da MESMA memoria sao equivalentes.
Uso: python comparar_csv.py exportacao_antiga.csv exportacao_nova.csv
Regra: o cabecalho deve ser identico e TODAS as linhas do arquivo antigo devem aparecer, na
mesma ordem e com o mesmo conteudo, no inicio do arquivo novo (o novo pode ter linhas extras
no fim, gravadas entre as duas exportacoes)."""
import sys

def read(path):
    with open(path, "r", encoding="utf-8-sig", newline="") as f:
        return [l.rstrip("\r\n") for l in f if l.strip()]

if len(sys.argv) != 3:
    sys.exit(__doc__)
old, new = read(sys.argv[1]), read(sys.argv[2])
if not old or not new:
    sys.exit("ERRO: arquivo vazio")
if old[0] != new[0]:
    print("DIFERENTE: cabecalho");  print(" antigo:", old[0]); print(" novo  :", new[0]); sys.exit(1)
for i, (a, b) in enumerate(zip(old, new), start=1):
    if a != b:
        print(f"DIFERENTE na linha {i}:\n antigo: {a}\n novo  : {b}"); sys.exit(1)
if len(new) < len(old):
    print(f"DIFERENTE: o arquivo novo tem menos linhas ({len(new)}) que o antigo ({len(old)})"); sys.exit(1)
print(f"IDENTICO: {len(old) - 1} registros do arquivo antigo reproduzidos exatamente "
      f"({len(new) - len(old)} registro(s) novo(s) no fim).")
