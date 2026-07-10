#include "datastream_rtt.h"

#include <QMessageBox>
#include <QSettings>

DataStreamRTT::DataStreamRTT()
{
  _socket = new QTcpSocket(this);
  _reconnect_timer = new QTimer(this);
  _reconnect_timer->setInterval(1000);
  _reconnect_timer->setSingleShot(true);

  connect(_socket, &QTcpSocket::connected, this, &DataStreamRTT::onConnected);
  connect(_socket, &QTcpSocket::readyRead, this, &DataStreamRTT::onReadyRead);
  connect(_socket, &QTcpSocket::disconnected, this, &DataStreamRTT::onSocketClosed);
  connect(_socket, &QTcpSocket::errorOccurred, this, &DataStreamRTT::onSocketClosed);
  connect(_reconnect_timer, &QTimer::timeout, this, &DataStreamRTT::connectToServer);
}

DataStreamRTT::~DataStreamRTT()
{
  shutdown();
}

bool DataStreamRTT::start(QStringList*)
{
  QSettings settings;
  _host = settings.value("DataStreamRTT/host", "127.0.0.1").toString();
  _port = settings.value("DataStreamRTT/port", 19021).toInt();
  _channel = settings.value("DataStreamRTT/channel", 1).toInt();

  const bool csv_enabled = settings.value("DataStreamRTT/csv_enabled", false).toBool();
  const QString csv_dir = settings.value("DataStreamRTT/csv_dir", "").toString();
  _csv_logger.setDirectory(csv_enabled ? csv_dir.toStdString() : std::string());

  _running = true;
  _warned_once = false;
  connectToServer();
  return true;
}

void DataStreamRTT::shutdown()
{
  _running = false;
  _reconnect_timer->stop();
  _csv_logger.close();
  _socket->abort();
}

void DataStreamRTT::connectToServer()
{
  if (!_running)
  {
    return;
  }
  _parser = LineParser();
  _socket->abort();
  _socket->connectToHost(_host, static_cast<quint16>(_port));
}

void DataStreamRTT::onConnected()
{
  // A config string precisa chegar em ate 100 ms apos o connect.
  const QByteArray cfg =
      "$$SEGGER_TELNET_ConfigStr=RTTCh;" + QByteArray::number(_channel) + "$$";
  _socket->write(cfg);
}

void DataStreamRTT::onSocketClosed()
{
  if (!_running)
  {
    return;
  }
  _reconnect_timer->start();
  if (!_warned_once)
  {
    _warned_once = true;
    auto* box = new QMessageBox(
        QMessageBox::Information, "RTT Streamer",
        QString("Sem conexao com %1:%2 (o GDB server do J-Link esta rodando?).\n"
                "Vou tentar reconectar a cada segundo em segundo plano.")
            .arg(_host)
            .arg(_port));
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->show();
  }
}

void DataStreamRTT::onReadyRead()
{
  const QByteArray data = _socket->readAll();
  const auto samples = _parser.feed(data.constData(), static_cast<size_t>(data.size()));
  if (!samples.empty())
  {
    pushSamples(samples);
  }
}

void DataStreamRTT::pushSamples(const std::vector<TelemetrySample>& samples)
{
  bool reset_seen = false;
  {
    std::lock_guard<std::mutex> lock(mutex());
    for (const auto& sample : samples)
    {
      _csv_logger.onSample(sample);
      if (sample.target_reset)
      {
        reset_seen = true;
      }
      for (const auto& [key, value] : sample.values)
      {
        const std::string series_name = "rtt/" + key;
        auto it = dataMap().numeric.find(series_name);
        if (it == dataMap().numeric.end())
        {
          it = dataMap().addNumeric(series_name);
        }
        it->second.pushBack(PJ::PlotData::Point(sample.t, value));
      }
      const std::string err_name = "rtt/_parse_errors";
      auto it = dataMap().numeric.find(err_name);
      if (it == dataMap().numeric.end())
      {
        it = dataMap().addNumeric(err_name);
      }
      it->second.pushBack(
          PJ::PlotData::Point(sample.t, static_cast<double>(_parser.malformedCount())));
    }
  }
  if (reset_seen)
  {
    emit clearBuffers();
  }
  emit dataReceived();
}

bool DataStreamRTT::xmlSaveState(QDomDocument&, QDomElement&) const
{
  return true;
}

bool DataStreamRTT::xmlLoadState(const QDomElement&)
{
  return true;
}
