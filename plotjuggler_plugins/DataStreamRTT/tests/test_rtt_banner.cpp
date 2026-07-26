/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include <gtest/gtest.h>

#include "rtt_banner.h"

TEST(RttBanner, NoBannerReturnsZero)
{
  const std::string data("\x01\x02\x03\x04", 4);
  EXPECT_EQ(RttBanner::findBannerEnd(data), 0u);
}

TEST(RttBanner, DetectsSeggerStyleBanner)
{
  const std::string banner =
      "SEGGER J-Link V8.68 - Real time terminal output\r\n"
      "SEGGER J-Link V9.7, SN=69730336\r\n"
      "Process: JLinkGDBServerCLExe\r\n";
  const std::string data = banner + std::string("\x01\x02\x03\x04", 4);
  EXPECT_EQ(RttBanner::findBannerEnd(data), banner.size());
}

TEST(RttBanner, WaitsForMoreDataWhileStillAllText)
{
  const std::string data = "SEGGER J-Link partial line, no newline yet";
  EXPECT_EQ(RttBanner::findBannerEnd(data), std::string::npos);
}

TEST(RttBanner, GivesUpAfterMaxScanWithNoNonTextByte)
{
  const std::string data(RttBanner::kMaxBannerScan, 'A');
  EXPECT_EQ(RttBanner::findBannerEnd(data), 0u);
}

TEST(RttBanner, SingleLineBannerNoTrailingBlankLine)
{
  const std::string data = "hello\r\n" + std::string("\xFF\xFE", 2);
  EXPECT_EQ(RttBanner::findBannerEnd(data), 7u);
}

TEST(RttBanner, EmptyBufferWaitsForData)
{
  EXPECT_EQ(RttBanner::findBannerEnd(""), std::string::npos);
}
