#!/usr/bin/env python3
"""Fake do telnet RTT do GDB server: reproduz um capture na porta 19021.

Uso: replay_server.py [arquivo] [taxa_hz]
Padrao: tests/data/capture_sample.txt a 1000 Hz, em loop por cliente.
"""
import os
import socket
import sys
import time

base = os.path.dirname(os.path.abspath(__file__))
path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    base, "..", "tests", "data", "capture_sample.txt")
rate_hz = float(sys.argv[2]) if len(sys.argv) > 2 else 1000.0

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", 19021))
srv.listen(1)
print("escutando em 127.0.0.1:19021 ...")
while True:
    conn, addr = srv.accept()
    print("cliente conectado:", addr)
    conn.settimeout(0.2)
    try:
        cfg = conn.recv(128)  # config string do cliente (so loga)
        print("config recebida:", cfg)
    except socket.timeout:
        pass
    try:
        with open(path) as f:
            for line in f:
                conn.sendall(line.encode())
                time.sleep(1.0 / rate_hz)
    except (BrokenPipeError, ConnectionResetError):
        print("cliente desconectou")
    finally:
        conn.close()
