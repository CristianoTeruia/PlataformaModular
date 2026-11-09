/**
 * @file eeprom24lc512.h
 * @brief Armazenamento de registros nas duas EEPROMs 24LC512 (I²C2) e exportação para a USB.
 *
 * São dois chips de 64 KiB (endereços de 7 bits 0x50 e 0x51), usados em sequência: os registros
 * começam em 0x0100 da EEPROM 1 (0x0000 a 0x00FF guardam os metadados) e, quando um registro não
 * cabe mais, continuam em 0x0000 da EEPROM 2. Um registro nunca é dividido entre os chips. Ao
 * esgotar o espaço, a gravação é interrompida sem sobrescrever dados.
 *
 * O formato do registro é descrito em sensor_registry.h. Todas as funções que acessam o
 * barramento obtêm o mutex do I²C2; as de gravação, exportação e limpeza devem ser chamadas com o
 * mutex da EEPROM já obtido.
 */
#ifndef EEPROM24LC512_H
#define EEPROM24LC512_H
#include "app_state.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Grava um bloco de bytes em um dos chips.
 *
 * A escrita é dividida nos limites de página de 128 B, porque uma escrita que cruza o limite da
 * página reiniciaria o endereço dentro dela e sobrescreveria dados. Após cada trecho espera 10 ms
 * (ciclo interno de gravação), cedendo a CPU.
 *
 * @param[in] address     Endereço inicial no chip.
 * @param[in] data        Dados a gravar.
 * @param[in] size        Quantidade de bytes (0 não faz nada e retorna `HAL_OK`).
 * @param[in] i2c_address Endereço I²C do chip (#EEPROM24LC512_ADDRESS_1 ou #EEPROM24LC512_ADDRESS_2).
 * @retval HAL_OK    Gravação concluída.
 * @retval HAL_ERROR @p data nulo, ou @p address + @p size acima de 64 KiB, ou falha ao obter o barramento.
 * @return Status da HAL em caso de falha de comunicação I²C.
 */
HAL_StatusTypeDef EEPROM24LC512_WriteBuffer(uint16_t address, uint8_t *data, uint16_t size, uint16_t i2c_address);

/**
 * @brief Lê um bloco de bytes de um dos chips.
 *
 * Logo após a leitura guarda `ErrorCode` e `State` do I²C2 em `eeprom_read_error_snapshot` e
 * `eeprom_read_state_snapshot`, para diagnóstico.
 *
 * @param[in]  address     Endereço inicial no chip.
 * @param[out] data        Destino dos dados lidos.
 * @param[in]  size        Quantidade de bytes (0 não faz nada e retorna `HAL_OK`).
 * @param[in]  i2c_address Endereço I²C do chip (#EEPROM24LC512_ADDRESS_1 ou #EEPROM24LC512_ADDRESS_2).
 * @retval HAL_OK    Leitura concluída.
 * @retval HAL_ERROR @p data nulo, ou @p address + @p size acima de 64 KiB, ou falha ao obter o barramento.
 * @return Status da HAL em caso de falha de comunicação I²C.
 */
HAL_StatusTypeDef EEPROM24LC512_ReadBuffer(uint16_t address, uint8_t *data, uint16_t size, uint16_t i2c_address);

/**
 * @brief Grava um byte na EEPROM 1.
 * @param[in] address Endereço no chip.
 * @param[in] data    Byte a gravar.
 * @return Status de EEPROM24LC512_WriteBuffer().
 */
HAL_StatusTypeDef EEPROM24LC512_WriteByte(uint16_t address, uint8_t data);

/**
 * @brief Lê um byte da EEPROM 1.
 * @param[in]  address Endereço no chip.
 * @param[out] data    Byte lido.
 * @return Status de EEPROM24LC512_ReadBuffer().
 */
HAL_StatusTypeDef EEPROM24LC512_ReadByte(uint16_t address, uint8_t *data);

/**
 * @brief Grava um registro de tamanho qualquer na EEPROM 1 (função simples, sem metadados).
 * @param[in] address Endereço no chip.
 * @param[in] data    Bytes do registro.
 * @param[in] size    Tamanho do registro, em bytes.
 * @return Status de EEPROM24LC512_WriteBuffer().
 */
HAL_StatusTypeDef EEPROM24LC512_WriteRecord(uint16_t address, uint8_t *data, uint8_t size);

/**
 * @brief Lê da EEPROM 1 um registro do formato antigo de tamanho fixo (legado).
 * @param[in]  address Endereço no chip.
 * @param[out] record  Registro lido (`sizeof(SensorRecordEEPROM)` bytes).
 * @retval HAL_ERROR @p record nulo.
 * @return Status de EEPROM24LC512_ReadBuffer().
 */
HAL_StatusTypeDef EEPROM24LC512_ReadRecord(uint16_t address, SensorRecordEEPROM *record);

/**
 * @brief Preenche um registro do formato antigo com as últimas leituras escaladas (legado).
 * @param[out] record Registro a preencher.
 */
void EEPROM_CreateSensorRecord(SensorRecordEEPROM *record);

/**
 * @brief Grava na EEPROM um registro com as últimas leituras dos sensores disponíveis.
 *
 * Etapas: valida os metadados; migra metadados da versão anterior, se for o caso; monta o registro
 * com os sensores que responderam no ciclo (EEPROM_BuildSensorRecord()); escolhe o chip e o
 * endereço (passa para a EEPROM 2 se o registro não couber na 1 e marca a memória como cheia se
 * também não couber na 2); grava; relê o que gravou e confere byte a byte e pelo CRC; avança o
 * ponteiro e salva os metadados. Atualiza `eeprom_status_display` (0 OK, 1 erro, 2 cheia).
 *
 * @retval HAL_OK    Registro gravado e verificado, e ponteiro persistido.
 * @retval HAL_ERROR Metadados inválidos, memória cheia, nenhum sensor disponível ou verificação
 *                   da gravação falhou.
 * @return Status da HAL em caso de falha de comunicação I²C.
 */
HAL_StatusTypeDef EEPROM_SaveSensorRecord(void);

/**
 * @brief Lê toda a EEPROM 1 em blocos de 64 B, para depuração.
 *
 * Cada bloco fica em `eeprom_full_read_buffer`; para ao primeiro erro. Não é usada em operação.
 */
void EEPROM24C512_ReadFullMemory(void);

/**
 * @brief Grava os metadados (`eeprom_metadata`) no endereço 0x0000 da EEPROM 1.
 * @return Status de EEPROM24LC512_WriteBuffer().
 */
HAL_StatusTypeDef EEPROM_SaveMetadata(void);

/**
 * @brief Exporta todos os registros gravados, em CSV, pela USB (CDC).
 *
 * Envia, nesta ordem: `EXPORT_BEGIN`, o cabeçalho do CSV, uma linha por registro válido (EEPROM 1
 * de 0x0100 até o ponteiro; EEPROM 2 de 0x0000 até o ponteiro) e `EXPORT_END,<enviados>,<suspeitos>`.
 * Cada linha começa por `sensor_mask,timestamp` e segue com as colunas dos sensores presentes.
 *
 * O CRC e a máscara de cada registro são validados. Um registro inválido é contado como suspeito e
 * a varredura avança um byte para procurar o próximo início válido (ressincronização). Se nada é
 * enviado por 500 ms, envia `PROGRESS,<endereço>` para o computador não dar tempo esgotado.
 * Realimenta o watchdog durante a operação e não escreve na memória. Em falha, envia uma linha
 * `ERRO,...` com o motivo.
 *
 * @retval HAL_OK    Exportação concluída.
 * @retval HAL_ERROR Metadados inválidos ou ponteiros inconsistentes.
 * @return Status da HAL em caso de falha de leitura ou de envio pela USB.
 */
HAL_StatusTypeDef EEPROM_ExportCSV_USB(void);

/**
 * @brief Lê e valida os metadados gravados na EEPROM 1.
 *
 * Metadados em branco (todos os bytes 0xFF) indicam a primeira utilização e são inicializados
 * (dados a partir de 0x0100 da EEPROM 1). Metadados com `magic` inválido ou campos inconsistentes
 * retornam `HAL_ERROR` sem alterar nada, para não perder registros. Metadados da versão anterior
 * (`full = 1` com a EEPROM 1 ativa) são migrados para a EEPROM 2. Atualiza `eeprom_metadata_load_ok`.
 *
 * @retval HAL_OK    Metadados válidos carregados.
 * @retval HAL_ERROR Metadados inválidos.
 * @return Status da HAL em caso de falha de comunicação I²C.
 */
HAL_StatusTypeDef EEPROM_LoadMetadata(void);

/**
 * @brief Calcula o CRC-16/CCITT-FALSE de um bloco de bytes.
 *
 * Polinômio 0x1021, valor inicial 0xFFFF, sem reflexão de bits e sem XOR final. Para a sequência
 * ASCII `"123456789"` o resultado é 0x29B1.
 *
 * @param[in] data   Bytes a processar.
 * @param[in] length Quantidade de bytes.
 * @return CRC calculado, ou 0 se @p data for nulo.
 */
uint16_t EEPROM_CalculateCRC16(const uint8_t *data, uint16_t length);

/**
 * @brief Autoteste do CRC com o vetor padrão `"123456789"` (esperado 0x29B1).
 * @retval HAL_OK    CRC correto.
 * @retval HAL_ERROR CRC diferente do esperado.
 */
HAL_StatusTypeDef EEPROM_Test_CRC16(void);

/**
 * @brief Monta um registro: máscara, dados dos sensores, data e hora do RTC e CRC.
 *
 * Os dados dos sensores seguem a ordem da tabela de sensor_registry.h. A data e a hora são lidas do
 * RTC sob App_RTC_Lock() (ano completo em 2 bytes, igual a 2000 + `Year`). O CRC cobre todos os
 * bytes anteriores e é gravado em little-endian.
 *
 * @param[out] buffer       Buffer de pelo menos #EEPROM_MAX_RECORD_SIZE bytes.
 * @param[out] record_size  Tamanho do registro montado, em bytes.
 * @param[in]  sensor_mask  Sensores que fazem parte do registro.
 * @retval HAL_OK    Registro montado.
 * @retval HAL_ERROR Parâmetro nulo, máscara nula ou inválida, falha de leitura do RTC ou campos de
 *                   calendário fora da faixa.
 */
HAL_StatusTypeDef EEPROM_BuildSensorRecord(uint8_t *buffer, uint8_t *record_size, uint8_t sensor_mask);

/**
 * @brief Apaga as duas EEPROMs e reinicia os metadados.
 *
 * Grava 0xFF em todas as páginas dos dois chips (leva vários segundos, e por isso realimenta o
 * watchdog) e depois regrava metadados iniciais válidos, com o ponteiro em 0x0100 da EEPROM 1.
 * @warning Operação destrutiva: todos os registros são perdidos.
 * @retval HAL_OK Memórias apagadas e metadados reiniciados.
 * @return Status da HAL em caso de falha de comunicação I²C.
 */
HAL_StatusTypeDef EEPROM24LC512_EraseAll(void);

/**
 * @name Testes manuais
 * Rotinas de verificação usadas durante o desenvolvimento. Não são executadas automaticamente
 * (as chamadas no `main.c` ficam comentadas).
 * @warning Os testes escrevem em endereços baixos dos chips e podem sobrescrever registros já
 *          gravados; não execute com dados importantes na memória.
 * @{
 */

/**
 * @brief Grava 32 bytes a partir de 0x0070 da EEPROM 2, atravessando o limite de página, e confere a leitura.
 * @retval HAL_OK Dados lidos iguais aos gravados.
 */
HAL_StatusTypeDef EEPROM2_Test(void);

/**
 * @brief Grava valores diferentes no endereço 0x0020 de cada chip e confere que são independentes.
 * @retval HAL_OK Cada chip devolveu o valor que recebeu.
 */
HAL_StatusTypeDef EEPROM_Test_Independence(void);

/**
 * @brief Grava e relê um byte em 0x0000, 0x0100, 0x1000, 0x8000 e 0xFFFF da EEPROM 2.
 * @retval HAL_OK Todos os valores conferem.
 */
HAL_StatusTypeDef EEPROM_Test_AddressBoundaries(void);

/**
 * @brief Grava 32 bytes a partir de 0x0070 da EEPROM 2 (cruza a página de 128 B) e confere a leitura.
 * @retval HAL_OK Dados lidos iguais aos gravados.
 */
HAL_StatusTypeDef EEPROM_Test_PageBoundary(void);

/**
 * @brief Monta um registro com os sensores disponíveis, grava em 0x0200 da EEPROM 2, relê e confere
 *        byte a byte e pelo CRC.
 * @retval HAL_OK Registro idêntico e com CRC válido.
 */
HAL_StatusTypeDef EEPROM_Test_SerializedRecordRoundTrip(void);
/** @} */
#ifdef __cplusplus
}
#endif
#endif
