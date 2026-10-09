/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct RttFieldDef
{
  std::string name;
  size_t offset = 0;
  std::string type;  // "uint8" | "uint16" | "uint32" | "float32"
  std::string unit;
};

struct RttDataMap
{
  size_t record_size = 0;
  std::string seq_field;
  double nominal_rate_hz = 1000.0;
  std::vector<RttFieldDef> fields;

  // Palavra de sincronismo no inicio de cada registro (offset 0, little
  // endian). RTT e' um stream de bytes sem enquadramento e o host nunca
  // comeca a ler num limite de registro, entao sem magic o parser
  // desliza e decodifica lixo plausivel silenciosamente. Opcional:
  // has_magic == false mantem o comportamento legado (assume que o
  // primeiro byte recebido inicia um registro).
  bool has_magic = false;
  uint32_t magic = 0;

  // Versao do layout, espelhada no byte baixo da magic pelo firmware
  // (ver Core/Inc/telemetry.h). Serve para diagnostico: um firmware com
  // layout diferente simplesmente nunca sincroniza.
  int version = 0;

  // Telemetria v6 (captura rapida da malha de corrente, TCC Fase 12):
  // registros de dump levam a magic com o bit 0x80 na versao, trazem so'
  // os campos de dump_fields e um seq (tick de 8 kHz) do passado.
  bool has_dump_magic = false;
  uint32_t dump_magic = 0;
  std::vector<std::string> dump_fields;
  // Incremento nominal do seq entre registros ao vivo (v6: 8, tick de
  // 8 kHz amostrado a 1 kHz). Gap so' acima de 2x isso (jitter da task).
  int64_t seq_step = 1;
  // Reset do alvo so' quando o seq volta E fica abaixo disto (o tick
  // zera no boot). -1 = legado: qualquer retrocesso e' reset.
  int64_t reset_below_seq = -1;

  static size_t typeSize(const std::string& type);

  // Throws std::runtime_error with a human-readable message on any
  // malformed/missing content (bad JSON, missing field, offset+size
  // overflowing record_size, unknown type, seq_field not found).
  static RttDataMap loadFromJsonText(const std::string& json_text);
  static RttDataMap loadFromFile(const std::string& path);
};
