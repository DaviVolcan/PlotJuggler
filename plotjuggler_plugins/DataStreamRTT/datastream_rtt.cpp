#include "datastream_rtt.h"

DataStreamRTT::~DataStreamRTT()
{
  shutdown();
}

bool DataStreamRTT::start(QStringList*)
{
  _running = true;
  return true;
}

void DataStreamRTT::shutdown()
{
  _running = false;
}

bool DataStreamRTT::xmlSaveState(QDomDocument&, QDomElement&) const
{
  return true;
}

bool DataStreamRTT::xmlLoadState(const QDomElement&)
{
  return true;
}
