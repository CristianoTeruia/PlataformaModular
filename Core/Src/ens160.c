/**
 * @file ens160.c
 * @brief Implementação do driver do ENS160 (ver ens160.h).
 *
 * O driver é uma máquina de estados enxuta: detecta o sensor, só o coloca em modo padrão se ele
 * não estiver medindo, espera dado novo e, após ciclos seguidos sem dado, reinicializa com RESET.
 */
#include "ens160.h"
#include <stddef.h>

#define ENS160_ADDR_HIGH        0x53U
#define ENS160_ADDR_LOW         0x52U
#define ENS160_PART_ID_EXPECTED 0x0160U

#define ENS160_REG_PART_ID      0x00U
#define ENS160_REG_OPMODE       0x10U
#define ENS160_REG_COMMAND      0x12U
#define ENS160_REG_TEMP_IN      0x13U /* 2 B, seguido de RH_IN (0x15, 2 B) */
#define ENS160_REG_STATUS       0x20U
#define ENS160_REG_DATA_T       0x30U /* T(2) + RH(2) usados pelo sensor nos calculos */
#define ENS160_REG_GPR_READ_4   0x4CU /* versao do firmware: maior, menor, build */
#define ENS160_REG_DATA_AQI     0x21U /* AQI(1) + TVOC(2) + eCO2(2), em sequencia */

#define ENS160_OPMODE_IDLE      0x01U
#define ENS160_OPMODE_STANDARD  0x02U
#define ENS160_OPMODE_RESET     0xF0U
#define ENS160_CMD_NOP          0x00U
#define ENS160_CMD_CLRGPR       0xCCU
#define ENS160_CMD_GET_APPVER   0x0EU

#define ENS160_STATUS_STATAS    0x80U /* modo de operacao em execucao */
#define ENS160_STATUS_STATER    0x40U /* erro */
#define ENS160_STATUS_NEWDAT    0x02U /* dado novo nos registradores DATA_* */

#define ENS160_TIMEOUT_MS       50U
#define ENS160_BOOT_MS          11U   /* o datasheet indica 10 ms; 11 ms e o valor seguro usado por outros drivers */
#define ENS160_MODE_MS          20U
#define ENS160_DATA_STEP_MS     100U  /* o modo STANDARD gera uma medicao por segundo */
#define ENS160_DATA_STEPS       16U   /* espera maxima por dado novo em regime: 1,6 s */
#define ENS160_FIRST_DATA_STEPS 32U   /* logo apos iniciar o modo STANDARD: 3,2 s */
#define ENS160_STUCK_CYCLES     3U    /* ciclos seguidos sem dado => reinicializa com RESET */

volatile uint8_t ens160_addr = 0U;
volatile uint16_t ens160_part_id = 0U;
volatile uint8_t ens160_last_status = 0U;
volatile uint8_t ens160_opmode = 0U;
volatile uint32_t ens160_init_count = 0U;
volatile uint32_t ens160_last_hal = 0U;
volatile uint32_t ens160_timeout_count = 0U;
volatile uint32_t ens160_error_count = 0U;
volatile uint32_t ens160_comm_fail_count = 0U;
volatile uint32_t ens160_reset_count = 0U;
volatile uint32_t ens160_zero_count = 0U;
volatile uint8_t ens160_fw_major = 0U, ens160_fw_minor = 0U, ens160_fw_build = 0U;
volatile uint16_t ens160_echo_t = 0U, ens160_echo_rh = 0U;
volatile uint16_t ens160_last_tvoc = 0U, ens160_last_eco2 = 0U;
volatile uint8_t ens160_last_aqi = 0U, ens160_last_validity = 0U;

static uint8_t init_failures = 0U;    /* tentativas sem sucesso: a proxima inicializacao usa RESET */
static uint8_t timeout_streak = 0U;   /* ciclos seguidos sem dado novo */
static bool need_init = false;

/**
 * @brief Lê registradores do ENS160.
 * @param[in]  reg    Registrador inicial.
 * @param[out] data   Destino.
 * @param[in]  length Quantidade de bytes.
 * @return Status da HAL.
 */
static HAL_StatusTypeDef RegRead(uint8_t reg, uint8_t *data, uint16_t length)
{
    return HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(ens160_addr << 1), reg, I2C_MEMADD_SIZE_8BIT,
                            data, length, ENS160_TIMEOUT_MS);
}

/**
 * @brief Escreve um registrador do ENS160.
 * @param[in] reg   Registrador.
 * @param[in] value Valor.
 * @return Status da HAL.
 */
static HAL_StatusTypeDef RegWrite(uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(ens160_addr << 1), reg, I2C_MEMADD_SIZE_8BIT,
                             &value, 1U, ENS160_TIMEOUT_MS);
}

/** Falha de comunicacao: esquece o endereco para procurar de novo no proximo ciclo. */
static HAL_StatusTypeDef CommFail(HAL_StatusTypeDef status)
{
    ens160_addr = 0U;
    ens160_last_hal = (uint32_t)status;
    ens160_comm_fail_count++;
    return status;
}

/** Sequencia de inicializacao usada pelos drivers de referencia: [RESET] -> IDLE -> limpa os
 * registradores de uso geral (NOP + CLRGPR, apaga o flag NEWGPR) -> STANDARD. */
static HAL_StatusTypeDef EnterStandardMode(bool with_reset)
{
    HAL_StatusTypeDef status;

    if (with_reset)
    {
        status = RegWrite(ENS160_REG_OPMODE, ENS160_OPMODE_RESET);
        if (status != HAL_OK) return status;
        ens160_reset_count++;
        App_Delay(ENS160_BOOT_MS);
    }
    status = RegWrite(ENS160_REG_OPMODE, ENS160_OPMODE_IDLE);
    if (status != HAL_OK) return status;
    App_Delay(ENS160_BOOT_MS);

    status = RegWrite(ENS160_REG_COMMAND, ENS160_CMD_NOP);
    if (status != HAL_OK) return status;
    status = RegWrite(ENS160_REG_COMMAND, ENS160_CMD_CLRGPR);
    if (status != HAL_OK) return status;
    App_Delay(ENS160_BOOT_MS);

    /* Le a versao do firmware do sensor (como o ESPHome). So diagnostico: falha aqui nao interrompe. */
    if (RegWrite(ENS160_REG_COMMAND, ENS160_CMD_GET_APPVER) == HAL_OK)
    {
        uint8_t version[3];
        App_Delay(ENS160_BOOT_MS);
        if (RegRead(ENS160_REG_GPR_READ_4, version, 3U) == HAL_OK)
        {
            ens160_fw_major = version[0];
            ens160_fw_minor = version[1];
            ens160_fw_build = version[2];
        }
    }

    status = RegWrite(ENS160_REG_OPMODE, ENS160_OPMODE_STANDARD);
    if (status != HAL_OK) return status;
    App_Delay(ENS160_MODE_MS);

    ens160_init_count++;
    return HAL_OK;
}

/** Le a temperatura e a umidade que o sensor informa estar usando (so diagnostico). */
static void ReadEcho(void)
{
    uint8_t e[4];
    if (RegRead(ENS160_REG_DATA_T, e, 4U) == HAL_OK)
    {
        ens160_echo_t = (uint16_t)((uint16_t)e[0] | ((uint16_t)e[1] << 8));
        ens160_echo_rh = (uint16_t)((uint16_t)e[2] | ((uint16_t)e[3] << 8));
    }
}

HAL_StatusTypeDef ENS160_Read(bool compensation_valid, float temperature_c, float humidity_pct,
                              ENS160_Sample *out)
{
    static const uint8_t candidates[2] = { ENS160_ADDR_HIGH, ENS160_ADDR_LOW };
    uint8_t data[5];
    uint8_t status_reg = 0U;
    HAL_StatusTypeDef status = HAL_ERROR;
    bool fresh_init = false;
    uint8_t max_steps;

    if (out == NULL) return HAL_ERROR;

    /* Detecta o sensor (confirmando o PART_ID) enquanto ele nao foi encontrado. */
    if (ens160_addr == 0U)
    {
        for (uint8_t i = 0U; i < 2U; i++)
        {
            ens160_addr = candidates[i];
            status = RegRead(ENS160_REG_PART_ID, data, 2U);
            if (status == HAL_OK)
            {
                ens160_part_id = (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
                if (ens160_part_id == ENS160_PART_ID_EXPECTED) break;
            }
            ens160_addr = 0U;
        }
        if (ens160_addr == 0U)
        {
            ens160_last_hal = (uint32_t)status;
            return HAL_ERROR;
        }
    }

    /* Coloca o sensor em STANDARD somente se ele NAO esta medindo. Um sensor que ja esta em STANDARD
     * (inclusive apos reiniciar a placa) nao e tocado. A 1a tentativa nao usa RESET; depois de uma
     * tentativa sem sucesso, a proxima usa RESET. */
    status = RegRead(ENS160_REG_OPMODE, data, 1U);
    if (status != HAL_OK) return CommFail(status);
    ens160_opmode = data[0];
    if ((data[0] != ENS160_OPMODE_STANDARD) || need_init)
    {
        status = EnterStandardMode(init_failures > 0U);
        if (status != HAL_OK) return CommFail(status);
        need_init = false;
        fresh_init = true;
    }

    /* Compensacao com a temperatura e umidade medidas (AHT): T = (C + 273,15) x 64, RH = % x 512. */
    if (compensation_valid)
    {
        float t = temperature_c;
        float rh = humidity_pct;
        uint16_t t_raw, rh_raw;
        if (t < -40.0f) t = -40.0f;
        if (t > 85.0f) t = 85.0f;
        if (rh < 0.0f) rh = 0.0f;
        if (rh > 100.0f) rh = 100.0f;
        t_raw = (uint16_t)((t + 273.15f) * 64.0f);
        rh_raw = (uint16_t)(rh * 512.0f);
        data[0] = (uint8_t)(t_raw & 0xFFU);
        data[1] = (uint8_t)(t_raw >> 8);
        data[2] = (uint8_t)(rh_raw & 0xFFU);
        data[3] = (uint8_t)(rh_raw >> 8);
        status = HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(ens160_addr << 1), ENS160_REG_TEMP_IN,
                                   I2C_MEMADD_SIZE_8BIT, data, 4U, ENS160_TIMEOUT_MS);
        if (status != HAL_OK) return CommFail(status);
    }

    /* Aguarda dado novo (NEWDAT). Em regime normal ele ja esta pronto; logo apos iniciar o modo
     * STANDARD a primeira medicao leva cerca de 1 s ou mais. A espera cede a CPU.
     * O bit STATAS (0x80) NAO e usado como criterio: neste modulo ele permanece desligado mesmo com
     * o sensor medindo (DEVICE_STATUS = 0x03). */
    max_steps = fresh_init ? ENS160_FIRST_DATA_STEPS : ENS160_DATA_STEPS;
    for (uint8_t step = 0U; ; step++)
    {
        status = RegRead(ENS160_REG_STATUS, &status_reg, 1U);
        if (status != HAL_OK) return CommFail(status);
        ens160_last_status = status_reg;
        ens160_last_hal = (uint32_t)HAL_OK;

        if ((status_reg & ENS160_STATUS_STATER) != 0U)
        {
            /* Erro interno do sensor: tenta de novo, desta vez com RESET. */
            ens160_error_count++;
            need_init = true;
            if (init_failures < 255U) init_failures++;
            return HAL_ERROR;
        }
        if ((status_reg & ENS160_STATUS_NEWDAT) != 0U) break;
        if (step >= max_steps)
        {
            ReadEcho();
            ens160_timeout_count++;
            if (fresh_init && (init_failures < 255U)) init_failures++; /* proxima inicializacao com RESET */
            if (++timeout_streak >= ENS160_STUCK_CYCLES)
            {
                /* Medindo ha varios ciclos sem entregar dado: reinicializa com RESET. */
                need_init = true;
                if (init_failures < 255U) init_failures++;
                timeout_streak = 0U;
            }
            return HAL_BUSY;
        }
        App_Delay(ENS160_DATA_STEP_MS);
    }

    status = RegRead(ENS160_REG_DATA_AQI, data, 5U);
    if (status != HAL_OK) return CommFail(status);

    out->aqi = (uint8_t)(data[0] & 0x07U);
    out->tvoc_ppb = (uint16_t)((uint16_t)data[1] | ((uint16_t)data[2] << 8));
    out->eco2_ppm = (uint16_t)((uint16_t)data[3] | ((uint16_t)data[4] << 8));
    out->validity = (uint8_t)((status_reg >> 2) & 0x03U);
    ReadEcho();

    if ((out->aqi == 0U) && (out->tvoc_ppb == 0U) && (out->eco2_ppm == 0U))
    {
        /* Nao e medicao: um ENS160 em operacao informa eCO2 >= 400 ppm e AQI de 1 a 5, mesmo em aquecimento.
         * Trata como "sem dado" (nao e gravado) e conta para uma eventual reinicializacao. */
        ens160_zero_count++;
        if (fresh_init && (init_failures < 255U)) init_failures++;
        if (++timeout_streak >= ENS160_STUCK_CYCLES)
        {
            need_init = true;
            if (init_failures < 255U) init_failures++;
            timeout_streak = 0U;
        }
        return HAL_BUSY;
    }

    init_failures = 0U;
    timeout_streak = 0U;
    ens160_last_aqi = out->aqi;
    ens160_last_tvoc = out->tvoc_ppb;
    ens160_last_eco2 = out->eco2_ppm;
    ens160_last_validity = out->validity;
    return HAL_OK;
}
