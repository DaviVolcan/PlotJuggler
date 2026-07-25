#include <gtest/gtest.h>

#include <cmath>
#include <cstring>

#include "binary_parser.h"

namespace
{
RttDataMap TestMap()
{
  return RttDataMap::loadFromJsonText(R"json(
  {
    "record_size": 12,
    "seq_field": "seq",
    "nominal_rate_hz": 1000,
    "fields": [
      {"name": "seq", "offset": 0, "type": "uint32"},
      {"name": "ib",  "offset": 4, "type": "float32", "unit": "A"},
      {"name": "ic",  "offset": 8, "type": "float32", "unit": "A"}
    ]
  }
  )json");
}

std::string PackRecord(uint32_t seq, float ib, float ic)
{
  std::string buf(12, '\0');
  std::memcpy(&buf[0], &seq, 4);
  std::memcpy(&buf[4], &ib, 4);
  std::memcpy(&buf[8], &ic, 4);
  return buf;
}
}  // namespace

TEST(BinaryParser, DecodesSingleRecord)
{
  BinaryParser parser(TestMap());
  const std::string record = PackRecord(1, 1.5f, -2.5f);
  const auto samples = parser.feed(record.data(), record.size());
  ASSERT_EQ(samples.size(), 1u);
  EXPECT_DOUBLE_EQ(samples[0].t, 0.001);
  ASSERT_EQ(samples[0].values.size(), 3u);
  EXPECT_EQ(samples[0].values[1].first, "ib");
  EXPECT_FLOAT_EQ(static_cast<float>(samples[0].values[1].second), 1.5f);
  EXPECT_FLOAT_EQ(static_cast<float>(samples[0].values[2].second), -2.5f);
}

TEST(BinaryParser, BuffersPartialRecord)
{
  BinaryParser parser(TestMap());
  const std::string record = PackRecord(1, 1.0f, 2.0f);
  auto samples = parser.feed(record.data(), 6);  // metade do registro
  EXPECT_TRUE(samples.empty());
  samples = parser.feed(record.data() + 6, record.size() - 6);
  ASSERT_EQ(samples.size(), 1u);
}

TEST(BinaryParser, DetectsSequenceGap)
{
  BinaryParser parser(TestMap());
  auto r1 = PackRecord(1, 0.0f, 0.0f);
  auto r3 = PackRecord(3, 0.0f, 0.0f);  // pula o 2
  parser.feed(r1.data(), r1.size());
  parser.feed(r3.data(), r3.size());
  EXPECT_EQ(parser.sequenceGaps(), 1u);
}

TEST(BinaryParser, DetectsTargetReset)
{
  BinaryParser parser(TestMap());
  auto r10 = PackRecord(10, 0.0f, 0.0f);
  auto r0 = PackRecord(0, 0.0f, 0.0f);  // firmware reiniciou
  parser.feed(r10.data(), r10.size());
  const auto samples = parser.feed(r0.data(), r0.size());
  ASSERT_EQ(samples.size(), 1u);
  EXPECT_TRUE(samples[0].target_reset);
}

TEST(BinaryParser, RejectsNonFiniteFloat)
{
  BinaryParser parser(TestMap());
  const float nan_value = std::nanf("");
  auto record = PackRecord(1, nan_value, 0.0f);
  const auto samples = parser.feed(record.data(), record.size());
  EXPECT_TRUE(samples.empty());
  EXPECT_EQ(parser.malformedCount(), 1u);
}
