/**
 * @file bmp280.c
 * @brief Implementação do driver do BMP280 (ver bmp280.h).
 *
 * A compensação de temperatura e de pressão segue as fórmulas em inteiros do datasheet do
 * fabricante, com os coeficientes `dig_*` lidos do próprio sensor.
 */
#include "bmp280.h"
#include "app_state.h"
#include <string.h>
#include <stddef.h>

HAL_StatusTypeDef BMP280_ReadCalibration(void)
{
    uint8_t calib[24];

    HAL_StatusTypeDef status;

    status = HAL_I2C_Mem_Read(
        &hi2c1,
        0x77 << 1,
        0x88,
        I2C_MEMADD_SIZE_8BIT,
        calib,
        24,
        100
    );

    if (status != HAL_OK)
        return status;

    dig_T1 = (uint16_t)((calib[1] << 8) | calib[0]);
    dig_T2 = (int16_t)((calib[3] << 8) | calib[2]);
    dig_T3 = (int16_t)((calib[5] << 8) | calib[4]);

    dig_P1 = (uint16_t)((calib[7] << 8) | calib[6]);
    dig_P2 = (int16_t)((calib[9] << 8) | calib[8]);
    dig_P3 = (int16_t)((calib[11] << 8) | calib[10]);
    dig_P4 = (int16_t)((calib[13] << 8) | calib[12]);
    dig_P5 = (int16_t)((calib[15] << 8) | calib[14]);
    dig_P6 = (int16_t)((calib[17] << 8) | calib[16]);
    dig_P7 = (int16_t)((calib[19] << 8) | calib[18]);
    dig_P8 = (int16_t)((calib[21] << 8) | calib[20]);
    dig_P9 = (int16_t)((calib[23] << 8) | calib[22]);

    return HAL_OK;
}


HAL_StatusTypeDef BMP280_Read(float *temperature, float *pressure)
{
    uint8_t data[6];

    int32_t adc_T;
    int32_t adc_P;

    int32_t var1;
    int32_t var2;
    int32_t T;

    int64_t p_var1;
    int64_t p_var2;
    int64_t p;

    HAL_StatusTypeDef status;

    /* Coloca o BMP280 em modo normal
       temperatura x1, pressão x1 */
    uint8_t ctrl_meas = 0x27;

    status = HAL_I2C_Mem_Write(
        &hi2c1,
        0x77 << 1,
        0xF4,
        I2C_MEMADD_SIZE_8BIT,
        &ctrl_meas,
        1,
        100
    );

    if (status != HAL_OK)
        return status;

    /* Aguarda a conversão */
    App_Delay(100);

    /* Lê pressão e temperatura
       registradores 0xF7 até 0xFC */
    status = HAL_I2C_Mem_Read(
        &hi2c1,
        0x77 << 1,
        0xF7,
        I2C_MEMADD_SIZE_8BIT,
        data,
        6,
        100
    );

    if (status != HAL_OK)
        return status;

    /* Valor bruto da pressão */
    adc_P = ((int32_t)data[0] << 12) |
            ((int32_t)data[1] << 4) |
            ((int32_t)data[2] >> 4);

    /* Valor bruto da temperatura */
    adc_T = ((int32_t)data[3] << 12) |
            ((int32_t)data[4] << 4) |
            ((int32_t)data[5] >> 4);

    /* Compensação da temperatura */

    var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) *
            ((int32_t)dig_T2)) >> 11;

    var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) *
              ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) *
            ((int32_t)dig_T3)) >> 14;

    bmp280_t_fine = var1 + var2;

    T = (bmp280_t_fine * 5 + 128) >> 8;

    *temperature = T / 100.0f;

    /* Compensação da pressão */

    p_var1 = ((int64_t)bmp280_t_fine) - 128000;

    p_var2 = p_var1 * p_var1 * (int64_t)dig_P6;
    p_var2 = p_var2 + ((p_var1 * (int64_t)dig_P5) << 17);
    p_var2 = p_var2 + (((int64_t)dig_P4) << 35);

    p_var1 = ((p_var1 * p_var1 * (int64_t)dig_P3) >> 8) +
             ((p_var1 * (int64_t)dig_P2) << 12);

    p_var1 = (((((int64_t)1) << 47) + p_var1) *
              ((int64_t)dig_P1)) >> 33;

    if (p_var1 == 0)
        return HAL_ERROR;

    p = 1048576 - adc_P;

    p = (((p << 31) - p_var2) * 3125) / p_var1;

    p_var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    p_var2 = (((int64_t)dig_P8) * p) >> 19;

    p = ((p + p_var1 + p_var2) >> 8) +
        (((int64_t)dig_P7) << 4);

    *pressure = p / 256.0f;

    return HAL_OK;
}

