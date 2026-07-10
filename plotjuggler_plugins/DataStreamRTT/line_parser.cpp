#include "line_parser.h"

#include <cstdlib>

std::vector<TelemetrySample> LineParser::feed(const char* data, size_t len)
{
  _buffer.append(data, len);
  std::vector<TelemetrySample> out;
  size_t pos;
  while ((pos = _buffer.find('\n')) != std::string::npos)
  {
    std::string line = _buffer.substr(0, pos);
    _buffer.erase(0, pos + 1);
    if (!line.empty() && line.back() == '\r')
    {
      line.pop_back();
    }
    if (line.empty())
    {
      continue;
    }
    TelemetrySample sample;
    if (parseLine(line, sample))
    {
      out.push_back(std::move(sample));
    }
    else
    {
      _malformed++;
    }
  }
  return out;
}

bool LineParser::parseLine(const std::string& line, TelemetrySample& out)
{
  bool has_t = false;
  bool has_n = false;
  double n_value = 0.0;

  size_t start = 0;
  while (start < line.size())
  {
    size_t comma = line.find(',', start);
    if (comma == std::string::npos)
    {
      comma = line.size();
    }
    const size_t eq = line.find('=', start);
    if (eq == std::string::npos || eq >= comma || eq == start)
    {
      return false;
    }
    std::string key = line.substr(start, eq - start);
    const std::string value_str = line.substr(eq + 1, comma - eq - 1);
    char* end = nullptr;
    const double value = std::strtod(value_str.c_str(), &end);
    if (end == value_str.c_str() || *end != '\0')
    {
      return false;
    }
    if (key == "t")
    {
      out.t = value;
      has_t = true;
    }
    else
    {
      if (key == "n")
      {
        n_value = value;
        has_n = true;
      }
      out.values.emplace_back(std::move(key), value);
    }
    start = comma + 1;
  }

  if (!has_t)
  {
    return false;
  }
  if (_last_t >= 0.0 && out.t < _last_t)
  {
    out.target_reset = true;
    _last_n = -1;  // reboot zera o rastreio de sequencia
  }
  _last_t = out.t;
  if (has_n)
  {
    const int64_t n = static_cast<int64_t>(n_value);
    if (_last_n >= 0 && n > _last_n + 1)
    {
      _seq_gaps += static_cast<uint64_t>(n - _last_n - 1);
    }
    _last_n = n;
  }
  return true;
}
