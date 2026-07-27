/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
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

// ---------------------------------------------------------------------
// Enquadramento por magic (v2). Reproduz o modo de falha de 2026-07-26:
// o stream RTT nunca comeca num limite de registro (o GDB Server despeja
// um backlog de ~1 MiB no connect e 1 MiB nao e' multiplo do registro),
// e sem magic o parser decodificava lixo fisicamente plausivel para
// sempre, sem nenhum erro.
// ---------------------------------------------------------------------
namespace
{
constexpr uint32_t kMagic = 0xC3A55A02u;

RttDataMap MagicMap()
{
  return RttDataMap::loadFromJsonText(R"json(
  {
    "version": 2,
    "record_size": 16,
    "magic": "0xC3A55A02",
    "seq_field": "seq",
    "nominal_rate_hz": 1000,
    "fields": [
      {"name": "seq", "offset": 4,  "type": "uint32"},
      {"name": "ib",  "offset": 8,  "type": "float32", "unit": "A"},
      {"name": "ic",  "offset": 12, "type": "float32", "unit": "A"}
    ]
  }
  )json");
}

std::string PackMagicRecord(uint32_t seq, float ib, float ic)
{
  std::string buf(16, '\0');
  std::memcpy(&buf[0], &kMagic, 4);
  std::memcpy(&buf[4], &seq, 4);
  std::memcpy(&buf[8], &ib, 4);
  std::memcpy(&buf[12], &ic, 4);
  return buf;
}

std::string PackMagicStream(uint32_t first_seq, int count)
{
  std::string out;
  for (int i = 0; i < count; i++)
  {
    out += PackMagicRecord(first_seq + static_cast<uint32_t>(i), static_cast<float>(i),
                           -static_cast<float>(i));
  }
  return out;
}
}  // namespace

TEST(BinaryParserMagic, MapCarriesMagicAndVersion)
{
  const RttDataMap map = MagicMap();
  EXPECT_TRUE(map.has_magic);
  EXPECT_EQ(map.magic, kMagic);
  EXPECT_EQ(map.version, 2);
}

TEST(BinaryParserMagic, DecodesAlignedStream)
{
  BinaryParser parser(MagicMap());
  const std::string stream = PackMagicStream(1, 4);
  const auto samples = parser.feed(stream.data(), stream.size());
  ASSERT_EQ(samples.size(), 4u);
  EXPECT_EQ(samples[0].values[0].first, "seq");
  EXPECT_TRUE(parser.synced());
  EXPECT_EQ(parser.discardedBytes(), 0u);
}

// O caso real: a conexao comeca no meio de um registro.
TEST(BinaryParserMagic, SyncsWhenStreamStartsMidRecord)
{
  BinaryParser parser(MagicMap());
  const std::string stream = PackMagicStream(100, 6);
  const size_t skew = 7;  // comeca 7 bytes dentro do primeiro registro
  const auto samples = parser.feed(stream.data() + skew, stream.size() - skew);
  ASSERT_EQ(samples.size(), 5u);  // perde so o registro parcial inicial
  EXPECT_EQ(samples[0].values[0].second, 101.0);
  EXPECT_EQ(parser.discardedBytes(), 16u - skew);
}

// O banner de texto do GDB Server e' descartado sem nenhuma heuristica
// de texto: simplesmente nao casa com a magic.
TEST(BinaryParserMagic, SkipsGdbServerBannerWithoutTextHeuristic)
{
  BinaryParser parser(MagicMap());
  const std::string banner = "SEGGER J-Link V8.68 - Real time terminal output\r\n"
                             "Process: JLinkGDBServerCLExe\r\n";
  const std::string stream = banner + PackMagicStream(1, 3);
  const auto samples = parser.feed(stream.data(), stream.size());
  ASSERT_EQ(samples.size(), 3u);
  EXPECT_EQ(parser.discardedBytes(), banner.size());
}

// Bytes perdidos no meio do stream: o parser precisa se recuperar
// sozinho, nao ficar desalinhado para sempre.
TEST(BinaryParserMagic, ResyncsAfterCorruptionMidStream)
{
  BinaryParser parser(MagicMap());
  std::string stream = PackMagicStream(1, 3);
  stream += std::string("\x11\x22\x33", 3);  // 3 bytes intrusos
  stream += PackMagicStream(4, 3);

  const auto samples = parser.feed(stream.data(), stream.size());
  EXPECT_EQ(samples.size(), 6u);
  EXPECT_GE(parser.resyncs(), 1u);
  EXPECT_TRUE(parser.synced());
  ASSERT_EQ(samples.back().values[0].first, "seq");
  EXPECT_EQ(samples.back().values[0].second, 6.0);
}

// Um firmware com layout diferente (magic/versao que nao batem) nunca
// sincroniza - e isso fica observavel, em vez de virar dado errado.
TEST(BinaryParserMagic, NeverSyncsOnForeignMagic)
{
  BinaryParser parser(MagicMap());
  std::string stream;
  const uint32_t other = 0xC3A55A01u;  // mesma familia, versao 1
  for (int i = 0; i < 8; i++)
  {
    std::string rec(16, '\0');
    std::memcpy(&rec[0], &other, 4);
    stream += rec;
  }
  const auto samples = parser.feed(stream.data(), stream.size());
  EXPECT_TRUE(samples.empty());
  EXPECT_FALSE(parser.synced());
  EXPECT_GT(parser.discardedBytes(), 0u);
}

// Magic partida entre dois pacotes TCP nao pode ser perdida.
TEST(BinaryParserMagic, HandlesMagicSplitAcrossFeeds)
{
  BinaryParser parser(MagicMap());
  const std::string stream = PackMagicStream(1, 3);
  auto samples = parser.feed(stream.data(), 2);  // so 2 bytes da magic
  EXPECT_TRUE(samples.empty());
  samples = parser.feed(stream.data() + 2, stream.size() - 2);
  EXPECT_EQ(samples.size(), 3u);
}
