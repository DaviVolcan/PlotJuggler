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

private:
  bool decodeRecord(const uint8_t* record, TelemetrySample& out);
  double fieldValue(const uint8_t* record, const RttFieldDef& field) const;

  RttDataMap _map;
  std::string _buffer;
  int64_t _last_seq = -1;
  uint64_t _malformed = 0;
  uint64_t _seq_gaps = 0;
};
