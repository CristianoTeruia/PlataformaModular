/**
 * @file aht20.h
 * @brief Driver do sensor de temperatura e umidade AHT20 (I²C1, endereço 0x38).
 */
#ifndef AHT20_H
#define AHT20_H
#include "app_state.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Dispara uma medição e lê o resultado.
 *
 * Envia o comando `0xAC 0x33 0x00`, espera 80 ms (cedendo a CPU) e lê 6 bytes. A umidade
 * (20 bits) é convertida por `raw * 100 / 2^20` e a temperatura (20 bits), por
 * `raw * 200 / 2^20 - 50`.
 *
 * @param[out] temperature Temperatura, em °C.
 * @param[out] humidity    Umidade relativa, em %.
 * @retval HAL_OK   Medição válida.
 * @retval HAL_BUSY O sensor ainda estava ocupado (bit 7 do status).
 * @return Outro status da HAL em caso de falha de comunicação I²C.
 */
HAL_StatusTypeDef AHT20_Read(float *temperature, float *humidity);
#ifdef __cplusplus
}
#endif
#endif
