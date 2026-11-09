/**
 * @file app_state.c
 * @brief Definição das variáveis globais declaradas em app_state.h.
 */
#include "app_state.h"
#include "cmsis_os2.h"

float aht20_temperature, aht20_humidity;
float bmp280_temperature, bmp280_pressure;
uint16_t dig_T1, dig_P1;
int16_t dig_T2, dig_T3, dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
int32_t bmp280_t_fine;
uint8_t mpu6500_who_am_i;
int16_t mpu6500_accel_x, mpu6500_accel_y, mpu6500_accel_z;
int16_t mpu6500_gyro_x, mpu6500_gyro_y, mpu6500_gyro_z;
float mpu6500_accel_x_g, mpu6500_accel_y_g, mpu6500_accel_z_g;
float mpu6500_gyro_x_dps, mpu6500_gyro_y_dps, mpu6500_gyro_z_dps;
float mpu6500_accel_offset_x, mpu6500_accel_offset_y, mpu6500_accel_offset_z;
float mpu6500_gyro_offset_x, mpu6500_gyro_offset_y, mpu6500_gyro_offset_z;
uint8_t ssd1306_buffer[1024];
HAL_StatusTypeDef ssd1306_status, ssd1306_fill_status;
volatile uint8_t i2c2_last_lock_status = 0U;
volatile uint32_t eeprom_read_error_snapshot = 0U, eeprom_read_state_snapshot = 0U;
volatile uint32_t oled_lock_fail_count = 0U, oled_page_switch_count = 0U;
volatile uint32_t oled_i2c_error_snapshot = 0U, oled_i2c_state_snapshot = 0U;
volatile uint32_t oled_pre_state_snapshot = 0U, oled_pre_lock_snapshot = 0U;
volatile uint32_t oled_hw_busy_snapshot = 0U;
volatile uint8_t oled_sda_snapshot = 1U, oled_scl_snapshot = 1U;
volatile uint8_t oled_failed_page = 0xFFU, oled_failed_stage = 0U;
volatile uint32_t oled_render_overlap_count = 0U, oled_buffer_mutation_count = 0U;
volatile uint32_t oled_buffer_checksum_before = 0U, oled_buffer_checksum_after = 0U;
uint8_t i2c2_device_found;
int32_t display_aht_temp, display_aht_hum, display_pressure, display_accel_z;
uint8_t display_page = 0;
uint8_t eeprom_write_data, eeprom_read_data;
HAL_StatusTypeDef eeprom_write_status, eeprom_read_status, eeprom_test_status;
uint8_t eeprom_test_write[5], eeprom_test_read[5];
SensorRecordEEPROM eeprom_record, eeprom_record_read;
uint16_t eeprom_record_size;
uint8_t eeprom_records_equal;
SensorRecordEEPROM eeprom_records_write[5], eeprom_records_read[5];
uint8_t eeprom_multi_test_ok;
uint16_t eeprom_next_address = 0x0000U;
uint8_t eeprom_full = 0U, eeprom_status_display = 0U, eeprom_metadata_load_ok = 0U;
volatile uint8_t usb_export_requested = 0U;
volatile uint8_t usb_clear_requested = 0U;
volatile uint8_t usb_rtc_set_requested = 0U;
volatile uint8_t usb_diag_requested = 0U;
volatile uint8_t oled_task_stage = 0U;
volatile uint32_t oled_task_loops = 0U, oled_update_ok_count = 0U, oled_update_fail_count = 0U;
volatile uint32_t oled_consecutive_fail = 0U, i2c2_recovery_count = 0U, i2c2_recovery_status = 0U;
volatile uint32_t oled_ff_status = 0U, oled_ff_i2cerr = 0U, oled_ff_i2cstate = 0U;
volatile uint8_t oled_ff_sensor_active = 0U, sensor_cycle_active = 0U;

void App_Delay(uint32_t ms)
{
    if (osKernelGetState() == osKernelRunning)
        (void)osDelay(ms);
    else
        HAL_Delay(ms);
}
char usb_rtc_set_command[32] = {0};
EEPROMMetadata eeprom_metadata;
bool aht20_available = false, bmp280_available = false, mpu6500_available = false;
uint8_t aht20_last_read_ok = 0U, bmp280_last_read_ok = 0U, mpu6500_last_read_ok = 0U;
uint8_t usb_operation_state = 0U;
volatile uint8_t diagnostic_sensor_stage = 0U; /* 1=AHT20, 2=BMP280, 3=MPU6500, 4=EEPROM, 5=LED/ciclo concluido */
volatile uint8_t diagnostic_export_stage = 0U; /* 1=entrada, 2=validacao, 3=EXPORT_BEGIN, 4=cabecalho, 5=registros/fim */
volatile int8_t diagnostic_export_result = -1; /* -1=nao terminou; 0=HAL_OK; 1=erro; 2=timeout */
volatile int8_t diagnostic_usb_tx_result = -1; /* -1=sem envio; 0=OK; 1=BUSY; 2=FAIL/outro */
uint8_t oled_available = 0U;
uint8_t eeprom_full_read_buffer[64];
uint16_t eeprom_full_read_address;
HAL_StatusTypeDef eeprom_full_read_status;
