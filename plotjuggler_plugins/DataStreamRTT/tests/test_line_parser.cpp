#include <gtest/gtest.h>

#include "line_parser.h"

TEST(LineParser, ParsesValidLine)
{
  LineParser p;
  const std::string in = "t=1.234567,n=42,ia=0.1200,ib=-0.0500\n";
  auto samples = p.feed(in.data(), in.size());
  ASSERT_EQ(samples.size(), 1u);
  EXPECT_DOUBLE_EQ(samples[0].t, 1.234567);
  EXPECT_FALSE(samples[0].target_reset);
  ASSERT_EQ(samples[0].values.size(), 3u);  // n, ia, ib
  EXPECT_EQ(samples[0].values[0].first, "n");
  EXPECT_DOUBLE_EQ(samples[0].values[0].second, 42.0);
  EXPECT_EQ(samples[0].values[1].first, "ia");
  EXPECT_DOUBLE_EQ(samples[0].values[1].second, 0.12);
  EXPECT_EQ(p.malformedCount(), 0u);
}

TEST(LineParser, JoinsPartialFeeds)
{
  LineParser p;
  const std::string a = "t=1.0,n=1,ia=0.5";
  const std::string b = ",ib=1.0\nt=2.0,n=2,ia=0.6,ib=1.1\n";
  EXPECT_TRUE(p.feed(a.data(), a.size()).empty());
  auto samples = p.feed(b.data(), b.size());
  ASSERT_EQ(samples.size(), 2u);
  EXPECT_DOUBLE_EQ(samples[0].t, 1.0);
  EXPECT_DOUBLE_EQ(samples[1].t, 2.0);
}

TEST(LineParser, DiscardsGdbServerBannerAsMalformed)
{
  LineParser p;
  const std::string in = "SEGGER J-Link GDB Server\r\nt=1.0,n=1,ia=0,ib=0\n";
  auto samples = p.feed(in.data(), in.size());
  ASSERT_EQ(samples.size(), 1u);
  EXPECT_EQ(p.malformedCount(), 1u);
}

TEST(LineParser, DiscardsTruncatedLineWithoutTimestamp)
{
  LineParser p;
  // Conexao no meio de uma linha: o rabo "a=0.12..." nao tem "t=".
  const std::string in = "a=0.12,ib=-0.05\nt=5.0,n=10,ia=0,ib=0\n";
  auto samples = p.feed(in.data(), in.size());
  ASSERT_EQ(samples.size(), 1u);
  EXPECT_DOUBLE_EQ(samples[0].t, 5.0);
  EXPECT_EQ(p.malformedCount(), 1u);
}

TEST(LineParser, CountsSequenceGaps)
{
  LineParser p;
  const std::string in = "t=1.0,n=1,ia=0,ib=0\n"
                         "t=2.0,n=5,ia=0,ib=0\n";  // pulou 2,3,4
  auto samples = p.feed(in.data(), in.size());
  ASSERT_EQ(samples.size(), 2u);
  EXPECT_EQ(p.sequenceGaps(), 3u);
}

TEST(LineParser, DetectsTargetReset)
{
  LineParser p;
  const std::string in = "t=100.0,n=900,ia=0,ib=0\n"
                         "t=0.5,n=1,ia=0,ib=0\n"   // reboot: t voltou
                         "t=0.6,n=2,ia=0,ib=0\n";
  auto samples = p.feed(in.data(), in.size());
  ASSERT_EQ(samples.size(), 3u);
  EXPECT_FALSE(samples[0].target_reset);
  EXPECT_TRUE(samples[1].target_reset);
  EXPECT_FALSE(samples[2].target_reset);
  // O reset zera o rastreio de sequencia: n=900 -> n=1 nao conta como buraco.
  EXPECT_EQ(p.sequenceGaps(), 0u);
}

TEST(LineParser, RejectsGarbageValues)
{
  LineParser p;
  const std::string in = "t=1.0,n=1,ia=abc,ib=0\n"
                         "t=,n=2,ia=0,ib=0\n"
                         "t=2.0,n=3,ia=0,ib=0\n";
  auto samples = p.feed(in.data(), in.size());
  ASSERT_EQ(samples.size(), 1u);
  EXPECT_DOUBLE_EQ(samples[0].t, 2.0);
  EXPECT_EQ(p.malformedCount(), 2u);
}
