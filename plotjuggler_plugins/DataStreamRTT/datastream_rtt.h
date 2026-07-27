/* Plugin DataStreamer do PlotJuggler: le telemetria do canal RTT via a
   porta telnet TCP do J-Link GDB Server (localhost:2334 neste
   projeto, ver .idea/debugServers/). */
#pragma once

#include <QTcpSocket>
#include <QTimer>
#include <QtPlugin>

#include <memory>
#include <string>

#include "PlotJuggler/datastreamer_base.h"
#include "binary_parser.h"
#include "csv_logger.h"
#include "rtt_data_map.h"

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
  void feedParser(const char* data, size_t len);

  QTcpSocket* _socket = nullptr;
  QTimer* _reconnect_timer = nullptr;
  std::unique_ptr<BinaryParser> _parser;
  RttDataMap _data_map;
  QString _data_map_path;
  CsvLogger _csv_logger;
  bool _running = false;
  bool _warned_once = false;
  QString _host;
  // Porta telnet do GDB Server. 2334 e' a configurada nos debug servers
  // deste projeto (-RTTTelnetPort 2334), nao a default 19021 da SEGGER.
  int _port = 2334;
  // Canal RTT da telemetria (RTT_TRANSPORT_CHANNEL no firmware). O canal
  // 1 e' do SystemView e o 0 e' o Terminal - apontar para eles nao rende
  // telemetria nenhuma.
  int _channel = 2;
};
