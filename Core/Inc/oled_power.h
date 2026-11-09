/**
 * @file oled_power.h
 * @brief Política de energia do OLED: apaga sozinho por inatividade e acende por evento.
 *
 * Lógica pura, sem dependência da HAL, testada no PC em `tests/` (`test_oled_power.c`).
 */
#ifndef OLED_POWER_H
#define OLED_POWER_H

#include <stdint.h>

/** @brief Estado da política de energia do OLED. */
typedef struct
{
    uint8_t on;                 /**< 1 = display aceso. */
    uint8_t button_prev;        /**< Estado anterior do botão, para detectar a borda de descida. */
    uint32_t last_activity_ms;  /**< Instante da última atividade (botão ou evento), em ms. */
} OledPower;

/** @brief Ação que o chamador deve executar no display. */
typedef enum
{
    OLED_POWER_NONE = 0,   /**< Nada a fazer. */
    OLED_POWER_TURN_ON,    /**< Acender o display. */
    OLED_POWER_TURN_OFF    /**< Apagar o display. */
} OledPowerAction;

/**
 * @brief Inicializa o estado com o display aceso.
 * @param[out] state  Estado a inicializar.
 * @param[in]  now_ms Instante atual, em ms.
 */
void OledPower_Init(OledPower *state, uint32_t now_ms);

/**
 * @brief Atualiza a política e informa a ação necessária.
 *
 * O botão acende o display na borda de descida (pressionado após ter estado solto); um evento
 * também o acende. Sem atividade por @p timeout_ms, a função manda apagar.
 *
 * @param[in,out] state       Estado da política.
 * @param[in]     now_ms      Instante atual, em ms.
 * @param[in]     button_down Botão pressionado (já filtrado).
 * @param[in]     event       1 em UM ciclo quando ocorre algo que deve acender o display.
 * @param[in]     timeout_ms  Tempo de inatividade até apagar; 0 desativa o desligamento.
 * @return Ação a executar no display.
 */
OledPowerAction OledPower_Update(OledPower *state, uint32_t now_ms, uint8_t button_down,
                                 uint8_t event, uint32_t timeout_ms);

/**
 * @brief Desfaz a mudança de estado quando a ação retornada não pôde ser executada.
 *
 * Chamar se a ação falhou (por exemplo, erro de I²C). Se acender falhou, o estado volta a
 * "apagado" e a tentativa se repete no próximo acionamento; se apagar falhou, o display continua
 * "aceso" e uma nova tentativa ocorre após outro intervalo de inatividade.
 *
 * @param[in,out] state  Estado da política.
 * @param[in]     action Ação que falhou.
 * @param[in]     now_ms Instante atual, em ms.
 */
void OledPower_ActionFailed(OledPower *state, OledPowerAction action, uint32_t now_ms);

#endif
