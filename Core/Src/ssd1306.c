/**
 * @file ssd1306.c
 * @brief Implementação do driver do OLED SSD1306 e das telas do sistema (ver ssd1306.h).
 */
#include "ssd1306.h"
#include "i2c2_bus_lock.h"
#include "sensor_registry.h"
#include <string.h>
#include <stddef.h>
#include <stdio.h>

/**
 * @brief Fonte de 5 x 7 pixels: 5 colunas por glifo, com o bit 0 na linha superior.
 *
 * Índices: 0 a 9 dígitos, 10 a 35 letras de A a Z, 36 `.`, 37 `:`, 38 `%` e 39 `-`.
 */
const uint8_t font5x7[][5] =
{
    /* 0-9 */
    {0x3E, 0x51, 0x49, 0x45, 0x3E},
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1E},

    /* A-Z */
    {0x7E, 0x11, 0x11, 0x11, 0x7E},
    {0x7F, 0x49, 0x49, 0x49, 0x36},
    {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x7F, 0x49, 0x49, 0x49, 0x41},
    {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A},
    {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01},
    {0x7F, 0x08, 0x14, 0x22, 0x41},
    {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    {0x7F, 0x04, 0x08, 0x10, 0x7F},
    {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x3E, 0x41, 0x51, 0x21, 0x5E},
    {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31},
    {0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F},
    {0x3F, 0x40, 0x38, 0x40, 0x3F},
    {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07},
    {0x61, 0x51, 0x49, 0x45, 0x43},
	/* . : % - */
	{0x00, 0x60, 0x60, 0x00, 0x00},
	{0x00, 0x36, 0x36, 0x00, 0x00},
	{0x62, 0x64, 0x08, 0x13, 0x23},
	{0x08, 0x08, 0x08, 0x08, 0x08}
};


/** Instrumentacao passiva para diagnosticar alteracoes concorrentes do buffer.
 * Nao bloqueia nem corrige o comportamento: apenas conta eventos observados. */
static volatile uint8_t oled_render_active = 0U;
/** @brief Checksum FNV-1a de 32 bits do buffer de 1024 bytes, usado para detectar alteração concorrente. */
static uint32_t OLED_BufferChecksum(void)
{
    uint32_t hash = 2166136261UL;
    for (uint16_t i = 0U; i < 1024U; i++)
    {
        hash ^= ssd1306_buffer[i];
        hash *= 16777619UL;
    }
    return hash;
}

HAL_StatusTypeDef SSD1306_Init(void)
{
    uint8_t command;

    /* Display OFF */
    command = 0xAE;
    if (HAL_I2C_Mem_Write(&hi2c2, 0x3D << 1, 0x00,
                          I2C_MEMADD_SIZE_8BIT,
                          &command, 1, 100) != HAL_OK)
        return HAL_ERROR;

    /* Display clock divide ratio / oscillator frequency */
    command = 0x80;
    if (HAL_I2C_Mem_Write(&hi2c2, 0x3D << 1, 0x00,
                          I2C_MEMADD_SIZE_8BIT,
                          &command, 1, 100) != HAL_OK)
        return HAL_ERROR;

    /* Multiplex ratio: 64 linhas */
    command = 0x3F;
    if (HAL_I2C_Mem_Write(&hi2c2, 0x3D << 1, 0x00,
                          I2C_MEMADD_SIZE_8BIT,
                          &command, 1, 100) != HAL_OK)
        return HAL_ERROR;

    /* Display offset = 0 */
    command = 0x00;
    if (HAL_I2C_Mem_Write(&hi2c2, 0x3D << 1, 0x00,
                          I2C_MEMADD_SIZE_8BIT,
                          &command, 1, 100) != HAL_OK)
        return HAL_ERROR;

    /* Start line = 0 */
    command = 0x40;
    if (HAL_I2C_Mem_Write(&hi2c2, 0x3D << 1, 0x00,
                          I2C_MEMADD_SIZE_8BIT,
                          &command, 1, 100) != HAL_OK)
        return HAL_ERROR;

    /* Charge Pump: ENABLE */
    command = 0x8D;
    if (HAL_I2C_Mem_Write(&hi2c2, 0x3D << 1, 0x00,
                          I2C_MEMADD_SIZE_8BIT,
                          &command, 1, 100) != HAL_OK)
        return HAL_ERROR;

    command = 0x14;
    if (HAL_I2C_Mem_Write(&hi2c2, 0x3D << 1, 0x00,
                          I2C_MEMADD_SIZE_8BIT,
                          &command, 1, 100) != HAL_OK)
        return HAL_ERROR;

    /* Segment remap */
    command = 0xA1;
    if (HAL_I2C_Mem_Write(&hi2c2, 0x3D << 1, 0x00,
                          I2C_MEMADD_SIZE_8BIT,
                          &command, 1, 100) != HAL_OK)
        return HAL_ERROR;

    /* COM scan direction */
    command = 0xC8;
    if (HAL_I2C_Mem_Write(&hi2c2, 0x3D << 1, 0x00,
                          I2C_MEMADD_SIZE_8BIT,
                          &command, 1, 100) != HAL_OK)
        return HAL_ERROR;

    /* COM pins hardware configuration */
    command = 0x12;
    if (HAL_I2C_Mem_Write(&hi2c2, 0x3D << 1, 0x00,
                          I2C_MEMADD_SIZE_8BIT,
                          &command, 1, 100) != HAL_OK)
        return HAL_ERROR;

    /* Contrast */
    command = 0x7F;
    if (HAL_I2C_Mem_Write(&hi2c2, 0x3D << 1, 0x00,
                          I2C_MEMADD_SIZE_8BIT,
                          &command, 1, 100) != HAL_OK)
        return HAL_ERROR;

    /* Pre-charge period */
    command = 0xD9;
    if (HAL_I2C_Mem_Write(&hi2c2, 0x3D << 1, 0x00,
                          I2C_MEMADD_SIZE_8BIT,
                          &command, 1, 100) != HAL_OK)
        return HAL_ERROR;

    command = 0xF1;
    if (HAL_I2C_Mem_Write(&hi2c2, 0x3D << 1, 0x00,
                          I2C_MEMADD_SIZE_8BIT,
                          &command, 1, 100) != HAL_OK)
        return HAL_ERROR;

    /* VCOMH deselect level */
    command = 0xDB;
    if (HAL_I2C_Mem_Write(&hi2c2, 0x3D << 1, 0x00,
                          I2C_MEMADD_SIZE_8BIT,
                          &command, 1, 100) != HAL_OK)
        return HAL_ERROR;

    command = 0x40;
    if (HAL_I2C_Mem_Write(&hi2c2, 0x3D << 1, 0x00,
                          I2C_MEMADD_SIZE_8BIT,
                          &command, 1, 100) != HAL_OK)
        return HAL_ERROR;

    /* Entire display follows RAM */
    command = 0xA4;
    if (HAL_I2C_Mem_Write(&hi2c2, 0x3D << 1, 0x00,
                          I2C_MEMADD_SIZE_8BIT,
                          &command, 1, 100) != HAL_OK)
        return HAL_ERROR;

    /* Normal display */
    command = 0xA6;
    if (HAL_I2C_Mem_Write(&hi2c2, 0x3D << 1, 0x00,
                          I2C_MEMADD_SIZE_8BIT,
                          &command, 1, 100) != HAL_OK)
        return HAL_ERROR;

    /* Display ON */
    command = 0xAF;
    if (HAL_I2C_Mem_Write(&hi2c2, 0x3D << 1, 0x00,
                          I2C_MEMADD_SIZE_8BIT,
                          &command, 1, 100) != HAL_OK)
        return HAL_ERROR;

    App_Delay(100);

    return HAL_OK;
}


/* Sequencias do datasheet do SSD1306: desligar = display OFF e bomba de carga desabilitada;
 * ligar = bomba de carga habilitada e display ON. O conteudo da RAM do display e mantido. */
HAL_StatusTypeDef SSD1306_SetPower(bool on)
{
    static const uint8_t sequence_on[3] = { 0x8D, 0x14, 0xAF };
    static const uint8_t sequence_off[3] = { 0xAE, 0x8D, 0x10 };
    const uint8_t *sequence = on ? sequence_on : sequence_off;
    HAL_StatusTypeDef status = I2C2_BusAcquire();

    if (status != HAL_OK) return status;
    for (uint8_t i = 0U; i < 3U; i++)
    {
        uint8_t command = sequence[i];
        status = HAL_I2C_Mem_Write(&hi2c2, 0x3D << 1, 0x00, I2C_MEMADD_SIZE_8BIT, &command, 1, 100);
        if (status != HAL_OK) break;
    }
    I2C2_BusRelease();
    return status;
}

HAL_StatusTypeDef SSD1306_TestConnection(void)
{
    HAL_StatusTypeDef status = I2C2_BusAcquire();
    if (status != HAL_OK) return status;

    status = HAL_I2C_IsDeviceReady(&hi2c2, 0x3D << 1, 3, 100);
    I2C2_BusRelease();
    return status;
}


void I2C2_Scan_All(void)
{
    uint8_t address;
    if (I2C2_BusAcquire() != HAL_OK) return;

    for (address = 1; address < 128; address++)
    {
        if (HAL_I2C_IsDeviceReady(&hi2c2, address << 1, 3, 100) == HAL_OK)
        {
            /* Coloque breakpoint aqui */
            i2c2_device_found = address;
        }
    }
    I2C2_BusRelease();
}


HAL_StatusTypeDef SSD1306_Fill(void)
{
    uint8_t buffer[129];
    HAL_StatusTypeDef status = I2C2_BusAcquire();
    if (status != HAL_OK) return status;

    /* Primeiro byte indica que os próximos dados são pixels. */
    buffer[0] = 0x40;
    for (int i = 1; i < 129; i++) buffer[i] = 0xFF;

    /* Mantem o mutex em toda a operacao, incluindo comandos e pixels. */
    for (uint8_t page = 0; page < 8; page++)
    {
        uint8_t commands[3] = { (uint8_t)(0xB0 | page), 0x00, 0x10 };
        status = HAL_I2C_Mem_Write(&hi2c2, 0x3D << 1, 0x00,
                                   I2C_MEMADD_SIZE_8BIT, commands, 3, 100);
        if (status != HAL_OK) break;

        status = HAL_I2C_Master_Transmit(&hi2c2, 0x3D << 1, buffer, 129, 100);
        if (status != HAL_OK) break;
    }

    I2C2_BusRelease();
    return status;
}

void SSD1306_Clear(void)
{
    for (uint16_t i = 0; i < 1024; i++)
    {
        ssd1306_buffer[i] = 0x00;
    }
}


void SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= 128 || y >= 64)
        return;

    if (color)
    {
        ssd1306_buffer[x + (y / 8) * 128] |= (1 << (y % 8));
    }
    else
    {
        ssd1306_buffer[x + (y / 8) * 128] &= ~(1 << (y % 8));
    }
}


void SSD1306_DrawChar(uint8_t x, uint8_t y, char c)
{
    uint8_t index;

    if (c >= '0' && c <= '9')
    {
        index = c - '0';
    }
    else if (c >= 'A' && c <= 'Z')
    {
        index = 10 + (c - 'A');
    }
    else if (c == '.')
    {
        index = 36;
    }
    else if (c == ':')
    {
        index = 37;
    }
    else if (c == '%')
    {
        index = 38;
    }
    else if (c == '-')
    {
        index = 39;
    }
    else
    {
        return;
    }

    for (uint8_t col = 0; col < 5; col++)
    {
        uint8_t line = font5x7[index][col];

        for (uint8_t row = 0; row < 7; row++)
        {
            if (line & (1 << row))
            {
                SSD1306_DrawPixel(x + col, y + row, 1);
            }
        }
    }
}


void SSD1306_DrawString(uint8_t x, uint8_t y, const char *str)
{
    while (*str)
    {
        if (*str == ' ')
        {
            x += 6;
        }
        else
        {
            SSD1306_DrawChar(x, y, *str);
            x += 6;
        }

        str++;
    }
}


/** Captura o estado imediatamente antes de uma chamada bloqueante da HAL.
 * I2C2 usa PB10=SCL e PB3=SDA neste projeto. Ler IDR não altera a configuração AF. */
static void OLED_CaptureI2C2PreState(void)
{
    oled_pre_state_snapshot = (uint32_t)hi2c2.State;
    oled_pre_lock_snapshot = (uint32_t)hi2c2.Lock;
    oled_hw_busy_snapshot = (__HAL_I2C_GET_FLAG(&hi2c2, I2C_FLAG_BUSY) != RESET) ? 1U : 0U;
    oled_sda_snapshot = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3) == GPIO_PIN_SET) ? 1U : 0U;
    oled_scl_snapshot = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10) == GPIO_PIN_SET) ? 1U : 0U;
}

void SSD1306_UpdateScreen(void)
{
    uint8_t data[129];
    HAL_StatusTypeDef status;
    uint8_t failed_page = 0xFFU;
    uint8_t failed_stage = 0U; /* 1=comando de pagina; 2=transferencia dos pixels */

    data[0] = 0x40;
    oled_task_stage = 4U; /* esperando mutex do I2C2 */
    status = I2C2_BusAcquire();
    if (status != HAL_OK)
    {
        oled_lock_fail_count++;
        oled_update_fail_count++;
        oled_consecutive_fail++;
        oled_failed_page = 0xFFU;
        oled_failed_stage = 3U; /* falha ao adquirir mutex */
        ssd1306_status = status;
        return;
    }
    oled_task_stage = 5U; /* transferindo pelo I2C2 */

    for (uint8_t page = 0; page < 8; page++)
    {
        uint8_t commands[3];
        commands[0] = 0xB0 | page;
        commands[1] = 0x00;
        commands[2] = 0x10;

        OLED_CaptureI2C2PreState();
        status = HAL_I2C_Mem_Write(
            &hi2c2, 0x3D << 1, 0x00, I2C_MEMADD_SIZE_8BIT, commands, 3, 100);
        if (status != HAL_OK)
        {
            failed_page = page;
            failed_stage = 1U;
            /* Captura antes de liberar o mutex e antes de outra tarefa poder
             * iniciar uma transacao que altere o estado da HAL. */
            oled_i2c_error_snapshot = hi2c2.ErrorCode;
            oled_i2c_state_snapshot = (uint32_t)hi2c2.State;
            break;
        }

        for (uint8_t x = 0; x < 128; x++)
        {
            data[x + 1] = ssd1306_buffer[x + page * 128];
        }

        oled_buffer_checksum_before = OLED_BufferChecksum();
        OLED_CaptureI2C2PreState();
        status = HAL_I2C_Master_Transmit(&hi2c2, 0x3D << 1, data, 129, 100);
        oled_buffer_checksum_after = OLED_BufferChecksum();
        if (oled_buffer_checksum_before != oled_buffer_checksum_after)
            oled_buffer_mutation_count++;
        if (status != HAL_OK)
        {
            failed_page = page;
            failed_stage = 2U;
            oled_i2c_error_snapshot = hi2c2.ErrorCode;
            oled_i2c_state_snapshot = (uint32_t)hi2c2.State;
            break;
        }
    }

    I2C2_BusRelease();

    if (status == HAL_OK)
    {
        /* Limpa o evento anterior somente apos uma atualizacao completa. */
        oled_i2c_error_snapshot = 0U;
        oled_i2c_state_snapshot = (uint32_t)hi2c2.State;
        oled_failed_page = 0xFFU;
        oled_failed_stage = 0U;
    }
    else
    {
        oled_failed_page = failed_page;
        oled_failed_stage = failed_stage;
    }

    /* Mantem o resultado da ultima atualizacao para diagnostico. */
    ssd1306_status = status;
    if (status == HAL_OK) { oled_update_ok_count++; oled_consecutive_fail = 0U; }
    else
    {
        if (oled_consecutive_fail == 0U)
        {
            oled_ff_status = (uint32_t)status;
            oled_ff_i2cerr = hi2c2.ErrorCode;
            oled_ff_i2cstate = (uint32_t)hi2c2.State;
            oled_ff_sensor_active = sensor_cycle_active;
        }
        oled_update_fail_count++; oled_consecutive_fail++;
    }
}


void SSD1306_DrawUInt(uint8_t x, uint8_t y, uint32_t number)
{
    char buffer[11];

    snprintf(buffer, sizeof(buffer), "%lu", number);

    SSD1306_DrawString(x, y, buffer);
}


void SSD1306_DrawFixed1(uint8_t x, uint8_t y, int32_t value)
{
    char buffer[16];
    int32_t integer_part;
    int32_t decimal_part;

    if (value < 0)
    {
        SSD1306_DrawChar(x, y, '-');
        x += 6;
        value = -value;
    }

    integer_part = value / 10;
    decimal_part = value % 10;

    snprintf(buffer, sizeof(buffer), "%ld.%ld",
             integer_part, decimal_part);

    SSD1306_DrawString(x, y, buffer);
}


void UpdateDisplaySnapshot(const DisplaySnapshot *snapshot)
{
    if ((snapshot == NULL) || (snapshot->oled_available == 0U))
        return;

    if (oled_render_active != 0U)
        oled_render_overlap_count++;
    oled_render_active = 1U;

    /* Sensores modulares: o esperado e o que ja foi detectado desde o boot; modulo nao
     * conectado nao e erro. Erro = sensor detectado que parou de responder. */
    uint8_t expected = Sensors_PopCount(snapshot->sensor_detected_mask);
    uint8_t healthy = Sensors_PopCount((uint8_t)(snapshot->sensor_present_mask & snapshot->sensor_detected_mask));
    const char *missing_sensor = Sensors_FirstMissingName(snapshot->sensor_detected_mask, snapshot->sensor_present_mask);
    char sensor_error_text[20];
    const char *overall = "OK";
    const char *rtc_text = (snapshot->rtc_time_valid != 0U) ? "RTC:VALIDO" : "RTC:INVALIDO";
    const char *usb_text = "USB:LIVRE";
    const char *error_text = "ERRO:NENHUM";

    if (snapshot->eeprom_status_display == 1U || snapshot->eeprom_metadata_load_ok == 0U)
        error_text = "ERRO:EEPROM";
    else if (expected == 0U)
        error_text = "ERRO:SEM SENS";
    else if (missing_sensor != NULL)
    {
        (void)snprintf(sensor_error_text, sizeof(sensor_error_text), "ERRO:%s", missing_sensor);
        error_text = sensor_error_text;
    }
    else if (snapshot->ssd1306_status != HAL_OK)
        error_text = "ERRO:OLED";

    if (healthy < expected || expected == 0U || snapshot->rtc_time_valid == 0U || snapshot->eeprom_status_display != 0U)
        overall = (snapshot->eeprom_status_display == 1U || snapshot->eeprom_metadata_load_ok == 0U) ? "ERRO" : "AVISO";

    switch (snapshot->usb_operation_state)
    {
        case 1U: usb_text = "USB:EXPORTANDO"; break;
        case 2U: usb_text = "USB:LIMPANDO"; break;
        case 3U: usb_text = "USB:ERRO"; break;
        default: usb_text = "USB:LIVRE"; break;
    }

    SSD1306_Clear();
    if (snapshot->display_page == 0U)
    {
        SSD1306_DrawString(0, 0, "ESTADO DO SISTEMA");
        SSD1306_DrawString(0, 12, "GERAL:");
        SSD1306_DrawString(42, 12, overall);
        SSD1306_DrawString(0, 24, "SENSORES:");
        SSD1306_DrawUInt(60, 24, healthy);
        SSD1306_DrawString(69, 24, "/");
        SSD1306_DrawUInt(75, 24, expected);
        SSD1306_DrawString(0, 36, rtc_text);
        SSD1306_DrawString(0, 50, error_text);
    }
    else
    {
        SSD1306_DrawString(0, 0, "ARMAZENAMENTO");
        SSD1306_DrawString(0, 12, "EEPROM ATIVA:");
        if (snapshot->eeprom_metadata_load_ok == 0U)
            SSD1306_DrawString(78, 12, "ERRO");
        else if (snapshot->eeprom_full != 0U)
            SSD1306_DrawString(78, 12, "CHEIA");
        else if (snapshot->eeprom_active == EEPROM_ACTIVE_EEPROM2)
            SSD1306_DrawString(78, 12, "EEPROM2");
        else
            SSD1306_DrawString(78, 12, "EEPROM1");

        SSD1306_DrawString(0, 24, "PROX. END:");
        if (snapshot->eeprom_metadata_load_ok != 0U)
            SSD1306_DrawUInt(60, 24, snapshot->eeprom_next_address);
        else
            SSD1306_DrawString(60, 24, "N/D");

        SSD1306_DrawString(0, 38, usb_text);
        /* V6: linha de diagnostico independente da USB. Valores sao estagios. */
        SSD1306_DrawString(0, 52, "S:"); SSD1306_DrawUInt(12, 52, snapshot->diagnostic_sensor_stage);
        SSD1306_DrawString(30, 52, "E:"); SSD1306_DrawUInt(42, 52, snapshot->diagnostic_export_stage);
        SSD1306_DrawString(60, 52, "T:"); SSD1306_DrawUInt(72, 52, (snapshot->diagnostic_usb_tx_result < 0) ? 9U : (uint8_t)snapshot->diagnostic_usb_tx_result);
        SSD1306_DrawString(90, 52, "R:"); SSD1306_DrawUInt(102, 52, (snapshot->diagnostic_export_result < 0) ? 9U : (uint8_t)snapshot->diagnostic_export_result);
    }
    SSD1306_UpdateScreen();
    oled_render_active = 0U;
}

void UpdateDisplay(void)
{
    DisplaySnapshot snapshot = {0};
    snapshot.oled_available = oled_available;
    snapshot.display_page = display_page;
    snapshot.sensor_present_mask = Sensors_AvailableMask();
    snapshot.sensor_detected_mask = Sensors_DetectedMask();
    snapshot.rtc_time_valid = rtc_time_valid;
    snapshot.eeprom_status_display = eeprom_status_display;
    snapshot.eeprom_metadata_load_ok = eeprom_metadata_load_ok;
    snapshot.eeprom_full = eeprom_full;
    snapshot.eeprom_active = eeprom_metadata.reserved;
    snapshot.eeprom_next_address = eeprom_next_address;
    snapshot.usb_operation_state = usb_operation_state;
    snapshot.ssd1306_status = ssd1306_status;
    UpdateDisplaySnapshot(&snapshot);
}

void SSD1306_DrawEEPROMStatus(uint8_t x, uint8_t y)
{
    SSD1306_DrawString(x, y, "EEPROM:");

    if (eeprom_status_display == 2)
    {
        SSD1306_DrawString(x + 48, y, "FULL");
    }
    else if (eeprom_status_display == 1)
    {
        SSD1306_DrawString(x + 48, y, "ERR");
    }
    else
    {
        SSD1306_DrawString(x + 48, y, "OK");
    }
}

