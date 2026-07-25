/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <gtest/gtest.h>

#include "rtt_data_map.h"

namespace
{
const char* kValidMap = R"json(
{
  "version": 1,
  "record_size": 12,
  "byte_order": "little",
  "nominal_rate_hz": 1000,
  "seq_field": "seq",
  "fields": [
    {"name": "seq", "offset": 0, "type": "uint32"},
    {"name": "ib",  "offset": 4, "type": "float32", "unit": "A"},
    {"name": "ic",  "offset": 8, "type": "float32", "unit": "A"}
  ]
}
)json";
}  // namespace

TEST(RttDataMap, ParsesValidMap)
{
  const RttDataMap map = RttDataMap::loadFromJsonText(kValidMap);
  EXPECT_EQ(map.record_size, 12u);
  EXPECT_EQ(map.seq_field, "seq");
  EXPECT_DOUBLE_EQ(map.nominal_rate_hz, 1000.0);
  ASSERT_EQ(map.fields.size(), 3u);
  EXPECT_EQ(map.fields[1].name, "ib");
  EXPECT_EQ(map.fields[1].offset, 4u);
  EXPECT_EQ(map.fields[1].type, "float32");
  EXPECT_EQ(map.fields[1].unit, "A");
}

TEST(RttDataMap, RejectsMalformedJson)
{
  EXPECT_THROW(RttDataMap::loadFromJsonText("{not valid json"), std::runtime_error);
}

TEST(RttDataMap, RejectsMissingRecordSize)
{
  const char* json = R"json({"fields": []})json";
  EXPECT_THROW(RttDataMap::loadFromJsonText(json), std::runtime_error);
}

TEST(RttDataMap, RejectsFieldPastRecordSize)
{
  const char* json = R"json(
  {
    "record_size": 4,
    "fields": [{"name": "x", "offset": 2, "type": "float32"}]
  }
  )json";
  EXPECT_THROW(RttDataMap::loadFromJsonText(json), std::runtime_error);
}

TEST(RttDataMap, RejectsUnknownSeqField)
{
  const char* json = R"json(
  {
    "record_size": 4,
    "seq_field": "nope",
    "fields": [{"name": "x", "offset": 0, "type": "uint32"}]
  }
  )json";
  EXPECT_THROW(RttDataMap::loadFromJsonText(json), std::runtime_error);
}

TEST(RttDataMap, RejectsNegativeRecordSize)
{
  const char* json = R"json(
  {
    "record_size": -1,
    "fields": [{"name": "x", "offset": 0, "type": "uint32"}]
  }
  )json";
  EXPECT_THROW(RttDataMap::loadFromJsonText(json), std::runtime_error);
}

TEST(RttDataMap, RejectsNegativeFieldOffset)
{
  const char* json = R"json(
  {
    "record_size": 12,
    "fields": [{"name": "x", "offset": -1, "type": "uint32"}]
  }
  )json";
  EXPECT_THROW(RttDataMap::loadFromJsonText(json), std::runtime_error);
}

TEST(RttDataMap, RejectsNonArrayFields)
{
  const char* json = R"json({"record_size": 4, "fields": "oops"})json";
  EXPECT_THROW(RttDataMap::loadFromJsonText(json), std::runtime_error);
}

TEST(RttDataMap, RejectsNonLittleByteOrder)
{
  const char* json = R"json(
  {
    "record_size": 4,
    "byte_order": "big",
    "fields": [{"name": "x", "offset": 0, "type": "uint32"}]
  }
  )json";
  EXPECT_THROW(RttDataMap::loadFromJsonText(json), std::runtime_error);
}

TEST(RttDataMap, LoadFromFileThrowsOnMissingFile)
{
  EXPECT_THROW(RttDataMap::loadFromFile("/no/such/file.json"), std::runtime_error);
}
