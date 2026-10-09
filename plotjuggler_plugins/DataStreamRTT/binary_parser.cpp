/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "binary_parser.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

BinaryParser::BinaryParser(RttDataMap map) : _map(std::move(map))
{
}

double BinaryParser::fieldValue(const uint8_t* record, const RttFieldDef& field) const
{
  if (field.type == "uint8")
  {
    return static_cast<double>(record[field.offset]);
  }
  if (field.type == "uint16")
  {
    uint16_t v;
    std::memcpy(&v, record + field.offset, sizeof(v));
    return static_cast<double>(v);
  }
  if (field.type == "uint32")
  {
    uint32_t v;
    std::memcpy(&v, record + field.offset, sizeof(v));
    return static_cast<double>(v);
  }
  // "float32" - o unico outro tipo aceito por RttDataMap::typeSize().
  float v;
  std::memcpy(&v, record + field.offset, sizeof(v));
  return static_cast<double>(v);
}

bool BinaryParser::decodeRecord(const uint8_t* record, bool is_dump, TelemetrySample& out)
{
  int64_t seq = -1;
  for (const RttFieldDef& field : _map.fields)
  {
    const double value = fieldValue(record, field);
    if (field.type == "float32" && !std::isfinite(value))
    {
      return false;
    }
    if (field.name == _map.seq_field)
    {
      seq = static_cast<int64_t>(value);
    }
    const bool publish =
        !is_dump || std::find(_map.dump_fields.begin(), _map.dump_fields.end(), field.name) !=
                        _map.dump_fields.end();
    if (publish)
    {
      out.values.emplace_back(field.name, value);
    }
  }
  out.is_dump = is_dump;

  if (!_map.seq_field.empty())
  {
    out.t = _map.nominal_rate_hz > 0.0 ? static_cast<double>(seq) / _map.nominal_rate_hz : 0.0;
    if (is_dump)
    {
      // Dump: seq do passado. Nao mexe na deteccao de reset/gap do ao
      // vivo; o proximo registro ao vivo nao conta o salto como perda.
      _after_dump = true;
      return true;
    }
    if (_last_seq >= 0 && seq < _last_seq &&
        (_map.reset_below_seq < 0 || seq < _map.reset_below_seq))
    {
      out.target_reset = true;
    }
    else if (_last_seq >= 0 && !_after_dump && seq > _last_seq)
    {
      const int64_t step = _map.seq_step;
      const int64_t delta = seq - _last_seq;
      const int64_t tolerance = (step > 1) ? 2 * step : 1;
      if (delta > tolerance)
      {
        _seq_gaps += static_cast<uint64_t>((delta + step / 2) / step - 1);
      }
    }
    _after_dump = false;
    _last_seq = seq;
  }
  return true;
}

bool BinaryParser::magicAt(size_t pos) const
{
  if (pos + sizeof(uint32_t) > _buffer.size())
  {
    return false;
  }
  uint32_t value;
  std::memcpy(&value, _buffer.data() + pos, sizeof(value));
  return value == _map.magic || (_map.has_dump_magic && value == _map.dump_magic);
}

bool BinaryParser::dumpMagicAt(size_t pos) const
{
  if (!_map.has_dump_magic || pos + sizeof(uint32_t) > _buffer.size())
  {
    return false;
  }
  uint32_t value;
  std::memcpy(&value, _buffer.data() + pos, sizeof(value));
  return value == _map.dump_magic;
}

size_t BinaryParser::findSync(size_t from) const
{
  const size_t rs = _map.record_size;
  for (size_t pos = from; pos + sizeof(uint32_t) <= _buffer.size(); pos++)
  {
    if (!magicAt(pos))
    {
      continue;
    }
    // Confirma com o registro seguinte quando ele ja chegou. Um casamento
    // isolado por acaso e' raro (medido: ~1e-11 por posicao no payload
    // real); exigir dois em sequencia torna o engano irrelevante.
    if (pos + rs + sizeof(uint32_t) <= _buffer.size() && !magicAt(pos + rs))
    {
      continue;
    }
    return pos;
  }
  return std::string::npos;
}

std::vector<TelemetrySample> BinaryParser::feed(const char* data, size_t len)
{
  _buffer.append(data, len);
  std::vector<TelemetrySample> out;

  // Formato legado sem magic: mantem o comportamento antigo (assume que o
  // buffer comeca num limite de registro).
  if (!_map.has_magic)
  {
    while (_buffer.size() >= _map.record_size)
    {
      const auto* record = reinterpret_cast<const uint8_t*>(_buffer.data());
      TelemetrySample sample;
      if (decodeRecord(record, false, sample))
      {
        out.push_back(std::move(sample));
      }
      else
      {
        _malformed++;
      }
      _buffer.erase(0, _map.record_size);
    }
    return out;
  }

  while (true)
  {
    if (!_synced)
    {
      const size_t sync = findSync(0);
      if (sync == std::string::npos)
      {
        // Nada aproveitavel ainda. Os ultimos 3 bytes podem ser o inicio
        // de uma magic partida entre dois pacotes TCP - preserva-os.
        const size_t keep = std::min<size_t>(_buffer.size(), sizeof(uint32_t) - 1);
        _discarded += _buffer.size() - keep;
        _buffer.erase(0, _buffer.size() - keep);
        break;
      }
      _discarded += sync;
      _buffer.erase(0, sync);
      _synced = true;
    }

    if (_buffer.size() < _map.record_size)
    {
      break;
    }

    // Cada registro revalida a magic: e' o que faz um sincronismo errado
    // (ou uma perda de bytes no meio do stream) se corrigir sozinho em
    // vez de virar lixo plausivel para sempre.
    if (!magicAt(0))
    {
      _synced = false;
      _resyncs++;
      _malformed++;
      // Pula 1 byte para nao reencontrar a mesma posicao ruim.
      _discarded += 1;
      _buffer.erase(0, 1);
      continue;
    }

    const auto* record = reinterpret_cast<const uint8_t*>(_buffer.data());
    TelemetrySample sample;
    if (decodeRecord(record, dumpMagicAt(0), sample))
    {
      out.push_back(std::move(sample));
    }
    else
    {
      _malformed++;
    }
    _buffer.erase(0, _map.record_size);
  }
  return out;
}
