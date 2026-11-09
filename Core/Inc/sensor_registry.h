/**
 * @file sensor_registry.h
 * @brief Registro de sensores: tabela que descreve cada sensor I²C do sistema.
 *
 * Cada sensor declara UMA entrada na tabela de sensor_registry.c: bit da máscara, tamanho do
 * dado no registro, função de leitura, serialização e formatação em CSV. O restante do firmware
 * (registro da EEPROM, exportação CSV, tarefa de sensores e OLED) apenas percorre a tabela.
 * Adicionar um sensor equivale a escrever um driver e acrescentar uma linha na tabela.
 *
 * Formato do registro gravado na EEPROM:
 * @code
 * [máscara 1 B][dados dos sensores presentes, na ordem da tabela][RTC 7 B][CRC-16 2 B]
 * @endcode
 * O RTC ocupa 7 bytes (ano em 2 bytes little-endian, mês, dia, hora, minuto e segundo) e o CRC
 * é o CRC-16/CCITT-FALSE dos bytes anteriores, gravado em little-endian.
 */
#ifndef SENSOR_REGISTRY_H
#define SENSOR_REGISTRY_H

#include <stdint.h>
#include <stddef.h>
#include "main.h"

/** @brief Entrada da tabela de sensores. */
typedef struct
{
    const char *name;         /**< Nome curto, usado no OLED (`ERRO:<nome>`). */
    uint8_t mask_bit;         /**< Bit deste sensor na máscara do registro. */
    uint8_t payload_size;     /**< Bytes deste sensor no registro. */
    HAL_StatusTypeDef (*read)(void);          /**< Lê o sensor; `HAL_OK` = amostra válida obtida. */
    void (*serialize)(uint8_t *out);          /**< Grava `payload_size` bytes da última amostra em @c out. */
    const char *csv_header;                   /**< Colunas deste sensor no CSV, separadas por vírgula. */
    /**
     * Escreve as colunas do CSV (sem vírgula inicial nem final). Com `payload == NULL` o sensor
     * está ausente no registro e os campos saem vazios. Retorna os caracteres escritos ou -1.
     */
    int (*csv_format)(const uint8_t *payload, char *out, size_t capacity);
} SensorDriver;

/**
 * @brief Número de sensores da tabela.
 * @return Quantidade de entradas.
 */
uint8_t Sensors_Count(void);

/**
 * @brief Obtém uma entrada da tabela.
 * @param[in] index Posição na tabela (a ordem define a ordem dos dados no registro e no CSV).
 * @return Ponteiro para a entrada, ou `NULL` se @p index for inválido.
 */
const SensorDriver *Sensors_Get(uint8_t index);

/**
 * @brief Lê um sensor e atualiza as máscaras "disponível agora" e "detectado desde o boot".
 *
 * Se a leitura falha, o bit sai da máscara de disponíveis. `HAL_BUSY` (sensor responde, mas ainda
 * não entrega dado) conta como detectado, para o OLED mostrar `ERRO:<nome>` em vez de ignorar um
 * módulo conectado.
 * @param[in] index Posição do sensor na tabela (índices inválidos são ignorados).
 */
void Sensors_ReadOne(uint8_t index);

/**
 * @brief Máscara dos sensores que responderam na última tentativa.
 * @return Bits de SENSOR_AHT20, SENSOR_BMP280 etc.
 */
uint8_t Sensors_AvailableMask(void);

/**
 * @brief Máscara dos sensores que já responderam desde o boot.
 * @return Bits de SENSOR_AHT20, SENSOR_BMP280 etc.
 */
uint8_t Sensors_DetectedMask(void);

/** @brief Atualiza as variáveis escaladas usadas pelo OLED (`display_*`) a partir das últimas leituras. */
void Sensors_UpdateDisplayValues(void);

/**
 * @brief Calcula o tamanho do registro para uma máscara de sensores.
 *
 * Soma máscara (1 B), RTC (7 B), CRC (2 B) e os dados de cada sensor presente.
 * @param[in]  mask Máscara de sensores do registro.
 * @param[out] size Tamanho total do registro, em bytes.
 * @retval 1 Máscara válida; @p size preenchido.
 * @retval 0 Máscara nula, com bits desconhecidos, @p size nulo ou registro maior que
 *           #EEPROM_MAX_RECORD_SIZE.
 */
uint8_t Sensors_RecordSizeFromMask(uint8_t mask, uint8_t *size);

/**
 * @brief Serializa os dados dos sensores da máscara, na ordem da tabela.
 *
 * A máscara deve ter sido validada por Sensors_RecordSizeFromMask().
 * @param[in] mask   Máscara de sensores.
 * @param[out] buffer Buffer do registro.
 * @param[in] index  Posição de @p buffer onde começam os dados.
 * @return Novo índice (posição logo após o último byte escrito).
 */
uint8_t Sensors_Serialize(uint8_t mask, uint8_t *buffer, uint8_t index);

/**
 * @brief Monta a linha de cabeçalho do CSV.
 * @param[out] out      Buffer de saída.
 * @param[in]  capacity Tamanho de @p out, em bytes.
 * @return Caracteres escritos, ou -1 se não couber.
 */
int Sensors_CsvHeader(char *out, size_t capacity);

/**
 * @brief Formata as colunas de sensores de uma linha do CSV.
 *
 * Sensores ausentes na máscara geram campos vazios.
 * @param[in]  mask     Máscara do registro.
 * @param[in]  payload  Dados dos sensores presentes (logo após o byte de máscara).
 * @param[out] out      Buffer de saída.
 * @param[in]  capacity Tamanho de @p out, em bytes.
 * @return Caracteres escritos, ou -1 se não couber.
 */
int Sensors_CsvFormatRow(uint8_t mask, const uint8_t *payload, char *out, size_t capacity);

/**
 * @brief Conta os bits em 1 de um byte.
 * @param[in] value Byte a examinar.
 * @return Número de bits em 1.
 */
uint8_t Sensors_PopCount(uint8_t value);

/**
 * @brief Nome do primeiro sensor detectado que não está respondendo agora.
 * @param[in] detected_mask Sensores detectados desde o boot.
 * @param[in] present_mask  Sensores que responderam na última tentativa.
 * @return Nome do sensor, ou `NULL` se todos os detectados estão respondendo.
 */
const char *Sensors_FirstMissingName(uint8_t detected_mask, uint8_t present_mask);

#endif /* SENSOR_REGISTRY_H */
