# Design — Parser binário guiado por mapa de dados

**Data:** 2026-07-25
**Status:** aprovado pelo autor (brainstorming em sessão, no repositório `TCC_Firmware`)
**Companion:** `docs/superpowers/specs/2026-07-25-rtt-binary-telemetry-design.md`
no repositório `TCC_Firmware` — fonte de verdade do formato de wire e do
mapa de dados. Este documento cobre só o lado do plugin.

## Objetivo

O firmware do TCC (`TCC_Firmware`) está migrando a telemetria de CAN bus
para um registro binário de tamanho fixo sobre um canal RTT dedicado (o
texto `key=value` que este plugin lê hoje via `LineParser` não é mais
emitido por nenhum firmware). Este plugin precisa de um novo caminho de
parsing binário, guiado por um schema JSON ("mapa de dados") gerado a
partir do lado do firmware, em vez de um layout hardcoded em C++ — assim
uma mudança de layout no firmware (campo novo, reordenação) é uma edição
de JSON, não um rebuild do plugin.

## Risco a validar antes de escrever o parser

O socket usado hoje (`datastream_rtt.cpp::connectToServer`) fala com a
porta **telnet** do GDB Server do J-Link
(`$$SEGGER_TELNET_ConfigStr=RTTCh;N$$`). Telnet tem regras de escaping
próprias (IAC `0xFF` em particular). O formato texto nunca esbarrou
nisso; um payload binário com bytes de float IEEE-754 vai conter `0xFF`,
`0x0A`, `0x0D` com frequência alta. Se o relay do J-Link fizer qualquer
tradução desses bytes no caminho, o stream chega corrompido de forma
silenciosa.

**Antes de implementar `BinaryParser`:** rodar um teste dedicado (ver
`scripts/rtt_probe.py`, adaptar para escrever/ler um padrão de bytes
conhecido incluindo sequências de `0xFF`) contra o firmware já emitindo
no canal 2, e confirmar bytes idênticos chegando. Se falhar, o formato
de wire no firmware precisa de um encoding adicional (ex. COBS) antes
deste plugin fazer sentido — isso mudaria o schema JSON (campo
`encoding`) e este design seria revisado.

## Decisões de abordagem

1. **Substituir `LineParser` por `BinaryParser`, não manter os dois.**
   Nenhum firmware no projeto ainda emite o formato texto (foi removido
   em 2026-07-23, antes até da tentativa via CAN) — manter os dois
   parsers seria manutenção de código morto. `LineParser`/`line_parser.*`
   e seus testes saem do plugin.

2. **Mapa de dados carregado em runtime (JSON via `QJsonDocument`), não
   hardcoded.** O layout do registro (offsets, tipos, nomes, unidades)
   vem de um arquivo JSON (schema em
   `docs/superpowers/specs/2026-07-25-rtt-binary-telemetry-design.md`,
   seção "Mapa de dados", no repo `TCC_Firmware`) selecionado no diálogo
   de conexão. Uma mudança de layout no firmware não exige rebuild do
   plugin — só apontar para um JSON atualizado.

3. **Reaproveitar `TelemetrySample`, `pushSamples()` e `CsvLogger` sem
   mudança.** `BinaryParser::feed()` tem a mesma assinatura de
   responsabilidade que `LineParser::feed()` — recebe bytes crus,
   devolve `std::vector<TelemetrySample>`. Enquanto o parser continuar
   produzindo essa struct, todo o resto do plugin (log CSV por ensaio,
   `clearBuffers` em reset, `dataReceived()`) continua funcionando sem
   tocar em `datastream_rtt.cpp` além da troca do parser.

4. **`seq` do mapa de dados dirige tempo e detecção de reset/gap, não um
   campo `t` enviado pelo firmware.** O JSON marca qual campo é o
   `seq_field`; o parser computa `t = seq / nominal_rate_hz` (ambos do
   JSON) em vez de confiar em timestamp de chegada (jitter de SO/TCP) ou
   em um campo de tempo do firmware. Reset do alvo = `seq` voltando a um
   valor menor que o anterior (mesma lógica que `LineParser` já tinha
   para `t` decrescente). Gap = `seq` pulando mais de 1 — mesma métrica
   de `sequenceGaps()` que já existe, só trocando a fonte do número de
   sequência.

## `BinaryParser` — esboço da interface

Mantém o mesmo formato de responsabilidade de `LineParser` para minimizar
mudança em `datastream_rtt.cpp`:

```cpp
struct RttFieldDef
{
  std::string name;
  size_t offset;
  std::string type;   // "uint8"|"uint16"|"uint32"|"float32"
  std::string unit;
};

struct RttDataMap
{
  size_t record_size;
  std::string seq_field;
  double nominal_rate_hz;
  std::vector<RttFieldDef> fields;

  static RttDataMap loadFromFile(const std::string& path);  // QJsonDocument
};

class BinaryParser
{
public:
  explicit BinaryParser(RttDataMap map);

  std::vector<TelemetrySample> feed(const char* data, size_t len);

  uint64_t malformedCount() const;   // registros descartados (tamanho invalido)
  uint64_t sequenceGaps() const;     // buracos detectados em seq

private:
  bool decodeRecord(const uint8_t* record, TelemetrySample& out);

  RttDataMap _map;
  std::string _buffer;   // bytes crus acumulados, ainda sem registro completo
  int64_t _last_seq = -1;
};
```

`decodeRecord()` lê `record_size` bytes fixos do início do `_buffer` (sem
delimitador — tamanho fixo dispensa isso), decodifica cada campo do mapa
pelo offset/tipo, e usa `seq_field` para achar o contador de sequência
dentro dos campos decodificados (sem tratamento especial de parsing, só
de pós-processamento — ele também vira uma série normal em `values`,
útil para visualizar gaps direto no PlotJuggler).

## Diálogo de conexão (`datastream_rtt.cpp::start`)

Adiciona um campo (`QLineEdit` + botão de arquivo, mesmo padrão do
diretório de CSV já existente) para o caminho do JSON do mapa de dados,
persistido em `QSettings` como os demais campos (`DataStreamRTT/data_map_path`).
Falha ao carregar/parsear o JSON bloqueia o `start()` com o mesmo padrão
de `QMessageBox::warning` já usado para diretório de CSV inválido.

## Mudanças em arquivos

- **Novo:** `binary_parser.h`/`.cpp` (+ `RttDataMap`), `tests/test_binary_parser.cpp`.
- **Removido:** `line_parser.h`/`.cpp`, `tests/test_line_parser.cpp`.
- **`datastream_rtt.h`/`.cpp`:** troca `LineParser _parser` por
  `BinaryParser _parser` (construído com o `RttDataMap` carregado no
  `start()`); novo campo de UI para o caminho do mapa; `xmlSaveState`/
  `xmlLoadState` ganham o atributo do caminho do mapa.
- **`csv_logger.h`/`.cpp`:** sem mudança — já opera sobre `TelemetrySample`.
- **`CMakeLists.txt`:** troca a lib `line_parser` por `binary_parser`;
  liga `Qt5::Core` (já linkado via `Qt5::Widgets`) para `QJsonDocument`.

## Plano de testes

1. Teste de transparência binária via telnet (ver "Risco a validar"
   acima) — bloqueante, roda antes do resto.
2. `test_binary_parser.cpp`: decodificação por offset/tipo, detecção de
   gap via `seq`, detecção de reset (`seq` decrescendo), registro
   truncado (menos bytes que `record_size` — deve esperar mais dados, não
   descartar).
3. `test_csv_logger.cpp` continua passando sem mudança (não depende do
   parser, só de `TelemetrySample`).
4. Validação end-to-end na bancada: firmware emitindo no canal 2, plugin
   conectado com o mapa JSON real do `TCC_Firmware`, confirmar séries
   nomeadas corretamente (`ib`, `ic`, ... como no mapa) e sem gaps numa
   sessão de alguns minutos.

## Fora de escopo

- Suporte a mais de um mapa de dados simultâneo / múltiplos registros de
  tamanhos diferentes no mesmo canal — o firmware emite um único tipo de
  registro fixo (ver spec do `TCC_Firmware`).
- Versionamento/migração automática de schema — o campo `version` no
  JSON existe para o futuro, sem lógica de migração implementada agora.
