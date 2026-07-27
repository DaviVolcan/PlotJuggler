/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "rtt_data_map.h"
#include "telemetry_sample.h"

class BinaryParser
{
public:
  explicit BinaryParser(RttDataMap map);

  std::vector<TelemetrySample> feed(const char* data, size_t len);

  uint64_t malformedCount() const
  {
    return _malformed;
  }
  uint64_t sequenceGaps() const
  {
    return _seq_gaps;
  }

  // Bytes jogados fora procurando a magic (banner do GDB Server, cauda
  // de registro parcial no connect, ressincronismo). Se isto cresce sem
  // parar e synced() nunca fica true, o firmware esta com um layout
  // diferente do mapa (magic/versao nao batem).
  uint64_t discardedBytes() const
  {
    return _discarded;
  }
  bool synced() const
  {
    return _synced;
  }
  // Quantas vezes o sincronismo foi perdido depois de ja ter sido obtido.
  uint64_t resyncs() const
  {
    return _resyncs;
  }

private:
  bool decodeRecord(const uint8_t* record, TelemetrySample& out);
  double fieldValue(const uint8_t* record, const RttFieldDef& field) const;
  bool magicAt(size_t pos) const;
  // Procura o proximo inicio de registro plausivel a partir de `from`.
  // Exige confirmacao (magic tambem um registro adiante) quando ha bytes
  // suficientes; sem isso aceita provisoriamente, porque cada registro
  // revalida a magic e um engano se corrige sozinho no proximo.
  size_t findSync(size_t from) const;

  RttDataMap _map;
  std::string _buffer;
  int64_t _last_seq = -1;
  uint64_t _malformed = 0;
  uint64_t _seq_gaps = 0;
  uint64_t _discarded = 0;
  uint64_t _resyncs = 0;
  bool _synced = false;
};
