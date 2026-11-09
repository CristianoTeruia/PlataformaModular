/**
 * @file ens160.h
 * @brief Driver do sensor de qualidade do ar ENS160 (I²C1).
 *
 * O endereço é 0x53 (ADDR em VCC) ou 0x52 (ADDR em GND); os dois são testados automaticamente e
 * confirmados pelo PART_ID (0x0160).
 *
 * @note O driver tem testes no PC, mas o módulo usado nos ensaios nunca completou uma medição
 *       com alimentação de 3,3 V; por isso o ENS160 ficou fora dos ensaios em hardware.
 */
#ifndef ENS160_H
#define ENS160_H

#include "app_state.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Amostra do ENS160. */
typedef struct
{
    uint8_t aqi;        /**< Índice de qualidade do ar AQI-UBA, de 1 (excelente) a 5 (ruim). */
    uint16_t tvoc_ppb;  /**< Compostos orgânicos voláteis totais, em ppb. */
    uint16_t eco2_ppm;  /**< CO₂ equivalente, em ppm. */
    uint8_t validity;   /**< 0 normal, 1 aquecimento, 2 partida inicial, 3 inválido. */
} ENS160_Sample;

extern volatile uint8_t ens160_addr;        /**< Endereço de 7 bits detectado (0 = não detectado). */
extern volatile uint16_t ens160_part_id;    /**< Último PART_ID lido (esperado 0x0160). */
extern volatile uint8_t ens160_last_status; /**< Último DEVICE_STATUS lido. */
extern volatile uint8_t ens160_opmode;      /**< Último OPMODE lido (0 sono, 1 ocioso, 2 padrão). */
extern volatile uint32_t ens160_init_count; /**< Sequências de inicialização executadas. */
extern volatile uint32_t ens160_last_hal;   /**< Último status da HAL (diagnóstico). */
extern volatile uint32_t ens160_timeout_count;   /**< Ciclos sem dado novo. */
extern volatile uint32_t ens160_error_count;     /**< Erros internos do sensor (bit STATER). */
extern volatile uint32_t ens160_comm_fail_count; /**< Falhas de comunicação I²C. */
extern volatile uint32_t ens160_reset_count;     /**< Inicializações que usaram RESET. */
extern volatile uint32_t ens160_zero_count;      /**< Leituras com AQI, TVOC e eCO2 todos nulos (sem medição real). */
extern volatile uint8_t ens160_fw_major, ens160_fw_minor, ens160_fw_build; /**< Versão do firmware interno do sensor. */
extern volatile uint16_t ens160_echo_t, ens160_echo_rh; /**< Temperatura e umidade que o sensor informa estar usando (DATA_T e DATA_RH). */
extern volatile uint16_t ens160_last_tvoc, ens160_last_eco2; /**< Últimos TVOC (ppb) e eCO2 (ppm) válidos. */
extern volatile uint8_t ens160_last_aqi, ens160_last_validity; /**< Último AQI e última validade válidos. */

/**
 * @brief Lê uma amostra de qualidade do ar.
 *
 * Detecta o sensor (confirmando o PART_ID), coloca-o em modo padrão se não estiver medindo,
 * envia a compensação de temperatura e umidade, aguarda dado novo (cedendo a CPU) e lê AQI,
 * TVOC e eCO2. Leituras com os três valores nulos não são medições e retornam `HAL_BUSY`. Após
 * falhas seguidas, a próxima inicialização usa RESET.
 *
 * @param[in]  compensation_valid Se verdadeiro, envia @p temperature_c e @p humidity_pct ao sensor.
 * @param[in]  temperature_c      Temperatura de compensação, em °C (limitada a -40 a 85).
 * @param[in]  humidity_pct       Umidade de compensação, em % (limitada a 0 a 100).
 * @param[out] out                Amostra lida.
 * @retval HAL_OK    Amostra válida.
 * @retval HAL_BUSY  O sensor ainda não produziu dado válido.
 * @retval HAL_ERROR @p out nulo, sensor não encontrado ou erro interno do sensor.
 * @return Outro status da HAL em caso de falha de comunicação I²C.
 */
HAL_StatusTypeDef ENS160_Read(bool compensation_valid, float temperature_c, float humidity_pct,
                              ENS160_Sample *out);

#ifdef __cplusplus
}
#endif

#endif
