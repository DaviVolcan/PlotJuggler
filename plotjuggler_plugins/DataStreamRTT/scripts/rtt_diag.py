#!/usr/bin/env python3
"""Diagnostico de conexao com o canal telnet RTT do J-Link GDB Server.

Diferente do rtt_probe.py (que so imprime o que chega), este script deixa
explicito em qual dos tres estados a conexao esta:

  - FALHA AO CONECTAR       (porta fechada / sessao de debug parada)
  - RECUSADO                (conectou, mas o servidor ja tem outro cliente
                              ativo -- so aceita 1 conexao por vez)
  - CONECTADO, SEM DADOS    (conectou e nao foi recusado, mas nenhuma linha
                              chegou depois de STALL_S segundos -- travado)
  - CONECTADO, RECEBENDO    (dados fluindo; imprime taxa e ultima linha)

Uso: rtt_diag.py [porta] [canal] [stall_s]   (padrao: 2334, canal 1, 2s)
"""
import socket
import sys
import time

REFUSED_MARKER = b"already is an active connection"

port = int(sys.argv[1]) if len(sys.argv) > 1 else 2334
channel = int(sys.argv[2]) if len(sys.argv) > 2 else 1
stall_s = float(sys.argv[3]) if len(sys.argv) > 3 else 2.0

try:
    s = socket.create_connection(("127.0.0.1", port), timeout=3)
except OSError as e:
    print(f"FALHA AO CONECTAR em 127.0.0.1:{port}: {e}", file=sys.stderr)
    print("-> sessao de debug parada ou porta errada?", file=sys.stderr)
    sys.exit(1)

s.settimeout(0.5)
s.sendall(f"$$SEGGER_TELNET_ConfigStr=RTTCh;{channel}$$".encode())
print(f"conectado em 127.0.0.1:{port}, canal {channel} (Ctrl-C para sair)",
      file=sys.stderr)

last_data = time.monotonic()
lines_total = 0
lines_window = 0
window_start = last_data
stalled = False
buf = b""

try:
    while True:
        try:
            chunk = s.recv(4096)
        except socket.timeout:
            chunk = None

        now = time.monotonic()

        if chunk == b"":
            print(f"CONEXAO FECHADA pelo servidor apos {now - last_data:.1f}s "
                  f"sem dados ({lines_total} linhas recebidas no total)",
                  file=sys.stderr)
            break

        if chunk:
            if REFUSED_MARKER in chunk:
                print("RECUSADO: ja existe outra conexao ativa nesta porta "
                      "(o GDB server so aceita 1 cliente por vez)",
                      file=sys.stderr)
                break
            buf += chunk
            *complete, buf = buf.split(b"\n")
            if complete:
                lines_total += len(complete)
                lines_window += len(complete)
                last_data = now
                if stalled:
                    print(f"[voltou a receber dados apos {now - last_data:.1f}s]",
                          file=sys.stderr)
                    stalled = False
                sys.stdout.write(complete[-1].decode(errors="replace") + "\n")
                sys.stdout.flush()

        if now - window_start >= 1.0:
            gap = now - last_data
            if gap >= stall_s:
                if not stalled:
                    print(f"AVISO: CONECTADO, SEM DADOS ha {gap:.1f}s "
                          f"(recebeu {lines_total} linhas ate agora, depois parou)",
                          file=sys.stderr)
                    stalled = True
            else:
                print(f"[ok] {lines_window} linhas/s", file=sys.stderr)
            lines_window = 0
            window_start = now
except KeyboardInterrupt:
    pass
finally:
    elapsed = time.monotonic() - window_start
    print(f"encerrado: {lines_total} linhas recebidas no total", file=sys.stderr)
