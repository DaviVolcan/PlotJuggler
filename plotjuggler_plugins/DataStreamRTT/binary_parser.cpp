/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "binary_parser.h"

#include <cmath>
#include <cstring>
#include <utility>

BinaryParser::BinaryParser(RttDataMap map) : _map(std::move(map))
{
}

double BinaryParser::fieldValue(const uint8_t* record,
                                const RttFieldDef& field) const
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

bool BinaryParser::decodeRecord(const uint8_t* record, TelemetrySample& out)
{
  int64_t seq = -1;
  for (const RttFieldDef& field : _map.fields)
  {
    const double value = fieldValue(record, field);
    if (field.type == "float32" && !std::isfinite(value))
    {
      return false;
    }
    out.values.emplace_back(field.name, value);
    if (field.name == _map.seq_field)
    {
      seq = static_cast<int64_t>(value);
    }
  }

  if (!_map.seq_field.empty())
  {
    if (_last_seq >= 0 && seq < _last_seq)
    {
      out.target_reset = true;
    }
    else if (_last_seq >= 0 && seq > _last_seq + 1)
    {
      _seq_gaps += static_cast<uint64_t>(seq - _last_seq - 1);
    }
    _last_seq = seq;
    out.t = _map.nominal_rate_hz > 0.0 ? static_cast<double>(seq) /
                                          _map.nominal_rate_hz
                                        : 0.0;
  }
  return true;
}

std::vector<TelemetrySample> BinaryParser::feed(const char* data, size_t len)
{
  _buffer.append(data, len);
  std::vector<TelemetrySample> out;

  while (_buffer.size() >= _map.record_size)
  {
    const auto* record = reinterpret_cast<const uint8_t*>(_buffer.data());
    TelemetrySample sample;
    if (decodeRecord(record, sample))
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
