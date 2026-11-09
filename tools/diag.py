#!/usr/bin/env python3
"""Consulta o estado interno do STM32.
   python diag.py --port COM3              -> envia DIAG e mostra o estado
   python diag.py --port COM3 --cmd WDTTEST -> envia o comando de teste do watchdog"""
import argparse, sys
import serial

RESET_FLAGS = {31: "LPWR", 30: "WWDG", 29: "IWDG (watchdog)", 28: "SOFTWARE", 27: "POR/PDR (energia)", 26: "PIN (reset)", 25: "BOR"}
FAULTS = {0xFA02: "estouro de stack", 0xFA03: "falha de heap (malloc)", 0xFA04: "HardFault"}

ap = argparse.ArgumentParser()
ap.add_argument("--port", default="COM3")
ap.add_argument("--baud", type=int, default=115200)
ap.add_argument("--cmd", default="DIAG")
args = ap.parse_args()

with serial.Serial(args.port, args.baud, timeout=3.0) as ser:
    ser.reset_input_buffer()
    ser.write((args.cmd + "\r\n").encode()); ser.flush()
    if args.cmd != "DIAG":
        print(ser.readline().decode("utf-8", errors="replace").strip() or "(sem resposta)")
        sys.exit(0)
    for _ in range(5):
        line = ser.readline().decode("utf-8", errors="replace").strip()
        if line.startswith("DIAG,"):
            fields = dict(item.split("=", 1) for item in line.split(",")[1:] if "=" in item)
            for k, v in fields.items():
                print(f"{k}={v}")
            csr = int(fields.get("rstcsr", "0"))
            causes = [name for bit, name in RESET_FLAGS.items() if csr & (1 << bit)]
            print("\n-- interpretacao --")
            print("ultimo reset:", ", ".join(causes) if causes else "nenhuma flag")
            print("resets por watchdog (acumulado):", fields.get("wdgrst"))
            lf = int(fields.get("lastfault", "0"))
            print("ultima falha fatal antes do reset:", FAULTS.get(lf, "nenhuma" if lf == 0 else hex(lf)))
            names = ((0, "AHT"), (1, "BMP280"), (2, "MPU6500"), (3, "BH1750"), (4, "ENS160"))
            sens, sdet = int(fields.get("sens", "0")), int(fields.get("sdet", "0"))
            print("sensores respondendo agora:", [n for b, n in names if sens & (1 << b)] or "nenhum")
            print("sensores detectados desde o boot:", [n for b, n in names if sdet & (1 << b)] or "nenhum")
            bha, ensa = int(fields.get("bha", "0")), int(fields.get("ensa", "0"))
            print("BH1750 endereco:", hex(bha) if bha else "nao detectado")
            print("ENS160 endereco:", hex(ensa) if ensa else "nao detectado",
                  "| PART_ID lido:", hex(int(fields.get("enspid", "0"))),
                  "| DEVICE_STATUS:", hex(int(fields.get("ensst", "0"))),
                  "| OPMODE:", fields.get("ensop"), "(2 = standard)",
                  "| inicializacoes:", fields.get("ensinit"))
            val = int(fields.get("ensval", "0"))
            print("ENS160 ultima amostra: AQI", fields.get("ensaqi"), "| TVOC", fields.get("enstvoc"), "ppb | eCO2",
                  fields.get("enseco2"), "ppm | validade", val,
                  ["(normal)", "(aquecimento)", "(partida inicial)", "(invalido)"][val & 3])
            print("ENS160 contadores: sem dado", fields.get("enstmo"), "| erro interno", fields.get("enserr"),
                  "| falha I2C", fields.get("enscomm"), "| usos de RESET", fields.get("ensrst"))
            et, erh = int(fields.get("enset", "0")), int(fields.get("ensrh", "0"))
            print("ENS160 firmware:", fields.get("ensfw"), "| leituras zeradas descartadas:", fields.get("enszero"),
                  "| T/UR que o sensor usa: %.1f C / %.1f %%" % (et / 64.0 - 273.15, erh / 512.0))
            print("OLED:", "aceso" if fields.get("oledon") == "1" else "apagado (economia)",
                  "| vezes que apagou:", fields.get("oledoffn"), "| vezes que acendeu:", fields.get("oledonn"))
            raw = int(fields.get("bhraw", "0"))
            print("BH1750 ultima leitura: bruto", raw, "=> %.1f lux" % (raw / 1.2))
            stale = int(fields.get("wdgstale", "0"))
            print("tasks fora do prazo agora:", [n for b, n in ((1, "sensores"), (2, "OLED")) if stale & (1 << b)] or "nenhuma")
            sys.exit(0)
        if line:
            print("(ignorado)", line)
    print("Sem resposta DIAG: a defaultTask nao respondeu em 3 s.", file=sys.stderr)
    sys.exit(1)
