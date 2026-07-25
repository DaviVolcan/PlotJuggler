/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "rtt_data_map.h"

#include <algorithm>
#include <stdexcept>

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QString>

size_t RttDataMap::typeSize(const std::string& type)
{
  if (type == "uint8")
  {
    return 1;
  }
  if (type == "uint16")
  {
    return 2;
  }
  if (type == "uint32")
  {
    return 4;
  }
  if (type == "float32")
  {
    return 4;
  }
  throw std::runtime_error("RttDataMap: tipo desconhecido '" + type + "'");
}

RttDataMap RttDataMap::loadFromJsonText(const std::string& json_text)
{
  QJsonParseError parse_error;
  const QJsonDocument doc =
      QJsonDocument::fromJson(QByteArray::fromStdString(json_text), &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !doc.isObject())
  {
    throw std::runtime_error("RttDataMap: JSON invalido - " +
                              parse_error.errorString().toStdString());
  }
  const QJsonObject root = doc.object();

  if (!root.contains("record_size") || !root.contains("fields"))
  {
    throw std::runtime_error("RttDataMap: faltando 'record_size' ou 'fields'");
  }

  if (root.contains("byte_order") && root.value("byte_order").toString() != "little")
  {
    throw std::runtime_error("RttDataMap: byte_order so' suporta 'little' (decode e' memcpy "
                              "direto, sem conversao de endianness)");
  }

  RttDataMap map;
  const int record_size_int = root.value("record_size").toInt();
  if (record_size_int < 0)
  {
    throw std::runtime_error("RttDataMap: record_size nao pode ser negativo");
  }
  map.record_size = static_cast<size_t>(record_size_int);
  map.seq_field = root.value("seq_field").toString().toStdString();
  map.nominal_rate_hz = root.value("nominal_rate_hz").toDouble(1000.0);

  if (!root.value("fields").isArray())
  {
    throw std::runtime_error("RttDataMap: 'fields' precisa ser um array");
  }
  const QJsonArray fields = root.value("fields").toArray();
  for (const QJsonValue& field_value : fields)
  {
    const QJsonObject field_obj = field_value.toObject();
    if (!field_obj.contains("name") || !field_obj.contains("offset") ||
        !field_obj.contains("type"))
    {
      throw std::runtime_error("RttDataMap: campo sem 'name'/'offset'/'type'");
    }
    RttFieldDef field;
    field.name = field_obj.value("name").toString().toStdString();
    const int offset_int = field_obj.value("offset").toInt();
    if (offset_int < 0)
    {
      throw std::runtime_error("RttDataMap: campo '" + field.name +
                                "' tem offset negativo");
    }
    field.offset = static_cast<size_t>(offset_int);
    field.type = field_obj.value("type").toString().toStdString();
    field.unit = field_obj.value("unit").toString().toStdString();

    const size_t type_size = typeSize(field.type);
    if (field.offset + type_size > map.record_size)
    {
      throw std::runtime_error("RttDataMap: campo '" + field.name +
                                "' ultrapassa record_size");
    }
    map.fields.push_back(std::move(field));
  }

  if (map.record_size == 0)
  {
    throw std::runtime_error("RttDataMap: record_size deve ser > 0");
  }

  if (!map.seq_field.empty())
  {
    const bool found =
        std::any_of(map.fields.begin(), map.fields.end(),
                    [&](const RttFieldDef& f) { return f.name == map.seq_field; });
    if (!found)
    {
      throw std::runtime_error("RttDataMap: seq_field '" + map.seq_field +
                                "' nao existe em 'fields'");
    }
  }

  return map;
}

RttDataMap RttDataMap::loadFromFile(const std::string& path)
{
  QFile file(QString::fromStdString(path));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    throw std::runtime_error("RttDataMap: nao foi possivel abrir '" + path + "'");
  }
  const QByteArray content = file.readAll();
  return loadFromJsonText(content.toStdString());
}
