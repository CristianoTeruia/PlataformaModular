/**
 * @file bh1750.c
 * @brief Implementação do driver do BH1750 (ver bh1750.h).
 */
#include "bh1750.h"
#include <stddef.h>

#define BH1750_ADDR_LOW          0x23U
#define BH1750_ADDR_HIGH         0x5CU
#define BH1750_CMD_POWER_ON      0x01U
#define BH1750_CMD_ONE_TIME_H    0x20U /* alta resolucao (1 lx); desliga sozinho ao terminar */
#define BH1750_TIMEOUT_MS        50U
#define BH1750_MEASURE_MS        180U  /* tipico 120 ms, maximo 180 ms */

volatile uint8_t bh1750_addr = 0U;
volatile uint32_t bh1750_last_hal = 0U;
volatile uint16_t bh1750_last_raw = 0U;

/**
 * @brief Envia um comando de 1 byte ao BH1750.
 * @param[in] address Endereço de 7 bits do sensor.
 * @param[in] command Comando (Power On, medição única etc.).
 * @return Status da HAL.
 */
static HAL_StatusTypeDef WriteCommand(uint8_t address, uint8_t command)
{
    return HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(address << 1), &command, 1U, BH1750_TIMEOUT_MS);
}

HAL_StatusTypeDef BH1750_Read(uint16_t *raw)
{
    static const uint8_t candidates[2] = { BH1750_ADDR_LOW, BH1750_ADDR_HIGH };
    uint8_t data[2];
    HAL_StatusTypeDef status = HAL_ERROR;

    if (raw == NULL) return HAL_ERROR;

    /* Procura o sensor enquanto ele nao foi detectado (tambem apos ser removido). */
    if (bh1750_addr == 0U)
    {
        for (uint8_t i = 0U; i < 2U; i++)
        {
            status = WriteCommand(candidates[i], BH1750_CMD_POWER_ON);
            if (status == HAL_OK)
            {
                bh1750_addr = candidates[i];
                break;
            }
        }
        if (bh1750_addr == 0U)
        {
            bh1750_last_hal = (uint32_t)status;
            return status;
        }
    }

    /* Power On + medicao unica: o sensor volta ao repouso sozinho (consumo minimo). */
    status = WriteCommand(bh1750_addr, BH1750_CMD_POWER_ON);
    if (status == HAL_OK) status = WriteCommand(bh1750_addr, BH1750_CMD_ONE_TIME_H);
    if (status != HAL_OK)
    {
        bh1750_addr = 0U;
        bh1750_last_hal = (uint32_t)status;
        return status;
    }

    App_Delay(BH1750_MEASURE_MS); /* cede a CPU durante a conversao */

    status = HAL_I2C_Master_Receive(&hi2c1, (uint16_t)(bh1750_addr << 1), data, 2U, BH1750_TIMEOUT_MS);
    bh1750_last_hal = (uint32_t)status;
    if (status != HAL_OK)
    {
        bh1750_addr = 0U;
        return status;
    }

    *raw = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
    bh1750_last_raw = *raw;
    return HAL_OK;
}
