#include "datastream_rtt.h"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>

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
  QDialog dialog;
  dialog.setWindowTitle("RTT Streamer");
  auto* layout = new QFormLayout(&dialog);

  auto* host_edit = new QLineEdit(settings.value("DataStreamRTT/host", "127.0.0.1").toString());
  auto* port_spin = new QSpinBox();
  port_spin->setRange(1, 65535);
  port_spin->setValue(settings.value("DataStreamRTT/port", 2334).toInt());
  auto* channel_spin = new QSpinBox();
  channel_spin->setRange(0, 15);
  channel_spin->setValue(settings.value("DataStreamRTT/channel", 2).toInt());
  auto* csv_check = new QCheckBox("Gravar CSV do ensaio");
  csv_check->setChecked(settings.value("DataStreamRTT/csv_enabled", false).toBool());
  auto* csv_dir_edit = new QLineEdit(settings.value("DataStreamRTT/csv_dir", "").toString());
  auto* csv_browse = new QPushButton("...");
  QObject::connect(csv_browse, &QPushButton::clicked, [&]() {
    const QString dir = QFileDialog::getExistingDirectory(&dialog, "Diretorio dos CSV");
    if (!dir.isEmpty())
    {
      csv_dir_edit->setText(dir);
    }
  });
  auto* dir_row = new QHBoxLayout();
  dir_row->addWidget(csv_dir_edit);
  dir_row->addWidget(csv_browse);

  auto* map_path_edit = new QLineEdit(settings.value("DataStreamRTT/data_map_path", "").toString());
  auto* map_browse = new QPushButton("...");
  QObject::connect(map_browse, &QPushButton::clicked, [&]() {
    const QString path =
        QFileDialog::getOpenFileName(&dialog, "Mapa de dados (JSON)", QString(), "JSON (*.json)");
    if (!path.isEmpty())
    {
      map_path_edit->setText(path);
    }
  });
  auto* map_row = new QHBoxLayout();
  map_row->addWidget(map_path_edit);
  map_row->addWidget(map_browse);

  layout->addRow("Host", host_edit);
  layout->addRow("Porta", port_spin);
  layout->addRow("Canal RTT", channel_spin);
  layout->addRow(csv_check);
  layout->addRow("Diretorio CSV", dir_row);
  layout->addRow("Mapa de dados (JSON)", map_row);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addRow(buttons);

  if (dialog.exec() != QDialog::Accepted)
  {
    return false;
  }

  _host = host_edit->text();
  _port = port_spin->value();
  _channel = channel_spin->value();
  const bool csv_enabled = csv_check->isChecked();
  const QString csv_dir = csv_dir_edit->text();

  if (csv_enabled && (csv_dir.isEmpty() || !QDir().mkpath(csv_dir)))
  {
    QMessageBox::warning(
        nullptr, "RTT Streamer",
        "Gravacao CSV habilitada mas o diretorio e invalido ou nao pode ser criado.\n"
        "Configure um diretorio valido e inicie de novo.");
    return false;
  }

  settings.setValue("DataStreamRTT/host", _host);
  settings.setValue("DataStreamRTT/port", _port);
  settings.setValue("DataStreamRTT/channel", _channel);
  settings.setValue("DataStreamRTT/csv_enabled", csv_enabled);
  settings.setValue("DataStreamRTT/csv_dir", csv_dir);

  _csv_logger.setDirectory(csv_enabled ? csv_dir.toStdString() : std::string());

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
  _parser = std::make_unique<BinaryParser>(_data_map);
  _csv_logger.close();
  _socket->abort();
  _socket->connectToHost(_host, static_cast<quint16>(_port));
}

void DataStreamRTT::onConnected()
{
  // A config string precisa chegar em ate 100 ms apos o connect.
  const QByteArray cfg = "$$SEGGER_TELNET_ConfigStr=RTTCh;" + QByteArray::number(_channel) + "$$";
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
    auto* box =
        new QMessageBox(QMessageBox::Information, "RTT Streamer",
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
  feedParser(data.constData(), static_cast<size_t>(data.size()));
}

void DataStreamRTT::feedParser(const char* data, size_t len)
{
  if (len == 0)
  {
    return;
  }
  // Nao ha mais tratamento especial do banner de texto do GDB Server: a
  // magic no inicio de cada registro (ver Core/Inc/telemetry.h) enquadra
  // o stream sozinha, e o banner simplesmente nao casa com ela. A antiga
  // heuristica de texto era indecidivel na raiz - nao da para distinguir
  // o '\n' final do banner de um 0x0A que comeca o payload binario.
  const auto samples = _parser->feed(data, len);
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
          PJ::PlotData::Point(sample.t, static_cast<double>(_parser->malformedCount())));
    }
  }
  if (reset_seen)
  {
    emit clearBuffers();
  }
  emit dataReceived();
}

bool DataStreamRTT::xmlSaveState(QDomDocument& doc, QDomElement& parent_element) const
{
  QDomElement elem = doc.createElement("rtt_streamer");
  elem.setAttribute("host", _host);
  elem.setAttribute("port", _port);
  elem.setAttribute("channel", _channel);
  elem.setAttribute("data_map_path", _data_map_path);
  parent_element.appendChild(elem);
  return true;
}

bool DataStreamRTT::xmlLoadState(const QDomElement& parent_element)
{
  const QDomElement elem = parent_element.firstChildElement("rtt_streamer");
  if (elem.isNull())
  {
    return false;
  }
  QSettings settings;
  settings.setValue("DataStreamRTT/host", elem.attribute("host", "127.0.0.1"));
  settings.setValue("DataStreamRTT/port", elem.attribute("port", "2334").toInt());
  settings.setValue("DataStreamRTT/channel", elem.attribute("channel", "2").toInt());
  settings.setValue("DataStreamRTT/data_map_path", elem.attribute("data_map_path", ""));
  return true;
}
