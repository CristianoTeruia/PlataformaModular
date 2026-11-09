#!/usr/bin/env python3
"""Recebe a exportacao CSV do logger STM32 via USB CDC."""

import argparse
import csv
import sys
from datetime import datetime
from pathlib import Path

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("Dependencia ausente. Instale com: py -m pip install pyserial", file=sys.stderr)
    raise SystemExit(2)


def main():
    parser = argparse.ArgumentParser(description="Exporta os registros EEPROM do STM32 para CSV.")
    parser.add_argument("--port", default="COM3", help="Porta serial USB (padrao: COM3)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate virtual (padrao: 115200)")
    parser.add_argument("--output", help="Nome do CSV de saida")
    parser.add_argument("--timeout", type=float, default=10.0,
                        help="Timeout sem receber dados, em segundos (padrao: 10)")
    args = parser.parse_args()

    output = Path(args.output or f"dados_sensores_{datetime.now():%Y%m%d_%H%M%S}.csv")
    print("Portas seriais disponiveis:")
    for port in list_ports.comports():
        print(f"  {port.device}: {port.description}")
    print(f"\nAbrindo {args.port}; feche o PuTTY ou outro programa que esteja usando essa porta.")

    rows = []
    got_header = False
    ended = False
    summary = None
    malformed_rows = 0
    n_lines = 0
    last_line = ""

    try:
        with serial.Serial(args.port, args.baud, timeout=args.timeout) as ser:
            ser.reset_input_buffer()
            ser.write(b"EXPORT\r\n")
            ser.flush()
            print("Comando EXPORT enviado. Aguardando os dados...\n")

            while True:
                raw = ser.readline()
                if not raw:
                    raise TimeoutError(
                        f"Nenhum dado recebido em {args.timeout:.0f} s. Linhas recebidas ate agora: {n_lines}. "
                        f"Ultima linha: {last_line[:80]!r}")
                line = raw.decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                n_lines += 1
                last_line = line
                # Diagnostico: mostra as primeiras linhas e depois uma a cada 200.
                if n_lines <= 5 or n_lines % 200 == 0 or line.startswith(("ERRO,", "EXPORT", "PROGRESS,")):
                    print(f"[{n_lines}] {line[:100]}", flush=True)
                if line.startswith("PROGRESS,"):
                    continue  # keepalive do firmware: nao e dado

                if line.startswith("ERRO,"):
                    raise RuntimeError(f"O dispositivo informou: {line}")
                if line == "EXPORT_BEGIN":
                    continue
                if line.startswith("EXPORT_END,"):
                    parts = line.split(",")
                    summary = parts[1:] if len(parts) >= 3 else []
                    ended = True
                    break
                if line.startswith("EXPORT,"):
                    continue
                if line.startswith("sensor_mask,"):
                    got_header = True
                    rows.append(line.split(","))
                    continue
                if got_header:
                    # Linhas CSV de dados; linhas de diagnostico nao sao gravadas.
                    if line.startswith("ERRO,"):
                        raise RuntimeError(line)
                    fields = next(csv.reader([line]))
                    if len(fields) == len(rows[0]):
                        rows.append(fields)
                    else:
                        malformed_rows += 1
                        print(
                            f"AVISO: linha CSV ignorada por ter {len(fields)} colunas "
                            f"(esperadas {len(rows[0])}). Total de linhas malformadas: {malformed_rows}.",
                            file=sys.stderr,
                        )

    except (serial.SerialException, TimeoutError, RuntimeError) as exc:
        print(f"\nFalha na exportacao: {exc}", file=sys.stderr)
        raise SystemExit(1)

    if not ended or not got_header:
        print("A exportacao terminou sem cabecalho/confirmacao completos; CSV nao foi salvo.", file=sys.stderr)
        raise SystemExit(1)

    with output.open("w", newline="", encoding="utf-8-sig") as file:
        writer = csv.writer(file)
        writer.writerows(rows)

    data_count = max(0, len(rows) - 1)
    print(f"\nCSV salvo em: {output.resolve()}")
    print(f"Registros recebidos e gravados no CSV: {data_count}")
    print(f"Linhas CSV malformadas ignoradas: {malformed_rows}")
    if summary and len(summary) >= 2:
        print(f"Registros validos reportados pelo STM32: {summary[0]}")
        print(f"Bytes/registros suspeitos ignorados reportados: {summary[1]}")

    # A divergencia bloqueia a limpeza, mas nao deve impedir a correcao do RTC.
    # Assim, os dados permanecem protegidos e o usuario ainda pode acertar o relogio.
    cleanup_allowed = True
    if summary and len(summary) >= 2:
        try:
            reported_count = int(summary[0])
        except ValueError:
            reported_count = -1
        if reported_count != data_count:
            cleanup_allowed = False
            print(
                "LIMPEZA BLOQUEADA: a quantidade de registros no CSV "
                f"({data_count}) difere da informada pela STM32 ({summary[0]}). "
                "Os dados foram preservados; a correcao do RTC sera tentada mesmo assim.",
                file=sys.stderr
            )

    # Ajusta o RTC depois de salvar o CSV, independentemente da limpeza.
    # O computador fornece a data/hora real; o STM32 confirma apenas apos configurar o RTC.
    now = datetime.now().astimezone()
    rtc_command = f"RTC_SET,{now.year:04d},{now.month:02d},{now.day:02d},{now.hour:02d},{now.minute:02d},{now.second:02d}"
    print("\nEnviando a data e hora atuais do computador para corrigir o RTC...")
    try:
        with serial.Serial(args.port, args.baud, timeout=10.0) as ser:
            ser.reset_input_buffer()
            ser.write((rtc_command + "\r\n").encode("ascii"))
            ser.flush()
            rtc_ok = False
            deadline = datetime.now().timestamp() + 10.0
            while datetime.now().timestamp() < deadline:
                raw = ser.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").strip()
                if line:
                    print(line)
                if line.startswith("RTC_SET_END,OK,"):
                    rtc_ok = True
                    break
                if line.startswith("RTC_SET_END,ERRO") or line.startswith("ERRO,"):
                    break
            if not rtc_ok:
                print(
                    "ERRO: nao foi possivel confirmar a correcao do RTC. "
                    "O horario deve continuar sendo tratado como nao confiavel; "
                    "as EEPROMs foram preservadas.",
                    file=sys.stderr,
                )
                raise SystemExit(4)
    except serial.SerialException as exc:
        print(f"Falha ao corrigir o RTC: {exc}. EEPROMs preservadas.", file=sys.stderr)
        raise SystemExit(4)

    # Se a exportacao apresentou divergencia, nunca oferecer a limpeza nesta execucao.
    if not cleanup_allowed:
        print("RTC tratado; limpeza continua bloqueada ate investigar a divergencia dos registros.")
        return

    # A limpeza e separada e exige confirmacao explicita depois de exportar e ajustar o RTC.
    answer = input(
        "\nDeseja apagar as DUAS EEPROMs e reinicializar os metadados? "
        "Confirme digitando APAGAR: "
    ).strip()
    if answer != "APAGAR":
        print("Memorias preservadas. Nenhum comando CLEAR foi enviado.")
        return

    print("Enviando CLEAR. A limpeza fisica pode levar cerca de 20 segundos...")
    try:
        with serial.Serial(args.port, args.baud, timeout=65.0) as ser:
            ser.reset_input_buffer()
            ser.write(b"CLEAR,CONFIRMO\r\n")  # o firmware exige a confirmacao explicita
            ser.flush()
            deadline = datetime.now().timestamp() + 65.0
            while datetime.now().timestamp() < deadline:
                raw = ser.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").strip()
                if line:
                    print(f"Resposta recebida: {line!r}")
                if line == "CLEAR_END,OK":
                    print("EEPROMs apagadas e metadados reinicializados com sucesso.")
                    return
                if line == "CLEAR_END,ERRO":
                    print("ERRO: a STM32 nao conseguiu concluir a limpeza.", file=sys.stderr)
                    raise SystemExit(3)
                if line.startswith("ERRO,"):
                    print(f"ERRO informado pela STM32: {line}", file=sys.stderr)
                    raise SystemExit(3)
            print("Timeout aguardando confirmacao da limpeza. Verifique o estado da STM32.",
                  file=sys.stderr)
            raise SystemExit(3)
    except serial.SerialException as exc:
        print(f"Falha ao solicitar/confirmar limpeza: {exc}", file=sys.stderr)
        raise SystemExit(3)


if __name__ == "__main__":
    main()
