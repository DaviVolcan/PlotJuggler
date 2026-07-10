/* Grava cada ensaio de telemetria num CSV proprio; target_reset rotaciona. */
#pragma once

#include <fstream>
#include <string>
#include <vector>

#include "line_parser.h"

class CsvLogger
{
public:
  void setDirectory(const std::string& dir);

  void onSample(const TelemetrySample& sample);

  void close();

  const std::string& currentFile() const
  {
    return _current_file;
  }

private:
  void openNewFile(const TelemetrySample& first);
  static std::string formatValue(double value);

  std::string _dir;
  std::string _current_file;
  std::ofstream _file;
  std::vector<std::string> _header;
};
