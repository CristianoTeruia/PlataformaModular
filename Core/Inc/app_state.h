/**
 * @file app_state.h
 * @brief Tipos, constantes e variáveis globais compartilhadas entre os módulos do firmware.
 *
 * Reúne o mapa da EEPROM, os bits de identificação dos sensores, o intervalo de aquisição e as
 * variáveis de estado e de diagnóstico lidas por mais de um módulo (definidas em app_state.c,
 * app_tasks.c e freertos.c). Os campos `volatile` são escritos por uma tarefa e lidos por outra,
 * por exemplo pelo comando USB `DIAG`.
 */
#ifndef APP_STATE_H
#define APP_STATE_H

#include "main.h"
#include <stdbool.h>

/**
 * @brief Registro de tamanho fixo do formato original (legado).
 *
 * Foi substituído pelo registro de tamanho variável montado por EEPROM_BuildSensorRecord() e
 * permanece apenas nas rotinas de teste manual. Os campos usam inteiros escalados.
 */
typedef struct __attribute__((packed))
{
    int16_t temperature_aht;  /**< Temperatura do AHT20, em centésimos de °C. */
    uint16_t humidity_aht;  /**< Umidade do AHT20, em centésimos de %. */
    int16_t temperature_bmp;  /**< Temperatura do BMP280, em centésimos de °C. */
    uint32_t pressure_bmp;  /**< Pressão do BMP280, em Pa (inteiro). */
    int16_t accel_x;  /**< Aceleração em X, em milésimos de g. */
    int16_t accel_y;  /**< Aceleração em Y, em milésimos de g. */
    int16_t accel_z;  /**< Aceleração em Z, em milésimos de g. */
    int16_t gyro_x;  /**< Velocidade angular em X, em centésimos de °/s. */
    int16_t gyro_y;  /**< Velocidade angular em Y, em centésimos de °/s. */
    int16_t gyro_z;  /**< Velocidade angular em Z, em centésimos de °/s. */
} SensorRecordEEPROM;

/**
 * @brief Metadados da EEPROM, gravados a partir do endereço 0x0000 da EEPROM 1.
 *
 * Indicam onde gravar o próximo registro. Só são considerados válidos se `magic` for igual a
 * #EEPROM_METADATA_MAGIC; metadados inválidos nunca são reinicializados sozinhos, para não
 * perder registros.
 */
typedef struct __attribute__((packed))
{
    uint16_t next_address;  /**< Próximo endereço livre no chip ativo; 0xFFFF quando a memória está cheia. */
    uint8_t full;  /**< 1 = as duas EEPROMs estão cheias. */
    uint8_t reserved;  /**< Seletor do chip ativo (#EEPROM_ACTIVE_EEPROM1 ou #EEPROM_ACTIVE_EEPROM2); nome histórico do campo. */
    uint16_t magic;  /**< Deve valer #EEPROM_METADATA_MAGIC. */
} EEPROMMetadata;

#define EEPROM24LC512_ADDRESS_1   (0x50U << 1)  /**< Endereço I²C da EEPROM 1 (0x50 de 7 bits, já deslocado para a HAL). */
#define EEPROM24LC512_ADDRESS_2   (0x51U << 1)  /**< Endereço I²C da EEPROM 2 (0x51 de 7 bits, já deslocado para a HAL). */
#define EEPROM24LC512_ADDRESS     EEPROM24LC512_ADDRESS_1  /**< Endereço padrão das funções de byte/registro simples (EEPROM 1). */
#define EEPROM24LC512_PAGE_SIZE   128U  /**< Tamanho da página de escrita da 24LC512, em bytes. */
#define EEPROM24LC512_WRITE_DELAY 10U  /**< Espera após cada escrita (ciclo interno de gravação), em ms. */
#define EEPROM24LC512_CAPACITY    65536UL  /**< Capacidade de cada chip, em bytes (64 KiB). */
#define EEPROM_ACTIVE_EEPROM1     0U  /**< Valor do seletor de chip ativo: EEPROM 1. */
#define EEPROM_ACTIVE_EEPROM2     1U  /**< Valor do seletor de chip ativo: EEPROM 2. */
#define EEPROM2_DATA_START_ADDRESS 0x0000U  /**< Primeiro endereço de dados na EEPROM 2. */
#define EEPROM_METADATA_ADDRESS   0x0000U  /**< Endereço dos metadados (na EEPROM 1). */
#define EEPROM_DATA_START_ADDRESS 0x0100U  /**< Primeiro endereço de dados na EEPROM 1 (0x0000 a 0x00FF reservados aos metadados). */
#define EEPROM_METADATA_MAGIC     0xA55AU  /**< Valor que identifica metadados válidos. */
#include "app_config.h"

#define SENSOR_AHT20   (1U << 0)  /**< Bit do AHT20 na máscara do registro. */
#define SENSOR_BMP280  (1U << 1)  /**< Bit do BMP280 na máscara do registro. */
#define SENSOR_MPU6500 (1U << 2)  /**< Bit do MPU6500 na máscara do registro. */
#define SENSOR_BH1750  (1U << 3)  /**< Bit do BH1750 na máscara do registro. */
#define SENSOR_ENS160  (1U << 4)  /**< Bit do ENS160 na máscara do registro. */
/* Maior registro: 1 (mascara) + 4 + 4 + 12 + 2 + 6 (sensores) + 7 (RTC) + 2 (CRC) = 38 bytes. */
#define EEPROM_MAX_RECORD_SIZE 40U  /**< Tamanho máximo de um registro, em bytes (o maior possível, com todos os sensores, tem 38 B). */

/* Intervalo central da aquisicao; valor inicial de projeto, ajustavel. */
#define SENSOR_ACQUISITION_INTERVAL_MS 10000U  /**< Intervalo entre aquisições, em ms (valor inicial de projeto, ajustável). */

extern I2C_HandleTypeDef hi2c1;  /**< I²C1: barramento dos sensores. */
extern I2C_HandleTypeDef hi2c2;  /**< I²C2: EEPROMs e display OLED. */
extern RTC_HandleTypeDef hrtc;  /**< RTC interno, mantido pelo VBAT (CR2032). */
extern uint8_t rtc_time_valid;  /**< 1 = hora do RTC válida; 0 = o RTC perdeu a hora (ficou em 01/01/2000). */

extern float aht20_temperature, aht20_humidity;  /**< Última leitura do AHT20: temperatura (°C) e umidade (%). */
extern float bmp280_temperature, bmp280_pressure;  /**< Última leitura do BMP280: temperatura (°C) e pressão (Pa). */
extern uint16_t dig_T1, dig_P1;  /**< Coeficientes de calibração do BMP280 (sem sinal). */
extern int16_t dig_T2, dig_T3, dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;  /**< Coeficientes de calibração do BMP280 (com sinal). */
extern int32_t bmp280_t_fine;  /**< Temperatura intermediária (`t_fine`) da compensação do BMP280, reutilizada no cálculo da pressão. */
extern uint8_t mpu6500_who_am_i;  /**< Último valor lido do registrador WHO_AM_I do MPU6500 (esperado 0x70). */
extern int16_t mpu6500_accel_x, mpu6500_accel_y, mpu6500_accel_z;  /**< Aceleração bruta, em contagens (fundo de escala ±2 g, 16384 LSB/g). */
extern int16_t mpu6500_gyro_x, mpu6500_gyro_y, mpu6500_gyro_z;  /**< Velocidade angular bruta, em contagens (fundo de escala ±250 °/s, 131 LSB/(°/s)). */
extern float mpu6500_accel_x_g, mpu6500_accel_y_g, mpu6500_accel_z_g;  /**< Aceleração em g, já com o offset removido. */
extern float mpu6500_gyro_x_dps, mpu6500_gyro_y_dps, mpu6500_gyro_z_dps;  /**< Velocidade angular em °/s, já com o offset removido. */
extern float mpu6500_accel_offset_x, mpu6500_accel_offset_y, mpu6500_accel_offset_z;  /**< Offsets da aceleração (g) calculados por MPU6500_Calibrate(). */
extern float mpu6500_gyro_offset_x, mpu6500_gyro_offset_y, mpu6500_gyro_offset_z;  /**< Offsets do giroscópio (°/s) calculados por MPU6500_Calibrate(). */
extern uint8_t ssd1306_buffer[1024];  /**< Memória de imagem do OLED: 128 x 64 pixels, 8 páginas de 128 bytes. */
extern HAL_StatusTypeDef ssd1306_status, ssd1306_fill_status;  /**< Status HAL da última atualização e do último preenchimento do OLED. */
extern volatile uint8_t i2c2_last_lock_status;  /**< Resultado da última aquisição do mutex do I²C2 em leitura da EEPROM (diagnóstico). */
extern volatile uint32_t eeprom_read_error_snapshot, eeprom_read_state_snapshot;  /**< `ErrorCode` e `State` do I²C2 logo após a última leitura da EEPROM. */
extern volatile uint32_t oled_lock_fail_count, oled_page_switch_count;  /**< Falhas do OLED ao obter o mutex do I²C2 e trocas de tela realizadas. */
extern volatile uint32_t oled_i2c_error_snapshot, oled_i2c_state_snapshot;  /**< `ErrorCode` e `State` do I²C2 na última falha do OLED. */
extern volatile uint32_t oled_pre_state_snapshot, oled_pre_lock_snapshot;  /**< `State` e `Lock` do I²C2 antes da última transferência do OLED. */
extern volatile uint32_t oled_hw_busy_snapshot;  /**< Flag BUSY do periférico I²C2 antes da última transferência do OLED. */
extern volatile uint8_t oled_sda_snapshot, oled_scl_snapshot;  /**< Nível lógico das linhas SDA e SCL do I²C2 antes da última transferência do OLED. */
extern volatile uint8_t oled_failed_page, oled_failed_stage;  /**< Página (0 a 7; 0xFF = nenhuma) e fase da última falha: 1 comando de página, 2 pixels, 3 mutex. */
extern volatile uint32_t oled_render_overlap_count, oled_buffer_mutation_count;  /**< Renderizações sobrepostas e alterações concorrentes do buffer detectadas (instrumentação passiva). */
extern volatile uint32_t oled_buffer_checksum_before, oled_buffer_checksum_after;  /**< Checksum (FNV-1a) do buffer do OLED antes e depois da transferência. */
extern uint8_t i2c2_device_found;  /**< Último endereço de 7 bits que respondeu em I2C2_Scan_All(). */
extern int32_t display_aht_temp, display_aht_hum, display_pressure, display_accel_z;  /**< Valores escalados para o OLED (legado): temperatura e umidade em décimos, pressão em Pa/10 e aceleração Z bruta. */
extern uint8_t display_page;  /**< Tela atual do OLED: 0 = estado do sistema; 1 = armazenamento. */
extern uint8_t eeprom_write_data, eeprom_read_data;  /**< Variáveis dos testes manuais da EEPROM (legado). */
extern HAL_StatusTypeDef eeprom_write_status, eeprom_read_status, eeprom_test_status;  /**< Status HAL dos testes manuais da EEPROM (legado). */
extern uint8_t eeprom_test_write[5], eeprom_test_read[5];  /**< Buffers dos testes manuais da EEPROM (legado). */
extern SensorRecordEEPROM eeprom_record, eeprom_record_read;  /**< Registros do formato antigo de tamanho fixo (legado). */
extern uint16_t eeprom_record_size;  /**< Tamanho do último registro serializado nos testes (legado). */
extern uint8_t eeprom_records_equal;  /**< 1 = registro lido igual ao gravado no teste (legado). */
extern SensorRecordEEPROM eeprom_records_write[5], eeprom_records_read[5];  /**< Vetores de registros dos testes manuais (legado). */
extern uint8_t eeprom_multi_test_ok;  /**< Resultado do teste de múltiplos registros (legado). */
extern uint16_t eeprom_next_address;  /**< Próximo endereço livre no chip ativo; 0xFFFF = memória cheia. */
extern uint8_t eeprom_full, eeprom_status_display, eeprom_metadata_load_ok;  /**< `eeprom_full`: 1 = as duas EEPROMs cheias. `eeprom_status_display`: 0 OK, 1 erro, 2 cheia. `eeprom_metadata_load_ok`: 1 = metadados válidos. */
extern volatile uint8_t usb_export_requested;  /**< Pedido `EXPORT` recebido pela USB, atendido pela `defaultTask`. */
extern volatile uint8_t usb_clear_requested;  /**< Pedido `CLEAR,CONFIRMO` recebido pela USB, atendido pela `defaultTask`. */
extern volatile uint8_t usb_rtc_set_requested;  /**< Pedido `RTC_SET` recebido pela USB, atendido pela `defaultTask`. */
/* V8: diagnostico do OLED/I2C2 (consultado pelo comando USB DIAG). */
extern volatile uint8_t usb_diag_requested;  /**< Pedido `DIAG` recebido pela USB, atendido pela `defaultTask`. */
extern volatile uint8_t oled_task_stage;  /**< Etapa da `oledTask`: 0 ocioso, 1 espera o mutex de sensores, 2 espera o mutex da EEPROM, 3 renderizando, 4 espera o mutex do I²C2, 5 transferindo pelo I²C2, 6 recuperando. */
extern volatile uint32_t oled_task_loops, oled_update_ok_count, oled_update_fail_count;  /**< Voltas da `oledTask` e atualizações do OLED bem-sucedidas e com falha. */
extern volatile uint32_t oled_consecutive_fail, i2c2_recovery_count, i2c2_recovery_status;  /**< Falhas seguidas do OLED, recuperações do I²C2 realizadas e status HAL da última. */
/* V9: captura da PRIMEIRA falha de cada episodio do OLED e atividade da task de sensores. */
extern volatile uint32_t oled_ff_status, oled_ff_i2cerr, oled_ff_i2cstate;  /**< Primeira falha de cada episódio do OLED: status HAL, `ErrorCode` e `State` do I²C2. */
extern volatile uint8_t oled_ff_sensor_active, sensor_cycle_active;  /**< `sensor_cycle_active`: 1 enquanto a `sensorTask` está em um ciclo de aquisição; `oled_ff_sensor_active`: valor dele na primeira falha. */
/* V10: falhas fatais (definidas em freertos.c), usadas pelo HardFault_Handler. */
extern volatile uint32_t diag_fault_cfsr, diag_fault_hfsr, diag_fault_bfar, diag_fault_mmfar;  /**< Registradores de falha (CFSR, HFSR, BFAR, MMFAR) capturados no HardFault (definidos em freertos.c). */
/**
 * @brief Para o sistema por falha fatal, piscando o LED (PC13) em um padrão de diagnóstico.
 *
 * Padrões: 2 piscadas = estouro de pilha de uma tarefa; 3 = falha de alocação no heap do
 * FreeRTOS; 4 = HardFault. Grava `0xFA00 | blinks` em `BKP DR2` (lido no boot) e, como ninguém
 * alimenta o IWDG, a placa reinicia sozinha de 11 s a 31 s depois. Não retorna.
 * @param[in] blinks Número de piscadas do padrão.
 */
void Diag_FaultBlink(uint8_t blinks);
/* V9: espera que cede a CPU quando o RTOS roda (HAL_Delay e espera ocupada). */
/**
 * @brief Espera @p ms milissegundos cedendo a CPU quando o RTOS está em execução.
 *
 * Usa `osDelay` com o kernel em execução e `HAL_Delay` antes dele. Evita que uma espera ocupada
 * em tarefa de prioridade maior deixe as demais sem executar.
 * @param[in] ms Tempo de espera, em milissegundos.
 */
void App_Delay(uint32_t ms);
/* V9: protege o acesso ao RTC interno (leitura e ajuste) entre tasks. */
/**
 * @brief Obtém o mutex que protege o acesso ao RTC interno (leitura e ajuste) entre tarefas.
 *
 * É um mutex "folha": nunca é mantido enquanto se espera por outro, portanto não cria deadlock.
 * Não faz nada antes do início do RTOS.
 */
void App_RTC_Lock(void);
/** @brief Libera o mutex obtido por App_RTC_Lock(). */
void App_RTC_Unlock(void);
extern char usb_rtc_set_command[32];  /**< Linha `RTC_SET` recebida, copiada da callback USB para ser tratada na `defaultTask`. */
extern EEPROMMetadata eeprom_metadata;  /**< Cópia em RAM dos metadados gravados na EEPROM 1. */
extern bool aht20_available, bmp280_available, mpu6500_available;  /**< 1 se o sensor respondeu na última tentativa de leitura. */
extern uint8_t aht20_last_read_ok, bmp280_last_read_ok, mpu6500_last_read_ok;  /**< 1 se a última leitura do sensor foi bem-sucedida. */
extern uint8_t usb_operation_state;  /**< Operação USB em curso: 0 livre, 1 exportando, 2 limpando, 3 erro. */
/* Diagnostico V6: ultimo estagio atingido; valores persistem ate reinicio. */
extern volatile uint8_t diagnostic_sensor_stage;  /**< Último estágio do ciclo de aquisição: de 1 em diante, leitura do sensor de índice (valor - 1) da tabela; 4 = avaliação e gravação na EEPROM; 5 = ciclo concluído (4 e 5 coincidem com os índices do BH1750 e do ENS160). */
extern volatile uint8_t diagnostic_export_stage;  /**< Estágio da exportação: 1 entrada, 2 validação, 3 `EXPORT_BEGIN`, 4 cabeçalho, 5 registros e resumo, 6 rotina retornou. */
extern volatile int8_t diagnostic_export_result;  /**< Resultado da exportação: -1 não terminou, 0 `HAL_OK`, 1 erro, 2 tempo esgotado. */
extern volatile int8_t diagnostic_usb_tx_result;  /**< Resultado do último envio USB: -1 sem envio, 0 OK, 1 `USBD_BUSY`, 2 falha ou outro. */
extern uint8_t oled_available;  /**< 1 se o SSD1306 respondeu na inicialização. */
extern uint8_t eeprom_full_read_buffer[64];  /**< Último bloco de 64 bytes lido por EEPROM24C512_ReadFullMemory() (depuração). */
extern uint16_t eeprom_full_read_address;  /**< Endereço inicial do último bloco lido por EEPROM24C512_ReadFullMemory(). */
extern HAL_StatusTypeDef eeprom_full_read_status;  /**< Status HAL da última leitura de EEPROM24C512_ReadFullMemory(). */

#endif
