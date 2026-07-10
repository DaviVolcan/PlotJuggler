#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "csv_logger.h"

namespace fs = std::filesystem;

static TelemetrySample makeSample(double t, double ia, bool reset = false)
{
  TelemetrySample s;
  s.t = t;
  s.target_reset = reset;
  s.values = { { "n", t * 1000 }, { "ia", ia } };
  return s;
}

static std::string readFile(const std::string& path)
{
  std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

class CsvLoggerTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    _dir = fs::temp_directory_path() / "csv_logger_test";
    fs::remove_all(_dir);
    fs::create_directories(_dir);
  }
  void TearDown() override
  {
    fs::remove_all(_dir);
  }
  fs::path _dir;
};

TEST_F(CsvLoggerTest, WritesHeaderFromFirstSampleAndRows)
{
  CsvLogger logger;
  logger.setDirectory(_dir.string());
  logger.onSample(makeSample(0.001, 0.5));
  logger.onSample(makeSample(0.002, 0.6));
  logger.close();

  const std::string content = readFile(logger.currentFile());
  EXPECT_EQ(content, "t,n,ia\n0.001000,1,0.5\n0.002000,2,0.6\n");
}

TEST_F(CsvLoggerTest, RotatesFileOnTargetReset)
{
  CsvLogger logger;
  logger.setDirectory(_dir.string());
  logger.onSample(makeSample(10.0, 0.5));
  const std::string first_file = logger.currentFile();
  logger.onSample(makeSample(0.001, 0.7, /*reset=*/true));
  logger.close();

  EXPECT_NE(logger.currentFile(), first_file);
  EXPECT_EQ(std::distance(fs::directory_iterator(_dir), fs::directory_iterator{}), 2);
}

TEST_F(CsvLoggerTest, MissingKeyBecomesEmptyCell)
{
  CsvLogger logger;
  logger.setDirectory(_dir.string());
  logger.onSample(makeSample(0.001, 0.5));
  TelemetrySample incomplete;
  incomplete.t = 0.002;
  incomplete.values = { { "n", 2 } };  // sem "ia"
  logger.onSample(incomplete);
  logger.close();

  const std::string content = readFile(logger.currentFile());
  EXPECT_EQ(content, "t,n,ia\n0.001000,1,0.5\n0.002000,2,\n");
}

TEST_F(CsvLoggerTest, DisabledWhenDirectoryEmpty)
{
  CsvLogger logger;
  logger.onSample(makeSample(0.001, 0.5));
  logger.close();
  EXPECT_TRUE(logger.currentFile().empty());
}
