/**
 * @file mpu6500.c
 * @brief Implementação do driver do MPU6500 (ver mpu6500.h).
 */
#include "mpu6500.h"
#include "app_state.h"
#include <string.h>
#include <stddef.h>

HAL_StatusTypeDef MPU6500_Init(void)
{
    HAL_StatusTypeDef status;

    uint8_t who_am_i;

    /* Lê WHO_AM_I */
    status = HAL_I2C_Mem_Read(
        &hi2c1,
        0x68 << 1,
        0x75,
        I2C_MEMADD_SIZE_8BIT,
        &who_am_i,
        1,
        100
    );

    if (status != HAL_OK)
        return status;

    mpu6500_who_am_i = who_am_i;

    /* Esperamos 0x70 para o MPU6500 */
    if (who_am_i != 0x70)
        return HAL_ERROR;

    /* Sai do modo sleep */
    uint8_t power_management = 0x00;

    status = HAL_I2C_Mem_Write(
        &hi2c1,
        0x68 << 1,
        0x6B,
        I2C_MEMADD_SIZE_8BIT,
        &power_management,
        1,
        100
    );

    if (status != HAL_OK)
        return status;

    App_Delay(100);

    /* Acelerômetro: ±2 g */
    uint8_t accel_config = 0x00;

    status = HAL_I2C_Mem_Write(
        &hi2c1,
        0x68 << 1,
        0x1C,
        I2C_MEMADD_SIZE_8BIT,
        &accel_config,
        1,
        100
    );

    if (status != HAL_OK)
        return status;

    /* Giroscópio: ±250 °/s */
    uint8_t gyro_config = 0x00;

    status = HAL_I2C_Mem_Write(
        &hi2c1,
        0x68 << 1,
        0x1B,
        I2C_MEMADD_SIZE_8BIT,
        &gyro_config,
        1,
        100
    );

    if (status != HAL_OK)
        return status;

    return HAL_OK;
}


/* PWR_MGMT_1 (0x6B): bit 6 = SLEEP. */
HAL_StatusTypeDef MPU6500_Sleep(void)
{
    uint8_t power_management = 0x40;
    return HAL_I2C_Mem_Write(&hi2c1, 0x68 << 1, 0x6B, I2C_MEMADD_SIZE_8BIT, &power_management, 1, 100);
}

HAL_StatusTypeDef MPU6500_Wake(void)
{
    uint8_t power_management = 0x00;
    HAL_StatusTypeDef status = HAL_I2C_Mem_Write(&hi2c1, 0x68 << 1, 0x6B, I2C_MEMADD_SIZE_8BIT,
                                                 &power_management, 1, 100);
    if (status == HAL_OK) App_Delay(100); /* estabilizacao do acelerometro e do giroscopio */
    return status;
}

HAL_StatusTypeDef MPU6500_Read(void)
{
    uint8_t data[14];

    HAL_StatusTypeDef status;

    /*
     * Começa em ACCEL_XOUT_H (0x3B)
     *
     * 0x3B-0x40 = acelerômetro
     * 0x41-0x42 = temperatura interna
     * 0x43-0x48 = giroscópio
     */

    status = HAL_I2C_Mem_Read(
        &hi2c1,
        0x68 << 1,
        0x3B,
        I2C_MEMADD_SIZE_8BIT,
        data,
        14,
        100
    );

    if (status != HAL_OK)
        return status;

    mpu6500_accel_x =
        (int16_t)((data[0] << 8) | data[1]);

    mpu6500_accel_y =
        (int16_t)((data[2] << 8) | data[3]);

    mpu6500_accel_z =
        (int16_t)((data[4] << 8) | data[5]);

    mpu6500_gyro_x =
        (int16_t)((data[8] << 8) | data[9]);

    mpu6500_gyro_y =
        (int16_t)((data[10] << 8) | data[11]);

    mpu6500_gyro_z =
        (int16_t)((data[12] << 8) | data[13]);

    /* Conversão do acelerômetro
       ±2 g → 16384 LSB/g */

    float raw_accel_x_g = mpu6500_accel_x / 16384.0f;
    float raw_accel_y_g = mpu6500_accel_y / 16384.0f;
    float raw_accel_z_g = mpu6500_accel_z / 16384.0f;


    /* Aplicação dos offsets */

    mpu6500_accel_x_g =
        raw_accel_x_g - mpu6500_accel_offset_x;

    mpu6500_accel_y_g =
        raw_accel_y_g - mpu6500_accel_offset_y;

    mpu6500_accel_z_g =
        raw_accel_z_g - mpu6500_accel_offset_z;


    /* Conversão do giroscópio
       ±250 °/s → 131 LSB/(°/s) */

    float raw_gyro_x_dps =
        mpu6500_gyro_x / 131.0f;

    float raw_gyro_y_dps =
        mpu6500_gyro_y / 131.0f;

    float raw_gyro_z_dps =
        mpu6500_gyro_z / 131.0f;


    /* Aplicação dos offsets */

    mpu6500_gyro_x_dps =
        raw_gyro_x_dps - mpu6500_gyro_offset_x;

    mpu6500_gyro_y_dps =
        raw_gyro_y_dps - mpu6500_gyro_offset_y;

    mpu6500_gyro_z_dps =
        raw_gyro_z_dps - mpu6500_gyro_offset_z;

    return HAL_OK;
}


HAL_StatusTypeDef MPU6500_Calibrate(void)
{
    const uint16_t samples = 500;

    float sum_ax = 0.0f;
    float sum_ay = 0.0f;
    float sum_az = 0.0f;

    float sum_gx = 0.0f;
    float sum_gy = 0.0f;
    float sum_gz = 0.0f;

    HAL_StatusTypeDef status;

    /* Dá um pequeno tempo para o sensor estabilizar */
    App_Delay(100);

    for (uint16_t i = 0; i < samples; i++)
    {
        status = MPU6500_Read();

        if (status != HAL_OK)
            return status;

        sum_ax += mpu6500_accel_x_g;
        sum_ay += mpu6500_accel_y_g;
        sum_az += mpu6500_accel_z_g;

        sum_gx += mpu6500_gyro_x_dps;
        sum_gy += mpu6500_gyro_y_dps;
        sum_gz += mpu6500_gyro_z_dps;

        App_Delay(5);
    }

    /* Calcula a média */
    mpu6500_accel_offset_x =
        sum_ax / samples;

    mpu6500_accel_offset_y =
        sum_ay / samples;

    /*
     * Para Z, não queremos simplesmente remover toda a leitura.
     *
     * Em repouso, Z deve medir aproximadamente +1 g.
     *
     * Portanto o offset é:
     *
     * leitura média - 1 g
     */
    mpu6500_accel_offset_z =
        (sum_az / samples) - 1.0f;

    mpu6500_gyro_offset_x =
        sum_gx / samples;

    mpu6500_gyro_offset_y =
        sum_gy / samples;

    mpu6500_gyro_offset_z =
        sum_gz / samples;

    return HAL_OK;
}

