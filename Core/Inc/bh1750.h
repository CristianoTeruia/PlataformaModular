/**
 * @file bh1750.h
 * @brief Driver do sensor de luminosidade BH1750 (I²C1).
 *
 * O endereço é 0x23 (ADDR em GND ou flutuando) ou 0x5C (ADDR em VCC); os dois são testados
 * automaticamente enquanto o sensor não for detectado.
 */
#ifndef BH1750_H
#define BH1750_H

#include "app_state.h"

#ifdef __cplusplus
extern "C" {
#endif

extern volatile uint8_t bh1750_addr;      /**< Endereço de 7 bits detectado (0 = não detectado). */
extern volatile uint32_t bh1750_last_hal; /**< Último status da HAL (diagnóstico). */
extern volatile uint16_t bh1750_last_raw; /**< Última contagem bruta lida. */

/**
 * @brief Faz uma medição única em alta resolução.
 *
 * Enquanto o sensor não foi detectado, procura-o em 0x23 e 0x5C. Envia Power On e o comando
 * One Time H-Resolution (0x20), espera 180 ms (cedendo a CPU) e lê 2 bytes; o sensor volta ao
 * repouso sozinho. Em qualquer falha de I²C o endereço é esquecido, para ser procurado de novo
 * no ciclo seguinte.
 *
 * @param[out] raw Contagem bruta de 16 bits; a iluminância em lux é `raw / 1,2`.
 * @retval HAL_OK    Medição válida.
 * @retval HAL_ERROR @p raw nulo.
 * @return Status da HAL em caso de falha de comunicação I²C.
 */
HAL_StatusTypeDef BH1750_Read(uint16_t *raw);

#ifdef __cplusplus
}
#endif

#endif
