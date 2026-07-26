/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#pragma once

#include <cstddef>
#include <string>

// O GDB Server do J-Link manda um banner de texto (versao, numero de serie,
// nome do processo) logo apos o connect, antes dos dados binarios do canal
// RTT comecarem a fluir - independente do canal selecionado pelo
// SEGGER_TELNET_ConfigStr. Sem pular esse banner, BinaryParser::feed()
// desalinha permanentemente (o banner nao e' multiplo do tamanho do
// registro).
namespace RttBanner
{
// Quantos bytes aceitar como "ainda pode ser banner" antes de desistir de
// esperar por uma quebra de linha (protege contra um bridge RTT que nunca
// manda banner nenhum - nesse caso os dados binarios ja comecam de cara e
// vao conter bytes nao-imprimiveis bem cedo, entao esse limite quase nunca
// e' atingido na pratica).
constexpr size_t kMaxBannerScan = 512;

// Procura o fim de um banner de texto ASCII (linhas terminadas em '\n') no
// inicio de buffer. Retorno:
//  - 0 se o buffer ja comeca binario (sem banner a pular);
//  - o offset logo apos o ultimo '\n' antes do primeiro byte nao-texto, se um
//    banner foi encontrado;
//  - std::string::npos se ainda nao da pra saber (buffer todo texto ate
//    agora, mas curto demais pra ter certeza) - chamador deve esperar mais
//    dados, a menos que buffer.size() >= kMaxBannerScan, caso em que deve
//    tratar como offset 0 (desiste de esperar banner).
size_t findBannerEnd(const std::string& buffer);
}  // namespace RttBanner
