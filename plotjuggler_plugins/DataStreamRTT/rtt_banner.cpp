/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "rtt_banner.h"

namespace RttBanner
{
size_t findBannerEnd(const std::string& buffer)
{
  size_t first_non_text = std::string::npos;
  for (size_t i = 0; i < buffer.size(); i++)
  {
    const unsigned char c = static_cast<unsigned char>(buffer[i]);
    const bool is_text = (c == '\n' || c == '\r' || (c >= 0x20 && c < 0x7F));
    if (!is_text)
    {
      first_non_text = i;
      break;
    }
  }

  if (first_non_text == std::string::npos)
  {
    if (buffer.size() >= kMaxBannerScan)
    {
      return 0;
    }
    return std::string::npos;
  }

  const size_t last_newline = buffer.rfind('\n', first_non_text);
  if (last_newline == std::string::npos)
  {
    return 0;
  }
  return last_newline + 1;
}
}  // namespace RttBanner
