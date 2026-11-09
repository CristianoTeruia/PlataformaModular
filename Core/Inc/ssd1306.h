/**
 * @file ssd1306.h
 * @brief Driver do display OLED SSD1306 (128 x 64 pixels, I²C2, endereço 0x3D) e telas do sistema.
 *
 * O desenho é feito em um buffer na RAM (`ssd1306_buffer`, 1024 bytes) e enviado ao display por
 * SSD1306_UpdateScreen(). A fonte é de 5 x 7 pixels e cobre apenas dígitos, letras maiúsculas e
 * os caracteres `.`, `:`, `%` e `-`. As transações obtêm o mutex do I²C2.
 */
#ifndef SSD1306_H
#define SSD1306_H
#include "app_state.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Cópia do estado do sistema usada para renderizar as telas.
 *
 * É preenchida pela `oledTask` sob mutexes curtos, de modo que a renderização e as transações
 * I²C2 ocorram depois de liberá-los.
 */
typedef struct
{
    uint8_t oled_available;       /**< 1 se o display respondeu na inicialização. */
    uint8_t display_page;         /**< Tela a desenhar: 0 = estado do sistema; 1 = armazenamento. */
    uint8_t sensor_present_mask;  /**< Sensores respondendo agora (bits do registro). */
    uint8_t sensor_detected_mask; /**< Sensores que já responderam desde o boot. */
    uint8_t rtc_time_valid;       /**< 1 se a hora do RTC é válida. */
    uint8_t eeprom_status_display;/**< 0 OK, 1 erro, 2 memória cheia. */
    uint8_t eeprom_metadata_load_ok; /**< 1 se os metadados da EEPROM foram carregados. */
    uint8_t eeprom_full;          /**< 1 se as duas EEPROMs estão cheias. */
    uint8_t eeprom_active;        /**< Chip ativo (#EEPROM_ACTIVE_EEPROM1 ou #EEPROM_ACTIVE_EEPROM2). */
    uint16_t eeprom_next_address; /**< Próximo endereço livre no chip ativo. */
    uint8_t usb_operation_state;  /**< 0 livre, 1 exportando, 2 limpando, 3 erro. */
    uint8_t diagnostic_sensor_stage;   /**< Estágio do ciclo de aquisição (ver `diagnostic_sensor_stage`). */
    uint8_t diagnostic_export_stage;   /**< Estágio da exportação (ver `diagnostic_export_stage`). */
    int8_t diagnostic_export_result;   /**< Resultado da exportação (ver `diagnostic_export_result`). */
    int8_t diagnostic_usb_tx_result;   /**< Resultado do último envio USB (ver `diagnostic_usb_tx_result`). */
    HAL_StatusTypeDef ssd1306_status;  /**< Status HAL da última atualização do display. */
} DisplaySnapshot;

/**
 * @brief Desenha uma das duas telas e a envia ao display.
 *
 * A tela 0 mostra o estado geral (OK, AVISO ou ERRO), os sensores saudáveis sobre os esperados, a
 * validade do RTC e a primeira causa de erro. A tela 1 mostra a EEPROM ativa, o próximo endereço, o
 * estado da USB e a linha de estágios de diagnóstico. Os sensores esperados são os já detectados
 * desde o boot: um módulo não conectado não é erro, e sim um sensor que parou de responder.
 * @param[in] snapshot Estado a exibir; ignorado se for nulo ou se o display não estiver disponível.
 */
void UpdateDisplaySnapshot(const DisplaySnapshot *snapshot);

/**
 * @brief Inicializa o SSD1306 (clock, multiplexação de 64 linhas, bomba de carga e display ligado).
 *
 * Não obtém o mutex do I²C2: chamar antes do início do RTOS ou com o mutex já obtido.
 * @retval HAL_OK    Display inicializado.
 * @retval HAL_ERROR Qualquer comando de inicialização falhou.
 */
HAL_StatusTypeDef SSD1306_Init(void);

/**
 * @brief Liga ou desliga o display e a bomba de carga, para economizar energia.
 *
 * Usa as sequências do datasheet; o conteúdo da RAM do display é mantido. Obtém o mutex do I²C2.
 * @param[in] on `true` para ligar; `false` para desligar.
 * @return Status da HAL do primeiro comando que falhar, ou `HAL_OK`.
 */
HAL_StatusTypeDef SSD1306_SetPower(bool on);

/**
 * @brief Verifica se o display responde no barramento.
 * @retval HAL_OK Display respondeu.
 */
HAL_StatusTypeDef SSD1306_TestConnection(void);

/** @brief Varre os endereços 1 a 127 do I²C2 e guarda em `i2c2_device_found` o último que respondeu (depuração). */
void I2C2_Scan_All(void);

/**
 * @brief Acende todos os pixels do display (teste), enviando diretamente, sem usar o buffer.
 * @return Status da HAL da primeira transferência que falhar, ou `HAL_OK`.
 */
HAL_StatusTypeDef SSD1306_Fill(void);

/** @brief Apaga o buffer na RAM; o display só muda após SSD1306_UpdateScreen(). */
void SSD1306_Clear(void);

/**
 * @brief Envia o buffer ao display, página a página (8 páginas de 128 bytes), sob o mutex do I²C2.
 *
 * Em falha, atualiza `ssd1306_status` e os contadores e instantâneos `oled_*` de diagnóstico.
 */
void SSD1306_UpdateScreen(void);

/**
 * @brief Acende ou apaga um pixel no buffer.
 * @param[in] x     Coluna (0 a 127); valores fora da tela são ignorados.
 * @param[in] y     Linha (0 a 63); valores fora da tela são ignorados.
 * @param[in] color 0 apaga o pixel; qualquer outro valor o acende.
 */
void SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t color);

/**
 * @brief Desenha um caractere de 5 x 7 pixels no buffer.
 *
 * Só há glifos para `0`-`9`, `A`-`Z`, `.`, `:`, `%` e `-`; os demais caracteres (inclusive letras
 * minúsculas) são ignorados.
 * @param[in] x Coluna do canto superior esquerdo.
 * @param[in] y Linha do canto superior esquerdo.
 * @param[in] c Caractere.
 */
void SSD1306_DrawChar(uint8_t x, uint8_t y, char c);

/**
 * @brief Desenha uma cadeia de caracteres no buffer (6 pixels por caractere).
 * @param[in] x   Coluna inicial.
 * @param[in] y   Linha inicial.
 * @param[in] str Texto terminado em zero.
 */
void SSD1306_DrawString(uint8_t x, uint8_t y, const char *str);

/**
 * @brief Desenha um inteiro sem sinal em decimal.
 * @param[in] x      Coluna inicial.
 * @param[in] y      Linha inicial.
 * @param[in] number Valor.
 */
void SSD1306_DrawUInt(uint8_t x, uint8_t y, uint32_t number);

/**
 * @brief Desenha um valor com uma casa decimal a partir de um inteiro em décimos (-123 vira `-12.3`).
 * @param[in] x     Coluna inicial.
 * @param[in] y     Linha inicial.
 * @param[in] value Valor em décimos.
 */
void SSD1306_DrawFixed1(uint8_t x, uint8_t y, int32_t value);

/**
 * @brief Monta o estado a partir das variáveis globais e renderiza.
 *
 * Usada antes do início do RTOS; depois dele a `oledTask` monta o estado sob mutexes
 * (UpdateDisplaySynchronized()).
 */
void UpdateDisplay(void);

/**
 * @brief Desenha o estado da EEPROM no formato curto `EEPROM:OK`, `EEPROM:ERR` ou `EEPROM:FULL` (legado).
 * @param[in] x Coluna inicial.
 * @param[in] y Linha inicial.
 */
void SSD1306_DrawEEPROMStatus(uint8_t x, uint8_t y);
#ifdef __cplusplus
}
#endif
#endif
