/* Plugin DataStreamer do PlotJuggler: le telemetria do canal RTT via a
   porta telnet TCP do J-Link GDB Server (padrao localhost:19021). */
#pragma once

#include <QTcpSocket>
#include <QTimer>
#include <QtPlugin>

#include "PlotJuggler/datastreamer_base.h"
#include "csv_logger.h"
#include "line_parser.h"

class DataStreamRTT : public PJ::DataStreamer
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "facontidavide.PlotJuggler3.DataStreamer")
  Q_INTERFACES(PJ::DataStreamer)

public:
  DataStreamRTT();

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

private slots:
  void onConnected();
  void onReadyRead();
  void onSocketClosed();

private:
  void connectToServer();
  void pushSamples(const std::vector<TelemetrySample>& samples);

  QTcpSocket* _socket = nullptr;
  QTimer* _reconnect_timer = nullptr;
  LineParser _parser;
  CsvLogger _csv_logger;
  bool _running = false;
  bool _warned_once = false;
  QString _host;
  int _port = 19021;
  int _channel = 1;
};
