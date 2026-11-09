/**
 * @file bmp280.h
 * @brief Driver do sensor de pressão e temperatura BMP280 (I²C1, endereço 0x77).
 */
#ifndef BMP280_H
#define BMP280_H
#include "app_state.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Lê os coeficientes de calibração de fábrica.
 *
 * Lê 24 bytes a partir do registrador 0x88 e preenche `dig_T1` a `dig_P9`. O registro de
 * sensores chama esta função antes de cada leitura.
 * @retval HAL_OK Calibração lida.
 * @return Status da HAL em caso de falha de comunicação I²C.
 */
HAL_StatusTypeDef BMP280_ReadCalibration(void);

/**
 * @brief Mede temperatura e pressão.
 *
 * Coloca o sensor em modo normal com oversampling x1 de temperatura e de pressão
 * (`CTRL_MEAS = 0x27`), espera 100 ms (cedendo a CPU), lê os registradores 0xF7 a 0xFC e aplica
 * a compensação em inteiros do datasheet, que atualiza `bmp280_t_fine`. Requer calibração
 * lida antes (BMP280_ReadCalibration()).
 *
 * @param[out] temperature Temperatura, em °C.
 * @param[out] pressure    Pressão, em Pa.
 * @retval HAL_OK    Medição válida.
 * @retval HAL_ERROR Divisor nulo na compensação da pressão (calibração inválida).
 * @return Status da HAL em caso de falha de comunicação I²C.
 */
HAL_StatusTypeDef BMP280_Read(float *temperature, float *pressure);
#ifdef __cplusplus
}
#endif
#endif
