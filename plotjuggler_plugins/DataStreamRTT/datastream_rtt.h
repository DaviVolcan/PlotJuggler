/* Plugin DataStreamer do PlotJuggler: le telemetria do canal RTT via a
   porta telnet TCP do J-Link GDB Server (padrao localhost:19021). */
#pragma once

#include <QtPlugin>

#include "PlotJuggler/datastreamer_base.h"

class DataStreamRTT : public PJ::DataStreamer
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "facontidavide.PlotJuggler3.DataStreamer")
  Q_INTERFACES(PJ::DataStreamer)

public:
  DataStreamRTT() = default;

  ~DataStreamRTT() override;

  bool start(QStringList*) override;

  void shutdown() override;

  bool isRunning() const override
  {
    return _running;
  }

  const char* name() const override
  {
    return "RTT Streamer";
  }

  bool xmlSaveState(QDomDocument& doc, QDomElement& parent_element) const override;

  bool xmlLoadState(const QDomElement& parent_element) override;

private:
  bool _running = false;
};
