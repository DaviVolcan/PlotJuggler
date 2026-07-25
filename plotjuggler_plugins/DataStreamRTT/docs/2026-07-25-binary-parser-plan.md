# DataStreamRTT Binary Parser Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace `DataStreamRTT`'s text `key=value` parser (`LineParser`) with a `BinaryParser` that decodes fixed-size binary records using a JSON data map, since no firmware in the project emits the text format anymore (see companion firmware plan, `docs/superpowers/plans/2026-07-25-rtt-binary-telemetry-firmware.md`, in the `TCC_Firmware` repo).

**Architecture:** `RttDataMap` loads/validates the JSON schema (offsets, types, names, units) via `QJsonDocument`. `BinaryParser` decodes fixed-size records from raw bytes using that schema, computing time from the `seq` field (`t = seq / nominal_rate_hz`) instead of trusting host arrival time — immune to TCP/telnet jitter. Both produce the same `TelemetrySample` (extracted from `line_parser.h` into its own `telemetry_sample.h`) that `pushSamples()`/`CsvLogger` already consume, so those stay unchanged.

**Tech Stack:** C++17, Qt5 (`QtCore`, `QtWidgets`, `QtNetwork`, `QtXml`), GoogleTest (already wired via `FetchContent` fallback in this plugin's `CMakeLists.txt`).

## Global Constraints

- Google C++ Style per repo `CLAUDE.md`: 2-space indent, 100-char lines, braces on new lines for classes/functions, PascalCase classes, camelCase methods, `_` suffix on member vars.
- Every source file needs the MPL-2.0 license header (match the existing files in this directory — check one, e.g. `line_parser.cpp`'s header, and replicate it verbatim in new files).
- Qt5 only, not Qt6. `CMAKE_AUTOMOC` is already enabled at the top level.
- `DataStreamer` plugins must protect `dataMap()` access with `mutex()` — already done in `pushSamples()`, unaffected by this plan.
- Commit messages: Conventional Commits, Portuguese subject, scope `plugin` (matches this directory's existing commit history). No `Co-Authored-By` trailer.
- Build/test commands (from the `PlotJuggler` repo root, a `build/` directory already exists from prior work):
  ```sh
  cmake --build build --target test_rtt_data_map test_binary_parser test_csv_logger
  ctest --test-dir build -R "rtt_data_map|binary_parser|csv_logger" --output-on-failure
  ```

---

### Task 1: `RttDataMap` — JSON schema loader

**Files:**
- Create: `plotjuggler_plugins/DataStreamRTT/rtt_data_map.h`
- Create: `plotjuggler_plugins/DataStreamRTT/rtt_data_map.cpp`
- Create: `plotjuggler_plugins/DataStreamRTT/tests/test_rtt_data_map.cpp`
- Modify: `plotjuggler_plugins/DataStreamRTT/CMakeLists.txt`

**Interfaces:**
- Produces: `struct RttFieldDef { std::string name; size_t offset; std::string type; std::string unit; }`, `struct RttDataMap { size_t record_size; std::string seq_field; double nominal_rate_hz; std::vector<RttFieldDef> fields; static size_t typeSize(const std::string&); static RttDataMap loadFromJsonText(const std::string&); static RttDataMap loadFromFile(const std::string&); }` — both throw `std::runtime_error` on malformed input.

- [ ] **Step 1: Write the failing test file**

`plotjuggler_plugins/DataStreamRTT/tests/test_rtt_data_map.cpp`:

```cpp
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
```

- [ ] **Step 2: Add the CMake target for the new test (and library) so it can fail-to-compile first**

In `plotjuggler_plugins/DataStreamRTT/CMakeLists.txt`, add right before the existing `add_library(line_parser ...)` block:

```cmake
add_library(rtt_data_map STATIC rtt_data_map.cpp)
target_include_directories(rtt_data_map PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rtt_data_map PUBLIC Qt5::Core)
set_target_properties(rtt_data_map PROPERTIES POSITION_INDEPENDENT_CODE ON)
```

And inside the existing `if(BUILD_TESTING)` block, next to `add_executable(test_line_parser ...)`:

```cmake
  add_executable(test_rtt_data_map tests/test_rtt_data_map.cpp)
  target_link_libraries(test_rtt_data_map PRIVATE rtt_data_map GTest::gtest_main)
  add_test(NAME rtt_data_map COMMAND test_rtt_data_map)
```

- [ ] **Step 3: Run the test to verify it fails to build (files don't exist yet)**

Run: `cmake -S . -B build -DBUILD_TESTING=ON && cmake --build build --target test_rtt_data_map`
Expected: FAIL — `rtt_data_map.h`/`rtt_data_map.cpp` don't exist yet.

- [ ] **Step 4: Write `rtt_data_map.h`**

```cpp
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
```

- [ ] **Step 5: Write `rtt_data_map.cpp`**

```cpp
#include "rtt_data_map.h"

#include <algorithm>
#include <stdexcept>

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QString>

size_t RttDataMap::typeSize(const std::string& type)
{
  if (type == "uint8")
  {
    return 1;
  }
  if (type == "uint16")
  {
    return 2;
  }
  if (type == "uint32")
  {
    return 4;
  }
  if (type == "float32")
  {
    return 4;
  }
  throw std::runtime_error("RttDataMap: tipo desconhecido '" + type + "'");
}

RttDataMap RttDataMap::loadFromJsonText(const std::string& json_text)
{
  QJsonParseError parse_error;
  const QJsonDocument doc =
      QJsonDocument::fromJson(QByteArray::fromStdString(json_text), &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !doc.isObject())
  {
    throw std::runtime_error("RttDataMap: JSON invalido - " +
                              parse_error.errorString().toStdString());
  }
  const QJsonObject root = doc.object();

  if (!root.contains("record_size") || !root.contains("fields"))
  {
    throw std::runtime_error("RttDataMap: faltando 'record_size' ou 'fields'");
  }

  RttDataMap map;
  map.record_size = static_cast<size_t>(root.value("record_size").toInt());
  map.seq_field = root.value("seq_field").toString().toStdString();
  map.nominal_rate_hz = root.value("nominal_rate_hz").toDouble(1000.0);

  const QJsonArray fields = root.value("fields").toArray();
  for (const QJsonValue& field_value : fields)
  {
    const QJsonObject field_obj = field_value.toObject();
    if (!field_obj.contains("name") || !field_obj.contains("offset") ||
        !field_obj.contains("type"))
    {
      throw std::runtime_error("RttDataMap: campo sem 'name'/'offset'/'type'");
    }
    RttFieldDef field;
    field.name = field_obj.value("name").toString().toStdString();
    field.offset = static_cast<size_t>(field_obj.value("offset").toInt());
    field.type = field_obj.value("type").toString().toStdString();
    field.unit = field_obj.value("unit").toString().toStdString();

    const size_t type_size = typeSize(field.type);
    if (field.offset + type_size > map.record_size)
    {
      throw std::runtime_error("RttDataMap: campo '" + field.name +
                                "' ultrapassa record_size");
    }
    map.fields.push_back(std::move(field));
  }

  if (map.record_size == 0)
  {
    throw std::runtime_error("RttDataMap: record_size deve ser > 0");
  }

  if (!map.seq_field.empty())
  {
    const bool found =
        std::any_of(map.fields.begin(), map.fields.end(),
                    [&](const RttFieldDef& f) { return f.name == map.seq_field; });
    if (!found)
    {
      throw std::runtime_error("RttDataMap: seq_field '" + map.seq_field +
                                "' nao existe em 'fields'");
    }
  }

  return map;
}

RttDataMap RttDataMap::loadFromFile(const std::string& path)
{
  QFile file(QString::fromStdString(path));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    throw std::runtime_error("RttDataMap: nao foi possivel abrir '" + path + "'");
  }
  const QByteArray content = file.readAll();
  return loadFromJsonText(content.toStdString());
}
```

- [ ] **Step 6: Run the tests to verify they pass**

Run: `cmake --build build --target test_rtt_data_map && ./build/plotjuggler_plugins/DataStreamRTT/test_rtt_data_map`
Expected: `[  PASSED  ] 5 tests.`

- [ ] **Step 7: Commit**

```bash
git add plotjuggler_plugins/DataStreamRTT/rtt_data_map.h \
        plotjuggler_plugins/DataStreamRTT/rtt_data_map.cpp \
        plotjuggler_plugins/DataStreamRTT/tests/test_rtt_data_map.cpp \
        plotjuggler_plugins/DataStreamRTT/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(plugin): RttDataMap, carrega o schema binario da telemetria em JSON

Layout do registro (offsets/tipos/nomes/unidades) passa a vir de um
arquivo JSON em runtime em vez de hardcoded - uma mudanca de layout no
firmware vira edicao de JSON, nao rebuild do plugin.
EOF
)"
```

---

### Task 2: `TelemetrySample` extraction + `BinaryParser`

**Files:**
- Create: `plotjuggler_plugins/DataStreamRTT/telemetry_sample.h`
- Modify: `plotjuggler_plugins/DataStreamRTT/line_parser.h` (struct moves out, keeps working via include)
- Modify: `plotjuggler_plugins/DataStreamRTT/csv_logger.h` (include path only)
- Create: `plotjuggler_plugins/DataStreamRTT/binary_parser.h`
- Create: `plotjuggler_plugins/DataStreamRTT/binary_parser.cpp`
- Create: `plotjuggler_plugins/DataStreamRTT/tests/test_binary_parser.cpp`
- Modify: `plotjuggler_plugins/DataStreamRTT/CMakeLists.txt`

**Interfaces:**
- Consumes: `RttDataMap`, `RttFieldDef` (Task 1).
- Produces: `struct TelemetrySample` (moved, same shape as before: `double t`, `bool target_reset`, `std::vector<std::pair<std::string,double>> values`), `class BinaryParser { BinaryParser(RttDataMap); std::vector<TelemetrySample> feed(const char*, size_t); uint64_t malformedCount() const; uint64_t sequenceGaps() const; }`.

- [ ] **Step 1: Extract `TelemetrySample` into its own header**

Create `plotjuggler_plugins/DataStreamRTT/telemetry_sample.h`:

```cpp
#pragma once

#include <string>
#include <utility>
#include <vector>

struct TelemetrySample
{
  double t = 0.0;
  bool target_reset = false;
  std::vector<std::pair<std::string, double>> values;
};
```

In `line_parser.h`, replace the inline `struct TelemetrySample { ... };` definition (lines 8-13) with `#include "telemetry_sample.h"` — `class LineParser` keeps using `TelemetrySample` exactly as before, now from the shared header.

In `csv_logger.h`, replace `#include "line_parser.h"` with `#include "telemetry_sample.h"` (it only ever needed the struct, not `LineParser` itself).

- [ ] **Step 2: Build to confirm the extraction didn't break anything**

Run: `cmake --build build --target line_parser csv_logger test_line_parser test_csv_logger`
Expected: builds cleanly, `ctest --test-dir build -R "line_parser|csv_logger"` still passes (these existing tests are the regression check for this step — the struct moved, behavior didn't).

- [ ] **Step 3: Write the failing test file for `BinaryParser`**

`plotjuggler_plugins/DataStreamRTT/tests/test_binary_parser.cpp`:

```cpp
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
```

- [ ] **Step 4: Add CMake targets, verify the test fails to build**

In `CMakeLists.txt`, next to the `rtt_data_map` library block added in Task 1:

```cmake
add_library(binary_parser STATIC binary_parser.cpp)
target_include_directories(binary_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(binary_parser PUBLIC rtt_data_map)
set_target_properties(binary_parser PROPERTIES POSITION_INDEPENDENT_CODE ON)
```

And in the `if(BUILD_TESTING)` block:

```cmake
  add_executable(test_binary_parser tests/test_binary_parser.cpp)
  target_link_libraries(test_binary_parser PRIVATE binary_parser GTest::gtest_main)
  add_test(NAME binary_parser COMMAND test_binary_parser)
```

Run: `cmake -S . -B build -DBUILD_TESTING=ON && cmake --build build --target test_binary_parser`
Expected: FAIL — `binary_parser.h`/`binary_parser.cpp` don't exist yet.

- [ ] **Step 5: Write `binary_parser.h`**

```cpp
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
```

- [ ] **Step 6: Write `binary_parser.cpp`**

```cpp
#include "binary_parser.h"

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
    out.t = _map.nominal_rate_hz > 0.0 ? static_cast<double>(seq) / _map.nominal_rate_hz : 0.0;
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
```

- [ ] **Step 7: Run the tests to verify they pass**

Run: `cmake --build build --target test_binary_parser && ./build/plotjuggler_plugins/DataStreamRTT/test_binary_parser`
Expected: `[  PASSED  ] 5 tests.`

- [ ] **Step 8: Commit**

```bash
git add plotjuggler_plugins/DataStreamRTT/telemetry_sample.h \
        plotjuggler_plugins/DataStreamRTT/line_parser.h \
        plotjuggler_plugins/DataStreamRTT/csv_logger.h \
        plotjuggler_plugins/DataStreamRTT/binary_parser.h \
        plotjuggler_plugins/DataStreamRTT/binary_parser.cpp \
        plotjuggler_plugins/DataStreamRTT/tests/test_binary_parser.cpp \
        plotjuggler_plugins/DataStreamRTT/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(plugin): BinaryParser, decodifica registros pelo mapa de dados

TelemetrySample extraido de line_parser.h para telemetry_sample.h
(compartilhado, sem depender de LineParser). BinaryParser::feed() tem
a mesma responsabilidade de LineParser::feed() - bytes crus entram,
TelemetrySample sai - pra nao precisar mudar pushSamples()/CsvLogger.
Tempo computado por seq/nominal_rate_hz, imune a jitter de chegada.
EOF
)"
```

---

### Task 3: Wire `BinaryParser` into `datastream_rtt`

**Files:**
- Modify: `plotjuggler_plugins/DataStreamRTT/datastream_rtt.h`
- Modify: `plotjuggler_plugins/DataStreamRTT/datastream_rtt.cpp`

**Interfaces:**
- Consumes: `RttDataMap::loadFromFile(const std::string&)` (Task 1), `BinaryParser` (Task 2).

- [ ] **Step 1: Update `datastream_rtt.h`**

Replace:
```cpp
#include "PlotJuggler/datastreamer_base.h"
#include "csv_logger.h"
#include "line_parser.h"
```
with:
```cpp
#include <memory>

#include "PlotJuggler/datastreamer_base.h"
#include "binary_parser.h"
#include "csv_logger.h"
#include "rtt_data_map.h"
```

Replace the private member `LineParser _parser;` with:
```cpp
  std::unique_ptr<BinaryParser> _parser;
  RttDataMap _data_map;
  QString _data_map_path;
```

- [ ] **Step 2: Add the data-map file picker to the connect dialog (`start()`)**

In `datastream_rtt.cpp::start()`, right after the existing `csv_dir_edit`/`csv_browse` block (before `layout->addRow(buttons);`), add:

```cpp
  auto* map_path_edit = new QLineEdit(settings.value("DataStreamRTT/data_map_path", "").toString());
  auto* map_browse = new QPushButton("...");
  QObject::connect(map_browse, &QPushButton::clicked, [&]() {
    const QString path = QFileDialog::getOpenFileName(&dialog, "Mapa de dados (JSON)", QString(),
                                                        "JSON (*.json)");
    if (!path.isEmpty())
    {
      map_path_edit->setText(path);
    }
  });
  auto* map_row = new QHBoxLayout();
  map_row->addWidget(map_path_edit);
  map_row->addWidget(map_browse);
  layout->addRow("Mapa de dados (JSON)", map_row);
```

Then, after the existing `_csv_logger.setDirectory(...)` line and before `_running = true;`, replace the direct `_running = true;` transition with a load attempt:

```cpp
  _data_map_path = map_path_edit->text();
  try
  {
    _data_map = RttDataMap::loadFromFile(_data_map_path.toStdString());
  }
  catch (const std::exception& e)
  {
    QMessageBox::warning(nullptr, "RTT Streamer",
                          QString("Falha ao carregar o mapa de dados:\n%1").arg(e.what()));
    return false;
  }
  settings.setValue("DataStreamRTT/data_map_path", _data_map_path);

  _running = true;
```

(remove the old standalone `_running = true;` line that followed `_csv_logger.setDirectory(...)` — there should be exactly one now, right after the data-map load succeeds).

- [ ] **Step 3: Rebuild the parser on every (re)connect**

In `connectToServer()`, replace:
```cpp
  _parser = LineParser();
```
with:
```cpp
  _parser = std::make_unique<BinaryParser>(_data_map);
```

- [ ] **Step 4: Update the two call sites that use `_parser`**

In `onReadyRead()`, replace:
```cpp
  const auto samples = _parser.feed(data.constData(), static_cast<size_t>(data.size()));
```
with:
```cpp
  const auto samples = _parser->feed(data.constData(), static_cast<size_t>(data.size()));
```

In `pushSamples()`, replace:
```cpp
      it->second.pushBack(
          PJ::PlotData::Point(sample.t, static_cast<double>(_parser.malformedCount())));
```
with:
```cpp
      it->second.pushBack(
          PJ::PlotData::Point(sample.t, static_cast<double>(_parser->malformedCount())));
```

- [ ] **Step 5: Persist the data-map path in save/load state**

In `xmlSaveState()`, add next to the existing `elem.setAttribute(...)` calls:
```cpp
  elem.setAttribute("data_map_path", _data_map_path);
```

In `xmlLoadState()`, add next to the existing `settings.setValue(...)` calls:
```cpp
  settings.setValue("DataStreamRTT/data_map_path", elem.attribute("data_map_path", ""));
```

- [ ] **Step 6: Update `CMakeLists.txt`'s `DataStreamRTT` target**

Replace:
```cmake
target_link_libraries(DataStreamRTT PRIVATE Qt5::Widgets Qt5::Network Qt5::Xml
                                            plotjuggler_base line_parser csv_logger)
```
with:
```cmake
target_link_libraries(DataStreamRTT PRIVATE Qt5::Widgets Qt5::Network Qt5::Xml
                                            plotjuggler_base binary_parser rtt_data_map csv_logger)
```

- [ ] **Step 7: Build the plugin**

Run: `cmake --build build --target DataStreamRTT`
Expected: builds cleanly (this is a Qt plugin target — no unit test to run here; the behavior is exercised end-to-end in Task 5's bench validation).

- [ ] **Step 8: Commit**

```bash
git add plotjuggler_plugins/DataStreamRTT/datastream_rtt.h \
        plotjuggler_plugins/DataStreamRTT/datastream_rtt.cpp \
        plotjuggler_plugins/DataStreamRTT/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(plugin): datastream_rtt usa BinaryParser, novo campo de mapa JSON

Dialogo de conexao ganha um seletor de arquivo pro mapa de dados;
LineParser sai (nenhum firmware emite mais o formato texto). Parser e'
reconstruido a cada (re)conexao a partir do RttDataMap ja carregado,
mesmo comportamento de reset de estado que LineParser tinha.
EOF
)"
```

---

### Task 4: Remove `LineParser`

**Files:**
- Delete: `plotjuggler_plugins/DataStreamRTT/line_parser.h`, `plotjuggler_plugins/DataStreamRTT/line_parser.cpp`, `plotjuggler_plugins/DataStreamRTT/tests/test_line_parser.cpp`
- Modify: `plotjuggler_plugins/DataStreamRTT/CMakeLists.txt`

**Interfaces:** none (confirmed nothing references `LineParser` after Task 3).

- [ ] **Step 1: Confirm nothing else references `LineParser`**

Run: `grep -rn "LineParser\|line_parser" plotjuggler_plugins/DataStreamRTT/ --include=*.h --include=*.cpp --include=CMakeLists.txt`
Expected: only the files being deleted in this task, plus the `add_library(line_parser ...)`/`add_executable(test_line_parser ...)`/`add_test(NAME line_parser ...)` lines in `CMakeLists.txt`. If anything else shows up, stop and investigate.

- [ ] **Step 2: Delete the files**

```bash
git rm plotjuggler_plugins/DataStreamRTT/line_parser.h \
       plotjuggler_plugins/DataStreamRTT/line_parser.cpp \
       plotjuggler_plugins/DataStreamRTT/tests/test_line_parser.cpp
```

- [ ] **Step 3: Remove the CMake entries**

In `CMakeLists.txt`, remove the `add_library(line_parser ...)` block (3 lines) and, inside `if(BUILD_TESTING)`, the `add_executable(test_line_parser ...)` / `target_link_libraries(test_line_parser ...)` / `add_test(NAME line_parser ...)` lines.

- [ ] **Step 4: Full test suite run**

Run: `cmake -S . -B build -DBUILD_TESTING=ON && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: all remaining tests pass (`rtt_data_map`, `binary_parser`, `csv_logger`) — no `line_parser` target exists anymore.

- [ ] **Step 5: Commit**

```bash
git add -A plotjuggler_plugins/DataStreamRTT/line_parser.h \
           plotjuggler_plugins/DataStreamRTT/line_parser.cpp \
           plotjuggler_plugins/DataStreamRTT/tests/test_line_parser.cpp \
           plotjuggler_plugins/DataStreamRTT/CMakeLists.txt
git commit -m "$(cat <<'EOF'
fix(plugin): remove LineParser, BinaryParser e' o unico parser

Nenhum firmware do projeto emite mais o formato texto key=value -
manter os dois parsers seria so' manutencao de codigo morto.
EOF
)"
```

---

### Task 5: End-to-end bench validation (bancada, manual)

**Files:** none — validation only, no code changes. Depends on the companion firmware plan (`docs/superpowers/plans/2026-07-25-rtt-binary-telemetry-firmware.md` in `TCC_Firmware`) having reached at least its Task 2 (telemetry emitting the binary record).

- [ ] **Step 1: Connect the plugin to the real firmware**

With the firmware flashed and the J-Link GDB Server running, open PlotJuggler, start `RTT Streamer`, and point the new "Mapa de dados (JSON)" field at `TCC_Firmware`'s `docs/rtt/tcc_firmware_rtt_map.json`.
Expected: series appear named exactly as in the map (`rtt/seq`, `rtt/ib`, `rtt/ic`, `rtt/vbus`, `rtt/encAngle`, `rtt/dutyA`, ... — the `rtt/` prefix comes from `pushSamples()`, unchanged).

- [ ] **Step 2: Confirm no gaps/malformed records over a multi-minute session**

Watch `rtt/_parse_errors` (already wired in `pushSamples()`, now backed by `BinaryParser::malformedCount()`) and `rtt/seq` for a few minutes of normal operation.
Expected: `_parse_errors` stays at 0, `seq` increments by 1 every sample with no visible gaps in the plotted signal.

- [ ] **Step 3: Confirm reset detection**

Reset the MCU while connected. Expected: `clearBuffers()` fires (buffers visibly clear in the PlotJuggler UI), matching the old `LineParser`-based reset behavior.

- [ ] **Step 4: Re-run the 10 Hz sinusoidal V/f bench test**

With the same test from the firmware plan's Task 5, confirm `dutyA/B/C`/`voltA/B/C` plot cleanly without aliasing artifacts.

- [ ] **Step 5: Commit any fixture/evidence updates**

If bench testing surfaces fixture updates (e.g. `tests/data/capture_spike.txt`, already modified in the working tree from prior work — check `git status` before including it, don't bundle unrelated changes):
```bash
git add plotjuggler_plugins/DataStreamRTT/tests/
git commit -m "$(cat <<'EOF'
test(plugin): atualiza fixtures apos validacao end-to-end binaria
EOF
)"
```
