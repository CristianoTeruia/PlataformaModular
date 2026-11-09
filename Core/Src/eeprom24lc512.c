/**
 * @file eeprom24lc512.c
 * @brief Implementação do armazenamento nas EEPROMs 24LC512 e da exportação CSV (ver eeprom24lc512.h).
 *
 * Organização do arquivo: acesso por página (escrita e leitura), registro e ponteiro de gravação
 * (metadados), CRC-16, rotinas de teste manual e, ao final, a exportação CSV pela USB.
 */
#include "eeprom24lc512.h"
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include "usbd_cdc_if.h"
#include "i2c2_bus_lock.h"
#include "cmsis_os2.h"
#include "app_watchdog.h"
#include "sensor_registry.h"

/**
 * @brief Espera @p ms milissegundos cedendo a CPU quando o RTOS está em execução.
 *
 * `HAL_Delay()` é espera ocupada e, em uma tarefa de prioridade maior, deixa as demais sem executar.
 * @param[in] ms Tempo de espera, em milissegundos.
 */
static void EEPROM_YieldDelay(uint32_t ms)
{
    if (osKernelGetState() == osKernelRunning)
        (void)osDelay(ms);
    else
        HAL_Delay(ms);
}

HAL_StatusTypeDef EEPROM24LC512_WriteBuffer(
    uint16_t address,
    uint8_t *data,
    uint16_t size,
    uint16_t i2c_address)
{
    HAL_StatusTypeDef status;
    uint16_t bytes_to_write;
    uint16_t page_offset;

    /* Nenhuma operacao necessaria. */
    if (size == 0U)
    {
        return HAL_OK;
    }

    /* Validacao do ponteiro de dados. */
    if (data == NULL)
    {
        return HAL_ERROR;
    }

    /* Impede ultrapassar o limite fisico de 64 KiB. */
    if (((uint32_t)address + (uint32_t)size) > 65536UL)
    {
        return HAL_ERROR;
    }

    while (size > 0U)
    {
        /* Bytes restantes na pagina atual. */
        page_offset = address % EEPROM24LC512_PAGE_SIZE;
        bytes_to_write =
            EEPROM24LC512_PAGE_SIZE - page_offset;

        /* Nao escrever alem do tamanho solicitado. */
        if (bytes_to_write > size)
        {
            bytes_to_write = size;
        }

        status = I2C2_BusAcquire();
        if (status != HAL_OK) return status;
        status = HAL_I2C_Mem_Write(
            &hi2c2,
            i2c_address,
            address,
            I2C_MEMADD_SIZE_16BIT,
            data,
            bytes_to_write,
            100);
        I2C2_BusRelease();

        if (status != HAL_OK)
        {
            return status;
        }

        /* Aguarda o ciclo interno de gravacao da EEPROM. */
        EEPROM_YieldDelay(EEPROM24LC512_WRITE_DELAY);

        address += bytes_to_write;
        data += bytes_to_write;
        size -= bytes_to_write;
    }

    return HAL_OK;
}





HAL_StatusTypeDef EEPROM24LC512_ReadBuffer(
    uint16_t address,
    uint8_t *data,
    uint16_t size,
    uint16_t i2c_address)
{
    /* Nenhuma operacao necessaria. */
    if (size == 0U)
    {
        return HAL_OK;
    }

    /* Validacao do ponteiro de destino. */
    if (data == NULL)
    {
        return HAL_ERROR;
    }

    /* Impede ultrapassar o limite fisico de 64 KiB. */
    if (((uint32_t)address + (uint32_t)size) > 65536UL)
    {
        return HAL_ERROR;
    }

    HAL_StatusTypeDef status = I2C2_BusAcquire();
    i2c2_last_lock_status = (uint8_t)status;
    if (status != HAL_OK)
    {
        eeprom_read_error_snapshot = hi2c2.ErrorCode;
        eeprom_read_state_snapshot = (uint32_t)hi2c2.State;
        return status;
    }

    status = HAL_I2C_Mem_Read(
        &hi2c2,
        i2c_address,
        address,
        I2C_MEMADD_SIZE_16BIT,
        data,
        size,
        100);
    /* Captura imediatamente: outros acessos podem alterar os campos HAL depois. */
    eeprom_read_error_snapshot = hi2c2.ErrorCode;
    eeprom_read_state_snapshot = (uint32_t)hi2c2.State;
    I2C2_BusRelease();
    return status;
}




HAL_StatusTypeDef EEPROM24LC512_WriteByte(
    uint16_t address,
    uint8_t data)
{
    return EEPROM24LC512_WriteBuffer(
        address,
        &data,
        1,
		EEPROM24LC512_ADDRESS_1
    );
}



HAL_StatusTypeDef EEPROM24LC512_ReadByte(
    uint16_t address,
    uint8_t *data)
{
    return EEPROM24LC512_ReadBuffer(
        address,
        data,
        1,
	    EEPROM24LC512_ADDRESS_1
    );
}


HAL_StatusTypeDef EEPROM24LC512_WriteRecord(
    uint16_t address,
    uint8_t *data,
    uint8_t size)
{
    return EEPROM24LC512_WriteBuffer(
        address,
        data,
        size,
		EEPROM24LC512_ADDRESS_1
    );
}




HAL_StatusTypeDef EEPROM24LC512_ReadRecord(
    uint16_t address,
    SensorRecordEEPROM *record)
{
    if (record == NULL)
    {
        return HAL_ERROR;
    }

    return EEPROM24LC512_ReadBuffer(
        address,
        (uint8_t *)record,
        (uint16_t)sizeof(*record),
        EEPROM24LC512_ADDRESS_1);
}



void EEPROM_CreateSensorRecord(SensorRecordEEPROM *record)
{
    record->temperature_aht =
        (int16_t)(aht20_temperature * 100.0f);

    record->humidity_aht =
        (uint16_t)(aht20_humidity * 100.0f);

    record->temperature_bmp =
        (int16_t)(bmp280_temperature * 100.0f);

    record->pressure_bmp =
        (uint32_t)(bmp280_pressure);

    record->accel_x =
        (int16_t)(mpu6500_accel_x_g * 1000.0f);

    record->accel_y =
        (int16_t)(mpu6500_accel_y_g * 1000.0f);

    record->accel_z =
        (int16_t)(mpu6500_accel_z_g * 1000.0f);

    record->gyro_x =
        (int16_t)(mpu6500_gyro_x_dps * 100.0f);

    record->gyro_y =
        (int16_t)(mpu6500_gyro_y_dps * 100.0f);

    record->gyro_z =
        (int16_t)(mpu6500_gyro_z_dps * 100.0f);
}



HAL_StatusTypeDef EEPROM_SaveSensorRecord(void)
{
    HAL_StatusTypeDef status;
    uint8_t sensor_mask = 0U;
    uint8_t record_buffer[EEPROM_MAX_RECORD_SIZE];
    uint8_t verify_buffer[EEPROM_MAX_RECORD_SIZE];
    uint8_t record_size = 0U;
    uint16_t stored_crc;
    uint16_t calculated_crc;
    uint16_t target_i2c_address;
    uint16_t target_address;
    uint8_t target_eeprom;
    uint32_t end_address;

    /* So grava depois que os metadados foram carregados/validados. */
    diagnostic_export_stage = 2U; /* validacao dos metadados */
    if (eeprom_metadata_load_ok == 0U)
    {
        eeprom_status_display = 1U;
        return HAL_ERROR;
    }

    /*
     * Recuperacao de metadados legados: em versoes anteriores, full=1
     * podia significar apenas que a EEPROM 1 estava cheia. Se o seletor
     * ainda aponta para a EEPROM 1, migra para a EEPROM 2 antes de bloquear.
     * A EEPROM so e considerada totalmente cheia quando o seletor aponta
     * para a EEPROM 2 e full=1.
     */
    if ((eeprom_full != 0U) &&
        (eeprom_metadata.reserved == EEPROM_ACTIVE_EEPROM1))
    {
        eeprom_full = 0U;
        eeprom_metadata.full = 0U;
        eeprom_metadata.reserved = EEPROM_ACTIVE_EEPROM2;
        eeprom_metadata.next_address = EEPROM2_DATA_START_ADDRESS;
        eeprom_next_address = EEPROM2_DATA_START_ADDRESS;

        status = EEPROM_SaveMetadata();
        if (status != HAL_OK)
        {
            eeprom_status_display = 1U;
            return status;
        }
    }

    if (eeprom_full != 0U)
    {
        eeprom_status_display = 2U;
        return HAL_ERROR;
    }

    sensor_mask = Sensors_AvailableMask(); /* sensores que responderam neste ciclo */

    if (sensor_mask == 0U)
    {
        eeprom_status_display = 1U;
        return HAL_ERROR;
    }

    status = EEPROM_BuildSensorRecord(record_buffer, &record_size, sensor_mask);
    if (status != HAL_OK || record_size == 0U || record_size > EEPROM_MAX_RECORD_SIZE)
    {
        eeprom_status_display = 1U;
        return (status == HAL_OK) ? HAL_ERROR : status;
    }

    /* Estado atual persistido: 0 = EEPROM 1; 1 = EEPROM 2. */
    target_eeprom = eeprom_metadata.reserved;
    target_address = eeprom_next_address;

    if (target_eeprom == EEPROM_ACTIVE_EEPROM1)
    {
        target_i2c_address = EEPROM24LC512_ADDRESS_1;
    }
    else if (target_eeprom == EEPROM_ACTIVE_EEPROM2)
    {
        target_i2c_address = EEPROM24LC512_ADDRESS_2;
    }
    else
    {
        eeprom_status_display = 1U;
        return HAL_ERROR;
    }

    /* Se o registro nao couber na EEPROM 1, tenta a EEPROM 2 sem dividir o registro. */
    end_address = (uint32_t)target_address + (uint32_t)record_size;
    if (end_address > EEPROM24LC512_CAPACITY)
    {
        if (target_eeprom == EEPROM_ACTIVE_EEPROM1)
        {
            target_eeprom = EEPROM_ACTIVE_EEPROM2;
            target_i2c_address = EEPROM24LC512_ADDRESS_2;
            target_address = EEPROM2_DATA_START_ADDRESS;
            end_address = (uint32_t)target_address + (uint32_t)record_size;
        }
        else
        {
            /* EEPROM 2 tambem nao tem espaco para o registro completo. */
            eeprom_full = 1U;
            eeprom_metadata.full = 1U;
            eeprom_metadata.reserved = EEPROM_ACTIVE_EEPROM2;
            eeprom_metadata.next_address = 0xFFFFU;
            eeprom_status_display = 2U;
            status = EEPROM_SaveMetadata();
            if (status != HAL_OK)
            {
                eeprom_status_display = 1U;
                return status;
            }
            return HAL_ERROR;
        }
    }

    /* Grava o registro inteiro em um unico chip. */
    status = EEPROM24LC512_WriteBuffer(
        target_address, record_buffer, record_size, target_i2c_address);
    if (status != HAL_OK)
    {
        eeprom_status_display = 1U;
        return status;
    }

    /* Leitura de verificacao no mesmo chip e endereco usados na gravacao. */
    status = EEPROM24LC512_ReadBuffer(
        target_address, verify_buffer, record_size, target_i2c_address);
    if (status != HAL_OK)
    {
        eeprom_status_display = 1U;
        return status;
    }

    if (memcmp(record_buffer, verify_buffer, record_size) != 0)
    {
        eeprom_status_display = 1U;
        return HAL_ERROR;
    }

    stored_crc = (uint16_t)verify_buffer[record_size - 2U] |
                 ((uint16_t)verify_buffer[record_size - 1U] << 8);
    calculated_crc = EEPROM_CalculateCRC16(verify_buffer, (uint16_t)(record_size - 2U));
    if (stored_crc != calculated_crc)
    {
        eeprom_status_display = 1U;
        return HAL_ERROR;
    }

    /* Avanca o ponteiro. Se a EEPROM 1 terminou, passa para a EEPROM 2. */
    if (end_address == EEPROM24LC512_CAPACITY)
    {
        if (target_eeprom == EEPROM_ACTIVE_EEPROM1)
        {
            target_eeprom = EEPROM_ACTIVE_EEPROM2;
            target_address = EEPROM2_DATA_START_ADDRESS;
        }
        else
        {
            eeprom_full = 1U;
            target_address = 0xFFFFU; /* sentinela; full indica que nao ha mais espaco */
        }
    }
    else
    {
        target_address = (uint16_t)end_address;
    }

    eeprom_metadata.reserved = target_eeprom;
    eeprom_metadata.next_address = target_address;
    eeprom_metadata.full = eeprom_full;
    eeprom_next_address = target_address;

    status = EEPROM_SaveMetadata();
    if (status != HAL_OK)
    {
        /* O registro ja foi escrito; nao reportar sucesso se o ponteiro nao persistiu. */
        eeprom_status_display = 1U;
        return status;
    }

    eeprom_status_display = (eeprom_full != 0U) ? 2U : 0U;
    return HAL_OK;
}


void EEPROM24C512_ReadFullMemory(void)
{
    for (uint32_t address = 0; address < 65536; address += 64)
    {
        eeprom_full_read_address = (uint16_t)address;

        eeprom_full_read_status =
            EEPROM24LC512_ReadBuffer(
                eeprom_full_read_address,
                eeprom_full_read_buffer,
                64,
			    EEPROM24LC512_ADDRESS_1
            );

        if (eeprom_full_read_status != HAL_OK)
        {
            return;
        }

        /*
         * BREAKPOINT AQUI
         *
         * eeprom_full_read_address:
         * endereço inicial do bloco
         *
         * eeprom_full_read_buffer[0..63]:
         * 64 bytes lidos
         *
         * eeprom_full_read_status:
         * deve ser HAL_OK
         */
    }
}


HAL_StatusTypeDef EEPROM_SaveMetadata(void)
{
    return EEPROM24LC512_WriteBuffer(
        EEPROM_METADATA_ADDRESS,
        (uint8_t *)&eeprom_metadata,
        sizeof(EEPROMMetadata),
		EEPROM24LC512_ADDRESS_1
    );
}


HAL_StatusTypeDef EEPROM_LoadMetadata(void)
{
    HAL_StatusTypeDef status;
    EEPROMMetadata temp;
    const uint8_t *raw_metadata;
    uint8_t blank = 1U;

    eeprom_metadata_load_ok = 0U;

    status = EEPROM24LC512_ReadBuffer(
        EEPROM_METADATA_ADDRESS,
        (uint8_t *)&temp,
        (uint16_t)sizeof(EEPROMMetadata),
        EEPROM24LC512_ADDRESS_1);
    if (status != HAL_OK)
    {
        return status;
    }

    raw_metadata = (const uint8_t *)&temp;
    for (uint16_t i = 0U; i < (uint16_t)sizeof(EEPROMMetadata); i++)
    {
        if (raw_metadata[i] != 0xFFU)
        {
            blank = 0U;
            break;
        }
    }

    if (blank != 0U)
    {
        /* Primeira utilizacao: metadados em branco; dados comecam na EEPROM 1. */
        eeprom_metadata.next_address = EEPROM_DATA_START_ADDRESS;
        eeprom_metadata.full = 0U;
        eeprom_metadata.reserved = EEPROM_ACTIVE_EEPROM1;
        eeprom_metadata.magic = EEPROM_METADATA_MAGIC;
        eeprom_next_address = EEPROM_DATA_START_ADDRESS;
        eeprom_full = 0U;

        status = EEPROM_SaveMetadata();
        if (status == HAL_OK)
        {
            eeprom_metadata_load_ok = 1U;
        }
        return status;
    }

    if (temp.magic != EEPROM_METADATA_MAGIC)
    {
        /* Nao reinicializar metadados invalidos: isso poderia perder registros. */
        return HAL_ERROR;
    }

    if ((temp.full > 1U) || (temp.reserved > EEPROM_ACTIVE_EEPROM2))
    {
        return HAL_ERROR;
    }

    /*
     * Compatibilidade com a versao anterior: full=1 e reserved=0 significava
     * que a EEPROM 1 havia sido marcada cheia. Nesta versao, continua-se na EEPROM 2.
     */
    if ((temp.full == 1U) && (temp.reserved == EEPROM_ACTIVE_EEPROM1))
    {
        temp.full = 0U;
        temp.reserved = EEPROM_ACTIVE_EEPROM2;
        temp.next_address = EEPROM2_DATA_START_ADDRESS;

        eeprom_metadata = temp;
        eeprom_next_address = EEPROM2_DATA_START_ADDRESS;
        eeprom_full = 0U;
        status = EEPROM_SaveMetadata();
        if (status != HAL_OK)
        {
            return status;
        }
    }
    else
    {
        if (temp.full == 0U)
        {
            if (temp.reserved == EEPROM_ACTIVE_EEPROM1)
            {
                if (temp.next_address < EEPROM_DATA_START_ADDRESS)
                {
                    return HAL_ERROR;
                }
            }
            /* EEPROM 2 pode usar desde o endereco 0x0000. */
        }
        else
        {
            /* Memoria totalmente cheia usa sentinela 0xFFFF e chip 2 ativo. */
            if ((temp.reserved != EEPROM_ACTIVE_EEPROM2) ||
                (temp.next_address != 0xFFFFU))
            {
                return HAL_ERROR;
            }
        }

        eeprom_metadata = temp;
        eeprom_next_address = temp.next_address;
        eeprom_full = temp.full;
    }

    eeprom_metadata_load_ok = 1U;
    return HAL_OK;
}


uint16_t EEPROM_CalculateCRC16(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFFU;

    if (data == NULL)
    {
        return 0U;
    }

    for (uint16_t i = 0; i < length; i++)
    {
        crc ^= (uint16_t)data[i] << 8;

        for (uint8_t bit = 0; bit < 8U; bit++)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            }
            else
            {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}


HAL_StatusTypeDef EEPROM_Test_CRC16(void)
{
    static const uint8_t test_data[] = "123456789";
    const uint16_t expected_crc = 0x29B1U;
    const uint16_t calculated_crc = EEPROM_CalculateCRC16(
        test_data,
        (uint16_t)(sizeof(test_data) - 1U)
    );

    return (calculated_crc == expected_crc) ? HAL_OK : HAL_ERROR;
}


HAL_StatusTypeDef EEPROM_BuildSensorRecord(
    uint8_t *buffer,
    uint8_t *record_size,
    uint8_t sensor_mask
)
{
    uint8_t index = 0;
    RTC_TimeTypeDef record_time;
    RTC_DateTypeDef record_date;
    uint16_t crc;
    uint16_t full_year;

    if (buffer == NULL || record_size == NULL || sensor_mask == 0U)
    {
        return HAL_ERROR;
    }

    /* Reserva um limite explícito para não ultrapassar o buffer. */
    memset(buffer, 0, EEPROM_MAX_RECORD_SIZE);

    /* Primeiro byte: máscara dos sensores presentes. */
    buffer[index++] = sensor_mask;

    /* Dados dos sensores presentes, na ordem da tabela (sensor_registry.c). */
    {
        uint8_t expected_size;
        if (Sensors_RecordSizeFromMask(sensor_mask, &expected_size) == 0U)
        {
            return HAL_ERROR;
        }
        index = Sensors_Serialize(sensor_mask, buffer, index);
    }

    /*
     * Lê o RTC imediatamente antes de completar o registro.
     * No STM32 HAL, a leitura de Time deve ser seguida pela leitura de Date.
     * Se qualquer leitura falhar, não grava um registro com calendário inválido.
     */
    App_RTC_Lock();
    HAL_StatusTypeDef rtc_status = HAL_RTC_GetTime(&hrtc, &record_time, RTC_FORMAT_BIN);
    if (rtc_status == HAL_OK)
    {
        rtc_status = HAL_RTC_GetDate(&hrtc, &record_date, RTC_FORMAT_BIN);
    }
    App_RTC_Unlock();
    if (rtc_status != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Validação básica dos campos do calendário antes de serializá-los. */
    if (record_time.Hours > 23U ||
        record_time.Minutes > 59U ||
        record_time.Seconds > 59U ||
        record_date.Month < 1U || record_date.Month > 12U ||
        record_date.Date < 1U || record_date.Date > 31U)
    {
        return HAL_ERROR;
    }

    /* Ano completo em dois bytes (HAL RTC Year normalmente representa 00–99). */
    full_year = (uint16_t)(2000U + record_date.Year);
    buffer[index++] = (uint8_t)(full_year & 0xFFU);
    buffer[index++] = (uint8_t)((full_year >> 8) & 0xFFU);
    buffer[index++] = record_date.Month;
    buffer[index++] = record_date.Date;
    buffer[index++] = record_time.Hours;
    buffer[index++] = record_time.Minutes;
    buffer[index++] = record_time.Seconds;

    /* CRC-16/CCITT-FALSE sobre todos os bytes anteriores ao próprio CRC. */
    if ((uint16_t)index + 2U > EEPROM_MAX_RECORD_SIZE)
    {
        return HAL_ERROR;
    }

    crc = EEPROM_CalculateCRC16(buffer, index);
    buffer[index++] = (uint8_t)(crc & 0xFFU);
    buffer[index++] = (uint8_t)((crc >> 8) & 0xFFU);

    *record_size = index;
    return HAL_OK;
}



HAL_StatusTypeDef EEPROM24LC512_EraseAll(void)
{
    HAL_StatusTypeDef status;
    uint8_t erase_buffer[EEPROM24LC512_PAGE_SIZE];
    const uint16_t chips[] = {
        EEPROM24LC512_ADDRESS_1,
        EEPROM24LC512_ADDRESS_2
    };

    /* Sobrescreve todas as paginas de ambos os chips com 0xFF. */
    memset(erase_buffer, 0xFF, sizeof(erase_buffer));

    for (uint8_t chip = 0U; chip < 2U; chip++)
    {
        for (uint32_t address = 0U; address < EEPROM24LC512_CAPACITY;
             address += EEPROM24LC512_PAGE_SIZE)
        {
            Watchdog_Service(); /* limpeza dura varios segundos */
            status = EEPROM24LC512_WriteBuffer(
                (uint16_t)address,
                erase_buffer,
                EEPROM24LC512_PAGE_SIZE,
                chips[chip]);

            if (status != HAL_OK)
            {
                eeprom_metadata_load_ok = 0U;
                return status;
            }
        }
    }

    /*
     * A EEPROM 1 agora esta em branco. Grava metadados iniciais validos
     * (incluindo magic) para que a inicializacao retome em 0x0100.
     */
    memset(&eeprom_metadata, 0, sizeof(eeprom_metadata));
    eeprom_metadata.next_address = EEPROM_DATA_START_ADDRESS;
    eeprom_metadata.full = 0U;
    eeprom_metadata.reserved = EEPROM_ACTIVE_EEPROM1;
    eeprom_metadata.magic = EEPROM_METADATA_MAGIC;
    eeprom_next_address = EEPROM_DATA_START_ADDRESS;
    eeprom_full = 0U;
    eeprom_metadata_load_ok = 0U;

    status = EEPROM_SaveMetadata();
    if (status != HAL_OK)
    {
        return status;
    }

    eeprom_metadata_load_ok = 1U;
    eeprom_status_display = 0U;
    return HAL_OK;
}


HAL_StatusTypeDef EEPROM2_Test(void)
{
    HAL_StatusTypeDef status;

    uint16_t start_address = 0x0070;
    uint8_t write_data[32];
    uint8_t read_data[32];

    /* Gera um padrão de dados conhecido */
    for (uint8_t i = 0; i < sizeof(write_data); i++)
    {
        write_data[i] = (uint8_t)(0x40 + i);
    }

    /* Escreve 32 bytes atravessando o limite da página */
    status = EEPROM24LC512_WriteBuffer(
        start_address,
        write_data,
        sizeof(write_data),
        EEPROM24LC512_ADDRESS_2
    );

    if (status != HAL_OK)
    {
        return status;
    }

    /* Lê os mesmos 32 bytes */
    status = EEPROM24LC512_ReadBuffer(
        start_address,
        read_data,
        sizeof(read_data),
        EEPROM24LC512_ADDRESS_2
    );

    if (status != HAL_OK)
    {
        return status;
    }

    /* Compara os dados gravados com os dados lidos */
    for (uint8_t i = 0; i < sizeof(write_data); i++)
    {
        if (read_data[i] != write_data[i])
        {
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}



HAL_StatusTypeDef EEPROM_Test_Independence(void)
{
    HAL_StatusTypeDef status;

    uint16_t test_address = 0x0020;

    uint8_t eeprom1_write = 0xA5;
    uint8_t eeprom2_write = 0x5A;

    uint8_t eeprom1_read = 0x00;
    uint8_t eeprom2_read = 0x00;

    /* 1. Escreve na EEPROM 1 */
    status = EEPROM24LC512_WriteBuffer(
        test_address,
        &eeprom1_write,
        1,
        EEPROM24LC512_ADDRESS_1
    );

    if (status != HAL_OK)
    {
        return status;
    }

    /* 2. Escreve na EEPROM 2 */
    status = EEPROM24LC512_WriteBuffer(
        test_address,
        &eeprom2_write,
        1,
        EEPROM24LC512_ADDRESS_2
    );

    if (status != HAL_OK)
    {
        return status;
    }

    /* 3. Lê a EEPROM 1 */
    status = EEPROM24LC512_ReadBuffer(
        test_address,
        &eeprom1_read,
        1,
        EEPROM24LC512_ADDRESS_1
    );

    if (status != HAL_OK)
    {
        return status;
    }

    /* 4. Lê a EEPROM 2 */
    status = EEPROM24LC512_ReadBuffer(
        test_address,
        &eeprom2_read,
        1,
        EEPROM24LC512_ADDRESS_2
    );

    if (status != HAL_OK)
    {
        return status;
    }

    /* 5. Verifica se os dados são independentes */
    if (eeprom1_read != eeprom1_write)
    {
        return HAL_ERROR;
    }

    if (eeprom2_read != eeprom2_write)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}





HAL_StatusTypeDef EEPROM_Test_AddressBoundaries(void)
{
    const uint16_t test_addresses[] = {
        0x0000,
        0x0100,
        0x1000,
        0x8000,
        0xFFFF
    };

    const uint8_t test_values[] = {
        0x12,
        0x34,
        0x56,
        0x78,
        0x9A
    };

    uint8_t read_value;
    HAL_StatusTypeDef status;
    uint16_t i;

    for (i = 0; i < 5; i++)
    {
        status = EEPROM24LC512_WriteBuffer(
            test_addresses[i],
            (uint8_t *)&test_values[i],
            1,
            EEPROM24LC512_ADDRESS_2
        );

        if (status != HAL_OK)
            return status;

        status = EEPROM24LC512_ReadBuffer(
            test_addresses[i],
            &read_value,
            1,
            EEPROM24LC512_ADDRESS_2
        );

        if (status != HAL_OK)
            return status;

        if (read_value != test_values[i])
            return HAL_ERROR;
    }

    return HAL_OK;
}



HAL_StatusTypeDef EEPROM_Test_PageBoundary(void)
{
    uint8_t tx[32];
    uint8_t rx[32];
    HAL_StatusTypeDef status;
    uint16_t i;

    for (i = 0; i < 32; i++)
        tx[i] = (uint8_t)(i + 1);

    status = EEPROM24LC512_WriteBuffer(
        0x0070,
        tx,
        sizeof(tx),
        EEPROM24LC512_ADDRESS_2
    );

    if (status != HAL_OK)
        return status;

    status = EEPROM24LC512_ReadBuffer(
        0x0070,
        rx,
        sizeof(rx),
        EEPROM24LC512_ADDRESS_2
    );

    if (status != HAL_OK)
        return status;

    for (i = 0; i < 32; i++)
    {
        if (rx[i] != tx[i])
            return HAL_ERROR;
    }

    return HAL_OK;
}



HAL_StatusTypeDef EEPROM_Test_SerializedRecordRoundTrip(void)
{
    HAL_StatusTypeDef status;

    // Endereco reservado para teste na EEPROM 2
    const uint16_t test_address = 0x0200;

    uint8_t tx_buffer[EEPROM_MAX_RECORD_SIZE];
    uint8_t rx_buffer[EEPROM_MAX_RECORD_SIZE];
    uint8_t record_size = 0;
    uint8_t sensor_mask = 0;

    // Identifica os sensores disponiveis
    if (aht20_available)
    {
        sensor_mask |= SENSOR_AHT20;
    }

    if (bmp280_available)
    {
        sensor_mask |= SENSOR_BMP280;
    }

    if (mpu6500_available)
    {
        sensor_mask |= SENSOR_MPU6500;
    }

    // Nao ha dados validos para montar o registro
    if (sensor_mask == 0)
    {
        return HAL_ERROR;
    }

    // 1. Monta o registro serializado na RAM
    status = EEPROM_BuildSensorRecord(
        tx_buffer,
        &record_size,
        sensor_mask
    );

    if (status != HAL_OK)
    {
        return status;
    }

    if (record_size == 0 ||
        record_size > EEPROM_MAX_RECORD_SIZE)
    {
        return HAL_ERROR;
    }

    // 2. Grava o registro na EEPROM 2
    status = EEPROM24LC512_WriteBuffer(
        test_address,
        tx_buffer,
        record_size,
        EEPROM24LC512_ADDRESS_2
    );

    if (status != HAL_OK)
    {
        return status;
    }

    // 3. Le o registro gravado
    status = EEPROM24LC512_ReadBuffer(
        test_address,
        rx_buffer,
        record_size,
        EEPROM24LC512_ADDRESS_2
    );

    if (status != HAL_OK)
    {
        return status;
    }

    // 4. Compara cada byte
    for (uint8_t i = 0; i < record_size; i++)
    {
        if (tx_buffer[i] != rx_buffer[i])
        {
            return HAL_ERROR;
        }
    }

    /* O registro deve conter ao menos máscara, calendário e CRC. */
    if (record_size < 10U)
    {
        return HAL_ERROR;
    }

    /* CRC gravado nos dois últimos bytes, em little-endian. */
    {
        uint16_t stored_crc = (uint16_t)rx_buffer[record_size - 2U] |
                              ((uint16_t)rx_buffer[record_size - 1U] << 8);
        uint16_t calculated_crc = EEPROM_CalculateCRC16(
            rx_buffer,
            (uint16_t)(record_size - 2U)
        );

        if (stored_crc != calculated_crc)
        {
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}



/* -------------------------------------------------------------------------
 * Exportacao CSV pela USB CDC.
 * Formato armazenado na EEPROM nao e alterado por esta rotina.
 * ------------------------------------------------------------------------- */
/**
 * @brief Envia um texto pela USB CDC e espera a transmissão terminar.
 *
 * Repete a tentativa enquanto o endpoint está ocupado (até 5 s), cedendo a CPU e realimentando o
 * watchdog. Atualiza `diagnostic_usb_tx_result`.
 * @param[in] text Texto terminado em zero; o buffer deve permanecer válido durante a chamada.
 * @retval HAL_OK      Texto enviado.
 * @retval HAL_ERROR   @p text nulo ou erro da USB.
 * @retval HAL_TIMEOUT Transmissão não concluída em 5 s.
 */
static HAL_StatusTypeDef EEPROM_USB_SendText(const char *text)
{
    uint32_t waited_ms = 0U;
    uint16_t length;
    uint8_t tx_status;

    if (text == NULL) { diagnostic_usb_tx_result = 2; return HAL_ERROR; }
    length = (uint16_t)strlen(text);

    while (waited_ms < 5000U)
    {
        tx_status = CDC_Transmit_FS((uint8_t *)text, length);
        diagnostic_usb_tx_result = (tx_status == USBD_OK) ? 0 : ((tx_status == USBD_BUSY) ? 1 : 2);
        if (tx_status == USBD_OK)
        {
            /* Mantem o buffer valido ate a conclusao da transferencia USB. */
            waited_ms = 0U;
            while (CDC_IsTxBusy() != 0U)
            {
                if (waited_ms++ >= 5000U) { diagnostic_usb_tx_result = 2; return HAL_TIMEOUT; }
                EEPROM_YieldDelay(1U);
                Watchdog_Service();
            }
            diagnostic_usb_tx_result = 0;
            return HAL_OK;
        }
        if (tx_status != USBD_BUSY) { diagnostic_usb_tx_result = 2; return HAL_ERROR; }
        EEPROM_YieldDelay(1U);
        Watchdog_Service();
        waited_ms++;
    }
    diagnostic_usb_tx_result = 2;
    return HAL_TIMEOUT;
}

/** @brief Atalho para Sensors_RecordSizeFromMask(). */
static uint8_t EEPROM_RecordSizeFromMask(uint8_t mask, uint8_t *size)
{
    return Sensors_RecordSizeFromMask(mask, size);
}

/**
 * @brief Confere o CRC de um registro.
 * @param[in] record Registro completo, com o CRC nos 2 últimos bytes (little-endian).
 * @param[in] size   Tamanho do registro, em bytes.
 * @retval 1 CRC gravado igual ao calculado sobre os bytes anteriores.
 * @retval 0 CRC diferente, @p record nulo ou @p size menor que 3.
 */
static uint8_t EEPROM_RecordCRCValid(const uint8_t *record, uint8_t size)
{
    uint16_t stored_crc;
    uint16_t calculated_crc;

    if ((record == NULL) || (size < 3U)) return 0U;
    stored_crc = (uint16_t)record[size - 2U] |
                 ((uint16_t)record[size - 1U] << 8);
    calculated_crc = EEPROM_CalculateCRC16(record, (uint16_t)(size - 2U));
    return (stored_crc == calculated_crc) ? 1U : 0U;
}

/** @brief Lê um inteiro de 16 bits sem sinal em little-endian. */
static uint16_t EEPROM_ReadLE16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/* Cache de leitura da exportacao: 1 transacao I2C de ate 128 B no lugar de uma
 * leitura por byte. Usado somente sob o mutex da EEPROM, por uma tarefa de cada vez. */
#define EXPORT_CACHE_SIZE 128U
static uint8_t  export_cache[EXPORT_CACHE_SIZE];
static uint32_t export_cache_base;
static uint32_t export_cache_len;

/**
 * @brief Lê bytes de um chip usando um cache de 128 B.
 *
 * Faz uma única transação I²C de até 128 B em vez de uma leitura por byte; recarrega o cache
 * quando o trecho pedido não está nele.
 * @param[in]  chip_address Endereço I²C do chip.
 * @param[in]  address      Endereço inicial da leitura.
 * @param[in]  end_address  Limite superior da área válida (exclusivo).
 * @param[out] out          Destino.
 * @param[in]  len          Quantidade de bytes.
 * @retval HAL_OK    Bytes lidos.
 * @retval HAL_ERROR O trecho ultrapassa @p end_address.
 * @return Status da HAL em caso de falha de leitura.
 */
static HAL_StatusTypeDef EEPROM_ExportRead(uint16_t chip_address, uint32_t address,
                                           uint32_t end_address, uint8_t *out, uint8_t len)
{
    if ((address + len) > end_address) return HAL_ERROR;
    if ((export_cache_len == 0U) || (address < export_cache_base) ||
        ((address + len) > (export_cache_base + export_cache_len)))
    {
        uint32_t chunk = end_address - address;
        HAL_StatusTypeDef st;
        if (chunk > EXPORT_CACHE_SIZE) chunk = EXPORT_CACHE_SIZE;
        export_cache_len = 0U;
        st = EEPROM24LC512_ReadBuffer((uint16_t)address, export_cache, (uint16_t)chunk, chip_address);
        if (st != HAL_OK) return st;
        export_cache_base = address;
        export_cache_len = chunk;
    }
    memcpy(out, &export_cache[address - export_cache_base], len);
    return HAL_OK;
}

/**
 * @brief Exporta em CSV os registros de um chip, de @p start_address até @p end_address.
 *
 * Lê a máscara no endereço atual. Máscara desconhecida (diferente de 0xFF, que é memória em branco
 * e não conta), registro que passa do fim da área ou CRC inválido contam como suspeitos e fazem a
 * varredura avançar 1 byte. Um registro válido é decodificado (data e hora e colunas pela tabela de
 * sensores), enviado como uma linha e a varredura avança o tamanho do registro.
 *
 * @param[in]     chip_address    Endereço I²C do chip.
 * @param[in]     start_address   Primeiro endereço a examinar.
 * @param[in]     end_address     Fim da área gravada (exclusivo).
 * @param[in,out] records_sent    Acumula os registros enviados.
 * @param[in,out] invalid_records Acumula os registros suspeitos ignorados.
 * @retval HAL_OK    Área percorrida.
 * @retval HAL_ERROR Parâmetros inválidos ou linha CSV que não coube no buffer.
 * @return Status da HAL em caso de falha de leitura ou de envio pela USB.
 */
static HAL_StatusTypeDef EEPROM_ExportOneChip(uint16_t chip_address,
                                               uint32_t start_address,
                                               uint32_t end_address,
                                               uint32_t *records_sent,
                                               uint32_t *invalid_records)
{
    HAL_StatusTypeDef status;
    uint8_t mask;
    uint8_t record[EEPROM_MAX_RECORD_SIZE];
    uint8_t record_size;
    uint32_t address = start_address;
    char line[256];
    char timestamp[24];
    uint16_t index;
    int n;
    uint8_t m;
    uint16_t year;
    uint8_t month, day, hour, minute, second;
    uint8_t payload_len;
    int row_len;

    if ((records_sent == NULL) || (invalid_records == NULL) ||
        (start_address > end_address) || (end_address > EEPROM24LC512_CAPACITY))
    {
        return HAL_ERROR;
    }

    uint32_t last_activity_tick = HAL_GetTick();
    export_cache_len = 0U;

    while (address < end_address)
    {
        Watchdog_Service();
        /* Keepalive: se passou 500 ms sem enviar nada (trecho de bytes invalidos),
         * avisa o PC para ele nao dar timeout. Linhas PROGRESS sao ignoradas pelo script. */
        if ((uint32_t)(HAL_GetTick() - last_activity_tick) >= 500U)
        {
            char progress[32];
            (void)snprintf(progress, sizeof(progress), "PROGRESS,%lu\r\n", (unsigned long)address);
            status = EEPROM_USB_SendText(progress);
            if (status != HAL_OK) return status;
            last_activity_tick = HAL_GetTick();
        }

        status = EEPROM_ExportRead(chip_address, address, end_address, &mask, 1U);
        if (status != HAL_OK) return status;

        /* Mascara desconhecida: nao confiamos no tamanho; busca byte a byte. */
        if (EEPROM_RecordSizeFromMask(mask, &record_size) == 0U)
        {
            if (mask != 0xFFU) (*invalid_records)++;
            address++;
            continue;
        }

        if ((address + record_size) > end_address)
        {
            (*invalid_records)++;
            address++;
            continue;
        }

        status = EEPROM_ExportRead(chip_address, address, end_address, record, record_size);
        if (status != HAL_OK) return status;

        if (EEPROM_RecordCRCValid(record, record_size) == 0U)
        {
            (*invalid_records)++;
            address++; /* recuperacao: procurar outro inicio sem confiar na mascara */
            continue;
        }

        /* Decodifica o formato de EEPROM_BuildSensorRecord usando a tabela de sensores. */
        m = record[0];
        payload_len = (uint8_t)(record_size - 1U - 7U - 2U); /* mascara, RTC e CRC fora */
        index = (uint16_t)(1U + payload_len);

        year = EEPROM_ReadLE16(&record[index]); index += 2U;
        month = record[index++]; day = record[index++];
        hour = record[index++]; minute = record[index++]; second = record[index++];
        (void)snprintf(timestamp, sizeof(timestamp), "%04u-%02u-%02u %02u:%02u:%02u",
                       (unsigned int)year, (unsigned int)month, (unsigned int)day,
                       (unsigned int)hour, (unsigned int)minute, (unsigned int)second);

        /* Colunas na ordem da tabela; sensores ausentes geram campos vazios. */
        n = snprintf(line, sizeof(line), "%u,%s,", (unsigned int)m, timestamp);
        if ((n <= 0) || ((size_t)n >= sizeof(line))) return HAL_ERROR;
        row_len = Sensors_CsvFormatRow(m, &record[1], &line[n], sizeof(line) - (size_t)n);
        if (row_len < 0) return HAL_ERROR;
        n += row_len;
        if (((size_t)n + 3U) > sizeof(line)) return HAL_ERROR;
        line[n++] = '\r'; line[n++] = '\n'; line[n] = '\0';
        status = EEPROM_USB_SendText(line);
        if (status != HAL_OK) return status;
        last_activity_tick = HAL_GetTick();

        (*records_sent)++;
        address += record_size;
    }
    return HAL_OK;
}

HAL_StatusTypeDef EEPROM_ExportCSV_USB(void)
{
    HAL_StatusTypeDef status;
    uint32_t records_sent = 0U;
    uint32_t invalid_records = 0U;
    uint32_t eeprom1_end;
    uint32_t eeprom2_end;
    char summary[80];
    static char header[256];

    if (eeprom_metadata_load_ok == 0U)
    {
        (void)EEPROM_USB_SendText("ERRO,METADADOS_INVALIDOS\r\n");
        return HAL_ERROR;
    }

    /* O limite gravado depende do chip ativo; nunca escrevemos na exportacao. */
    if (eeprom_metadata.reserved == EEPROM_ACTIVE_EEPROM1)
    {
        eeprom1_end = eeprom_metadata.next_address;
        eeprom2_end = EEPROM2_DATA_START_ADDRESS;
        if ((eeprom_metadata.full != 0U) ||
            (eeprom1_end < EEPROM_DATA_START_ADDRESS) ||
            (eeprom1_end > EEPROM24LC512_CAPACITY))
        {
            (void)EEPROM_USB_SendText("ERRO,LIMITE_EEPROM_INVALIDO\r\n");
            return HAL_ERROR;
        }
    }
    else if (eeprom_metadata.reserved == EEPROM_ACTIVE_EEPROM2)
    {
        eeprom1_end = EEPROM24LC512_CAPACITY;
        if (eeprom_metadata.full != 0U)
        {
            if (eeprom_metadata.next_address != 0xFFFFU)
            {
                (void)EEPROM_USB_SendText("ERRO,METADADOS_INCONSISTENTES\r\n");
                return HAL_ERROR;
            }
            eeprom2_end = EEPROM24LC512_CAPACITY;
        }
        else
        {
            eeprom2_end = eeprom_metadata.next_address;
            if (eeprom2_end > EEPROM24LC512_CAPACITY)
            {
                (void)EEPROM_USB_SendText("ERRO,LIMITE_EEPROM_INVALIDO\r\n");
                return HAL_ERROR;
            }
        }
    }
    else
    {
        (void)EEPROM_USB_SendText("ERRO,SELETOR_EEPROM_INVALIDO\r\n");
        return HAL_ERROR;
    }

    diagnostic_export_stage = 3U; /* tentativa de enviar EXPORT_BEGIN */
    status = EEPROM_USB_SendText("EXPORT_BEGIN\r\n");
    if (status != HAL_OK) return status;
    diagnostic_export_stage = 4U; /* tentativa de enviar cabecalho CSV */
    if (Sensors_CsvHeader(header, sizeof(header)) < 0) return HAL_ERROR;
    status = EEPROM_USB_SendText(header);
    if (status != HAL_OK) return status;

    diagnostic_export_stage = 5U; /* leitura e envio dos registros */
    if (eeprom1_end > EEPROM_DATA_START_ADDRESS)
    {
        status = EEPROM_ExportOneChip(EEPROM24LC512_ADDRESS_1,
                                       EEPROM_DATA_START_ADDRESS, eeprom1_end,
                                       &records_sent, &invalid_records);
        if (status != HAL_OK)
        {
            static char diagnostic[384];
            /* Diagnostico nao destrutivo: registra a falha da EEPROM e a ultima
             * falha I2C do OLED, incluindo pagina e fase da transferencia. */
            (void)snprintf(diagnostic, sizeof(diagnostic),
                           "ERRO,FALHA_LEITURA_EEPROM1,HAL=%u,LOCK=%u,I2CERR=%lu,I2CSTATE=%lu,OLED=%u,OLEDERR=%lu,OLEDSTATE=%lu,OLEDPAGE=%u,OLEDSTAGE=%u,PRESTATE=%lu,PRELOCK=%lu,HW_BUSY=%lu,SDA=%u,SCL=%u,OLEDLOCKFAIL=%lu,PAGINAS=%lu,RENDER_OVERLAP=%lu,BUFFER_MUT=%lu,BUF_HASH_PRE=%lu,BUF_HASH_POST=%lu\r\n",
                           (unsigned int)status,
                           (unsigned int)i2c2_last_lock_status,
                           (unsigned long)eeprom_read_error_snapshot,
                           (unsigned long)eeprom_read_state_snapshot,
                           (unsigned int)ssd1306_status,
                           (unsigned long)oled_i2c_error_snapshot,
                           (unsigned long)oled_i2c_state_snapshot,
                           (unsigned int)oled_failed_page,
                           (unsigned int)oled_failed_stage,
                           (unsigned long)oled_pre_state_snapshot,
                           (unsigned long)oled_pre_lock_snapshot,
                           (unsigned long)oled_hw_busy_snapshot,
                           (unsigned int)oled_sda_snapshot,
                           (unsigned int)oled_scl_snapshot,
                           (unsigned long)oled_lock_fail_count,
                           (unsigned long)oled_page_switch_count,
                           (unsigned long)oled_render_overlap_count,
                           (unsigned long)oled_buffer_mutation_count,
                           (unsigned long)oled_buffer_checksum_before,
                           (unsigned long)oled_buffer_checksum_after);
            (void)EEPROM_USB_SendText(diagnostic);
            return status;
        }
    }

    if (eeprom2_end > EEPROM2_DATA_START_ADDRESS)
    {
        status = EEPROM_ExportOneChip(EEPROM24LC512_ADDRESS_2,
                                       EEPROM2_DATA_START_ADDRESS, eeprom2_end,
                                       &records_sent, &invalid_records);
        if (status != HAL_OK)
        {
            (void)EEPROM_USB_SendText("ERRO,FALHA_LEITURA_EEPROM2\r\n");
            return status;
        }
    }

    (void)snprintf(summary, sizeof(summary), "EXPORT_END,%lu,%lu\r\n",
                   (unsigned long)records_sent, (unsigned long)invalid_records);
    diagnostic_export_stage = 5U; /* envio do resumo final */
    return EEPROM_USB_SendText(summary);
}
