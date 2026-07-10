#!/usr/bin/env python3
"""Conecta no canal telnet RTT do J-Link GDB Server e imprime um up-channel.

Uso: rtt_probe.py [porta] [canal]   (padrao: 19021, canal 1)
Obs.: o GDB server iniciado pelo CLion usa a porta do campo "console port"
da config de debug (neste projeto: 2334).
"""
import socket
import sys

port = int(sys.argv[1]) if len(sys.argv) > 1 else 19021
channel = int(sys.argv[2]) if len(sys.argv) > 2 else 1

s = socket.create_connection(("127.0.0.1", port), timeout=3)
# A config string precisa chegar em ate 100 ms apos o connect.
s.sendall(f"$$SEGGER_TELNET_ConfigStr=RTTCh;{channel}$$".encode())
print(f"conectado em 127.0.0.1:{port}; lendo canal {channel} (Ctrl-C para sair)",
      file=sys.stderr)
try:
    while True:
        data = s.recv(4096)
        if not data:
            print("conexao fechada pelo servidor", file=sys.stderr)
            break
        sys.stdout.write(data.decode(errors="replace"))
        sys.stdout.flush()
except KeyboardInterrupt:
    pass
