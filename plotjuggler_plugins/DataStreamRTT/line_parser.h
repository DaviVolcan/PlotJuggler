#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct TelemetrySample
{
  double t = 0.0;
  bool target_reset = false;
  std::vector<std::pair<std::string, double>> values;
};

class LineParser
{
public:
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
  bool parseLine(const std::string& line, TelemetrySample& out);

  std::string _buffer;
  double _last_t = -1.0;
  int64_t _last_n = -1;
  uint64_t _malformed = 0;
  uint64_t _seq_gaps = 0;
};
