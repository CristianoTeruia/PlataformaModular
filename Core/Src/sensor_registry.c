/**
 * @file sensor_registry.c
 * @brief Tabela de sensores e funções de serialização e formatação CSV (ver sensor_registry.h).
 */
#include "sensor_registry.h"
#include "app_state.h"
#include "aht20.h"
#include "bmp280.h"
#include "mpu6500.h"
#include "bh1750.h"
#include "ens160.h"
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Utilitarios                                                               */
/* ------------------------------------------------------------------------- */
/** @brief Lê um inteiro de 16 bits sem sinal em little-endian. */
static uint16_t ReadLE16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/** @brief Lê um inteiro de 32 bits sem sinal em little-endian. */
static uint32_t ReadLE32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/** Escreve `<sinal><inteiro>.<decimais>` em buffer+current. */
static int AppendScaled(char *buffer, size_t capacity, int current,
                        int32_t value, uint32_t scale, uint8_t decimals)
{
    uint32_t magnitude;
    const char *sign = "";
    int64_t wide_value = value;
    if (current < 0) return -1;
    if (wide_value < 0)
    {
        sign = "-";
        wide_value = -wide_value;
    }
    magnitude = (uint32_t)wide_value;
    return snprintf(buffer + current, capacity - (size_t)current,
                    "%s%lu.%0*lu", sign,
                    (unsigned long)(magnitude / scale), (int)decimals,
                    (unsigned long)(magnitude % scale));
}

/** Acrescenta texto formatado em out+pos; retorna a nova posicao ou -1 se nao couber. */
static int Put(char *out, size_t capacity, int pos, int written)
{
    if ((pos < 0) || (written < 0)) return -1;
    if ((size_t)(pos + written) >= capacity) return -1;
    return pos + written;
}

/* ------------------------------------------------------------------------- */
/* AHT (AHT20 / AHT21: mesmo endereco e protocolo)                           */
/* ------------------------------------------------------------------------- */
/** @brief Lê o AHT20 e atualiza `aht20_available` e `aht20_last_read_ok`. */
static HAL_StatusTypeDef Aht_Read(void)
{
    HAL_StatusTypeDef st = AHT20_Read(&aht20_temperature, &aht20_humidity);
    aht20_last_read_ok = (st == HAL_OK) ? 1U : 0U;
    aht20_available = (st == HAL_OK);
    return st;
}

/** @brief Grava 4 bytes: temperatura (int16) e umidade (uint16), ambas em centésimos, little-endian. */
static void Aht_Serialize(uint8_t *out)
{
    /* Temperatura em centesimos de grau C e umidade em centesimos de %. */
    int16_t temperature = (int16_t)(aht20_temperature * 100.0f);
    uint16_t humidity = (uint16_t)(aht20_humidity * 100.0f);
    memcpy(&out[0], &temperature, sizeof(temperature));
    memcpy(&out[2], &humidity, sizeof(humidity));
}

/** @brief Colunas `aht_present,aht_temp_c,aht_humidity_pct`; com o sensor ausente, `0,,`. */
static int Aht_Csv(const uint8_t *p, char *out, size_t cap)
{
    int pos;
    if (p == NULL) return Put(out, cap, 0, snprintf(out, cap, "0,,"));
    pos = Put(out, cap, 0, snprintf(out, cap, "1,"));
    pos = Put(out, cap, pos, AppendScaled(out, cap, pos, (int32_t)(int16_t)ReadLE16(&p[0]), 100U, 2U));
    pos = Put(out, cap, pos, snprintf(out + pos, (pos < 0) ? 0U : cap - (size_t)pos, ","));
    pos = Put(out, cap, pos, AppendScaled(out, cap, pos, (int32_t)ReadLE16(&p[2]), 100U, 2U));
    return pos;
}

/* ------------------------------------------------------------------------- */
/* BMP280                                                                    */
/* ------------------------------------------------------------------------- */
/** @brief Lê a calibração e depois a temperatura e a pressão do BMP280; atualiza a disponibilidade. */
static HAL_StatusTypeDef Bmp_Read(void)
{
    HAL_StatusTypeDef st;
    if (BMP280_ReadCalibration() == HAL_OK)
    {
        st = BMP280_Read(&bmp280_temperature, &bmp280_pressure);
        bmp280_last_read_ok = (st == HAL_OK) ? 1U : 0U;
        bmp280_available = (st == HAL_OK);
        return st;
    }
    bmp280_last_read_ok = 0U;
    bmp280_available = false;
    return HAL_ERROR;
}

/** @brief Grava 4 bytes: pressão em Pa (uint32, parte inteira), little-endian. */
static void Bmp_Serialize(uint8_t *out)
{
    uint32_t pressure = (uint32_t)bmp280_pressure;
    memcpy(&out[0], &pressure, sizeof(pressure));
}

/** @brief Coluna `bmp_pressure`, em Pa inteiros. */
static int Bmp_Csv(const uint8_t *p, char *out, size_t cap)
{
    if (p == NULL) { if (cap > 0U) out[0] = '\0'; return 0; }
    return Put(out, cap, 0, snprintf(out, cap, "%lu", (unsigned long)ReadLE32(&p[0])));
}

/* ------------------------------------------------------------------------- */
/* MPU6500                                                                   */
/* ------------------------------------------------------------------------- */
/**
 * @brief Lê o MPU6500, que dorme entre as amostras.
 *
 * Se o sensor não estava disponível, reinicializa e recalibra (MPU6500_Init() e MPU6500_Calibrate())
 * antes de ler; caso contrário, acorda, lê e volta a dormir. Atualiza a disponibilidade.
 * Veja o aviso sobre recalibração em MPU6500_Calibrate().
 */
static HAL_StatusTypeDef Mpu_Read(void)
{
    HAL_StatusTypeDef st;
    if (!mpu6500_available)
    {
        /* Detecta/inicializa/calibra (o sensor fica acordado) e entao volta a dormir. */
        if ((MPU6500_Init() == HAL_OK) && (MPU6500_Calibrate() == HAL_OK))
        {
            st = MPU6500_Read();
            (void)MPU6500_Sleep();
            mpu6500_last_read_ok = (st == HAL_OK) ? 1U : 0U;
            mpu6500_available = (st == HAL_OK);
            return st;
        }
        mpu6500_last_read_ok = 0U;
        mpu6500_available = false;
        return HAL_ERROR;
    }
    /* Economia de energia: acorda, le e volta a dormir (o sensor consome alguns mA acordado). */
    st = MPU6500_Wake();
    if (st == HAL_OK) st = MPU6500_Read();
    (void)MPU6500_Sleep();
    mpu6500_last_read_ok = (st == HAL_OK) ? 1U : 0U;
    mpu6500_available = (st == HAL_OK);
    return st;
}

/** @brief Grava 12 bytes (int16, little-endian): aceleração X, Y, Z em milésimos de g e velocidade angular X, Y, Z em centésimos de °/s. */
static void Mpu_Serialize(uint8_t *out)
{
    /* Aceleracao em milesimos de g e giroscopio em centesimos de grau/s. */
    int16_t accel_x = (int16_t)(mpu6500_accel_x_g * 1000.0f);
    int16_t accel_y = (int16_t)(mpu6500_accel_y_g * 1000.0f);
    int16_t accel_z = (int16_t)(mpu6500_accel_z_g * 1000.0f);
    int16_t gyro_x = (int16_t)(mpu6500_gyro_x_dps * 100.0f);
    int16_t gyro_y = (int16_t)(mpu6500_gyro_y_dps * 100.0f);
    int16_t gyro_z = (int16_t)(mpu6500_gyro_z_dps * 100.0f);
    memcpy(&out[0], &accel_x, sizeof(accel_x));
    memcpy(&out[2], &accel_y, sizeof(accel_y));
    memcpy(&out[4], &accel_z, sizeof(accel_z));
    memcpy(&out[6], &gyro_x, sizeof(gyro_x));
    memcpy(&out[8], &gyro_y, sizeof(gyro_y));
    memcpy(&out[10], &gyro_z, sizeof(gyro_z));
}

/** @brief Seis colunas: aceleração com 3 casas decimais (g) e velocidade angular com 2 (°/s); ausente, `,,,,,`. */
static int Mpu_Csv(const uint8_t *p, char *out, size_t cap)
{
    static const uint32_t scale[6] = { 1000U, 1000U, 1000U, 100U, 100U, 100U };
    static const uint8_t decimals[6] = { 3U, 3U, 3U, 2U, 2U, 2U };
    int pos = 0;

    if (p == NULL) return Put(out, cap, 0, snprintf(out, cap, ",,,,,"));
    for (uint8_t i = 0U; i < 6U; i++)
    {
        if (i > 0U) pos = Put(out, cap, pos, snprintf(out + pos, (pos < 0) ? 0U : cap - (size_t)pos, ","));
        pos = Put(out, cap, pos, AppendScaled(out, cap, pos,
                  (int32_t)(int16_t)ReadLE16(&p[2U * i]), scale[i], decimals[i]));
    }
    return pos;
}

/* ------------------------------------------------------------------------- */
/* BH1750 (luminosidade): 2 bytes = contagem bruta; lux = bruto / 1,2           */
/* ------------------------------------------------------------------------- */
static uint16_t bh1750_raw_sample;

/** @brief Mede a iluminância do BH1750 e guarda a contagem bruta em `bh1750_raw_sample`. */
static HAL_StatusTypeDef Bh_Read(void)
{
    return BH1750_Read(&bh1750_raw_sample);
}

/** @brief Grava 2 bytes: contagem bruta de 16 bits, little-endian. */
static void Bh_Serialize(uint8_t *out)
{
    memcpy(&out[0], &bh1750_raw_sample, sizeof(bh1750_raw_sample));
}

/** @brief Coluna `bh_lux`: contagem bruta dividida por 1,2, com uma casa decimal e arredondamento. */
static int Bh_Csv(const uint8_t *p, char *out, size_t cap)
{
    uint32_t lux_x10;
    if (p == NULL) { if (cap > 0U) out[0] = '\0'; return 0; }
    lux_x10 = ((uint32_t)ReadLE16(&p[0]) * 100U + 6U) / 12U; /* bruto / 1,2 com 1 casa, arredondado */
    return Put(out, cap, 0, AppendScaled(out, cap, 0, (int32_t)lux_x10, 10U, 1U));
}

/* ------------------------------------------------------------------------- */
/* ENS160 (qualidade do ar): AQI(1) + TVOC ppb(2) + eCO2 ppm(2) + validade(1)  */
/* ------------------------------------------------------------------------- */
static ENS160_Sample ens160_sample;

/** @brief Lê o ENS160, compensado pela temperatura e umidade do AHT20 (lido antes, pela ordem da tabela). */
static HAL_StatusTypeDef Ens_Read(void)
{
    /* O AHT (lido antes, pela ordem da tabela) fornece temperatura e umidade para compensacao. */
    return ENS160_Read(aht20_available, aht20_temperature, aht20_humidity, &ens160_sample);
}

/** @brief Grava 6 bytes: AQI (1), TVOC em ppb (2), eCO2 em ppm (2) e validade (1). */
static void Ens_Serialize(uint8_t *out)
{
    out[0] = ens160_sample.aqi;
    memcpy(&out[1], &ens160_sample.tvoc_ppb, sizeof(ens160_sample.tvoc_ppb));
    memcpy(&out[3], &ens160_sample.eco2_ppm, sizeof(ens160_sample.eco2_ppm));
    out[5] = ens160_sample.validity;
}

/** @brief Colunas `ens_aqi,ens_tvoc_ppb,ens_eco2_ppm,ens_validity`; ausente, `,,,`. */
static int Ens_Csv(const uint8_t *p, char *out, size_t cap)
{
    if (p == NULL) return Put(out, cap, 0, snprintf(out, cap, ",,,"));
    return Put(out, cap, 0, snprintf(out, cap, "%u,%u,%u,%u", (unsigned int)p[0],
                                     (unsigned int)ReadLE16(&p[1]), (unsigned int)ReadLE16(&p[3]),
                                     (unsigned int)p[5]));
}

/* ------------------------------------------------------------------------- */
/* Tabela de sensores. A ORDEM define a ordem dos dados no registro e das     */
/* colunas do CSV. Os bits NAO podem mudar: registros ja gravados dependem deles. */
/* ------------------------------------------------------------------------- */
/**
 * @brief Tabela dos sensores do sistema.
 *
 * A ORDEM define a ordem dos dados no registro e das colunas do CSV. Os bits da máscara NÃO podem
 * mudar, porque os registros já gravados dependem deles.
 */
static const SensorDriver sensor_table[] =
{
    { "AHT",     SENSOR_AHT20,   4U,  Aht_Read, Aht_Serialize, "aht_present,aht_temp_c,aht_humidity_pct", Aht_Csv },
    { "BMP280",  SENSOR_BMP280,  4U,  Bmp_Read, Bmp_Serialize, "bmp_pressure", Bmp_Csv },
    { "MPU6500", SENSOR_MPU6500, 12U, Mpu_Read, Mpu_Serialize,
      "accel_x_g,accel_y_g,accel_z_g,gyro_x_dps,gyro_y_dps,gyro_z_dps", Mpu_Csv },
    { "BH1750",  SENSOR_BH1750,  2U,  Bh_Read,  Bh_Serialize,  "bh_lux", Bh_Csv },
    { "ENS160",  SENSOR_ENS160,  6U,  Ens_Read, Ens_Serialize, "ens_aqi,ens_tvoc_ppb,ens_eco2_ppm,ens_validity", Ens_Csv },
};
#define SENSOR_COUNT ((uint8_t)(sizeof(sensor_table) / sizeof(sensor_table[0])))

static volatile uint8_t sensors_available_mask = 0U;
static volatile uint8_t sensors_detected_mask = 0U;

uint8_t Sensors_Count(void) { return SENSOR_COUNT; }

const SensorDriver *Sensors_Get(uint8_t index)
{
    return (index < SENSOR_COUNT) ? &sensor_table[index] : NULL;
}

void Sensors_ReadOne(uint8_t index)
{
    if (index >= SENSOR_COUNT) return;
    HAL_StatusTypeDef result = sensor_table[index].read();
    if (result == HAL_OK)
    {
        sensors_available_mask |= sensor_table[index].mask_bit;
        sensors_detected_mask |= sensor_table[index].mask_bit;
    }
    else
    {
        sensors_available_mask &= (uint8_t)~sensor_table[index].mask_bit;
        /* HAL_BUSY = o sensor responde no barramento mas ainda nao entrega dado: conta como
         * detectado, para o OLED mostrar ERRO:<nome> em vez de ignorar um modulo conectado. */
        if (result == HAL_BUSY) sensors_detected_mask |= sensor_table[index].mask_bit;
    }
}

uint8_t Sensors_AvailableMask(void) { return sensors_available_mask; }
uint8_t Sensors_DetectedMask(void) { return sensors_detected_mask; }

void Sensors_UpdateDisplayValues(void)
{
    display_aht_temp = (int32_t)(aht20_temperature * 10.0f);
    display_aht_hum = (int32_t)(aht20_humidity * 10.0f);
    display_pressure = (int32_t)(bmp280_pressure / 10.0f);
    display_accel_z = (int32_t)mpu6500_accel_z;
}

/* ------------------------------------------------------------------------- */
/* Registro da EEPROM                                                        */
/* ------------------------------------------------------------------------- */
uint8_t Sensors_RecordSizeFromMask(uint8_t mask, uint8_t *size)
{
    uint8_t known = 0U;
    uint16_t calculated = 1U + 7U + 2U; /* mascara + RTC + CRC */

    for (uint8_t i = 0U; i < SENSOR_COUNT; i++) known |= sensor_table[i].mask_bit;
    if ((size == NULL) || (mask == 0U) || ((mask & (uint8_t)~known) != 0U)) return 0U;

    for (uint8_t i = 0U; i < SENSOR_COUNT; i++)
    {
        if ((mask & sensor_table[i].mask_bit) != 0U) calculated += sensor_table[i].payload_size;
    }
    if (calculated > EEPROM_MAX_RECORD_SIZE) return 0U;
    *size = (uint8_t)calculated;
    return 1U;
}

/* Grava os dados dos sensores da mascara a partir de buffer[index]; devolve o novo indice.
 * A mascara deve ter sido validada por Sensors_RecordSizeFromMask(). */
uint8_t Sensors_Serialize(uint8_t mask, uint8_t *buffer, uint8_t index)
{
    for (uint8_t i = 0U; i < SENSOR_COUNT; i++)
    {
        if ((mask & sensor_table[i].mask_bit) != 0U)
        {
            sensor_table[i].serialize(&buffer[index]);
            index = (uint8_t)(index + sensor_table[i].payload_size);
        }
    }
    return index;
}

/* ------------------------------------------------------------------------- */
/* CSV                                                                       */
/* ------------------------------------------------------------------------- */
int Sensors_CsvHeader(char *out, size_t capacity)
{
    int pos = Put(out, capacity, 0, snprintf(out, capacity, "sensor_mask,timestamp"));
    for (uint8_t i = 0U; i < SENSOR_COUNT; i++)
    {
        pos = Put(out, capacity, pos, snprintf(out + pos, (pos < 0) ? 0U : capacity - (size_t)pos,
                                               ",%s", sensor_table[i].csv_header));
    }
    pos = Put(out, capacity, pos, snprintf(out + pos, (pos < 0) ? 0U : capacity - (size_t)pos, "\r\n"));
    return pos;
}

int Sensors_CsvFormatRow(uint8_t mask, const uint8_t *payload, char *out, size_t capacity)
{
    int pos = 0;
    for (uint8_t i = 0U; i < SENSOR_COUNT; i++)
    {
        const uint8_t *p = ((mask & sensor_table[i].mask_bit) != 0U) ? payload : NULL;
        int n;
        if (i > 0U)
        {
            if ((size_t)(pos + 1) >= capacity) return -1;
            out[pos++] = ',';
        }
        n = sensor_table[i].csv_format(p, out + pos, capacity - (size_t)pos);
        if (n < 0) return -1;
        pos += n;
        if (p != NULL) payload += sensor_table[i].payload_size;
    }
    if ((size_t)pos >= capacity) return -1;
    out[pos] = '\0';
    return pos;
}

/* ------------------------------------------------------------------------- */
/* OLED                                                                      */
/* ------------------------------------------------------------------------- */
uint8_t Sensors_PopCount(uint8_t value)
{
    uint8_t count = 0U;
    while (value != 0U) { count = (uint8_t)(count + (value & 1U)); value >>= 1; }
    return count;
}

/* Primeiro sensor DETECTADO desde o boot que nao esta respondendo agora (ou NULL). */
const char *Sensors_FirstMissingName(uint8_t detected_mask, uint8_t present_mask)
{
    for (uint8_t i = 0U; i < SENSOR_COUNT; i++)
    {
        uint8_t bit = sensor_table[i].mask_bit;
        if (((detected_mask & bit) != 0U) && ((present_mask & bit) == 0U)) return sensor_table[i].name;
    }
    return NULL;
}
