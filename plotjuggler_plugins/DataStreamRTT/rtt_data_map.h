/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct RttFieldDef
{
  std::string name;
  size_t offset = 0;
  std::string type;  // "uint8" | "uint16" | "uint32" | "float32"
  std::string unit;
};

struct RttDataMap
{
  size_t record_size = 0;
  std::string seq_field;
  double nominal_rate_hz = 1000.0;
  std::vector<RttFieldDef> fields;

  static size_t typeSize(const std::string& type);

  // Throws std::runtime_error with a human-readable message on any
  // malformed/missing content (bad JSON, missing field, offset+size
  // overflowing record_size, unknown type, seq_field not found).
  static RttDataMap loadFromJsonText(const std::string& json_text);
  static RttDataMap loadFromFile(const std::string& path);
};
