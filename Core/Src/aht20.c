/**
 * @file aht20.c
 * @brief Implementação do driver do AHT20 (ver aht20.h).
 */
#include "aht20.h"
#include "app_state.h"
#include <string.h>
#include <stddef.h>

HAL_StatusTypeDef AHT20_Read(float *temperature, float *humidity)
{
    uint8_t command[3] = {0xAC, 0x33, 0x00};
    uint8_t data[6];

    HAL_StatusTypeDef status;

    // Inicia uma medição
    status = HAL_I2C_Master_Transmit(&hi2c1, 0x38 << 1,
                                     command, 3, 100);

    if (status != HAL_OK)
        return status;

    // Aguarda o AHT20 terminar a medição
    App_Delay(80);

    // Lê os 6 bytes de resposta
    status = HAL_I2C_Master_Receive(&hi2c1, 0x38 << 1,
                                    data, 6, 100);

    if (status != HAL_OK)
        return status;

    // Verifica se o sensor ainda está ocupado
    if (data[0] & 0x80)
        return HAL_BUSY;

    // Calcula umidade
    uint32_t raw_humidity =
        ((uint32_t)data[1] << 12) |
        ((uint32_t)data[2] << 4) |
        ((uint32_t)data[3] >> 4);

    *humidity = ((float)raw_humidity * 100.0f) / 1048576.0f;

    // Calcula temperatura
    uint32_t raw_temperature =
        (((uint32_t)data[3] & 0x0F) << 16) |
        ((uint32_t)data[4] << 8) |
        data[5];

    *temperature =
        ((float)raw_temperature * 200.0f) / 1048576.0f - 50.0f;

    return HAL_OK;
}

