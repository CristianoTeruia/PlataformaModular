/**
 * @file oled_power.c
 * @brief Implementação da política de energia do OLED (ver oled_power.h).
 */
#include "oled_power.h"

void OledPower_Init(OledPower *state, uint32_t now_ms)
{
    state->on = 1U;
    state->button_prev = 0U;
    state->last_activity_ms = now_ms;
}

OledPowerAction OledPower_Update(OledPower *state, uint32_t now_ms, uint8_t button_down,
                                 uint8_t event, uint32_t timeout_ms)
{
    uint8_t press_edge = ((button_down != 0U) && (state->button_prev == 0U)) ? 1U : 0U;
    state->button_prev = (button_down != 0U) ? 1U : 0U;

    if ((press_edge != 0U) || (event != 0U))
    {
        state->last_activity_ms = now_ms;
        if (state->on == 0U)
        {
            state->on = 1U;
            return OLED_POWER_TURN_ON;
        }
        return OLED_POWER_NONE;
    }

    if ((state->on != 0U) && (timeout_ms != 0U) &&
        ((uint32_t)(now_ms - state->last_activity_ms) >= timeout_ms))
    {
        state->on = 0U;
        return OLED_POWER_TURN_OFF;
    }
    return OLED_POWER_NONE;
}

void OledPower_ActionFailed(OledPower *state, OledPowerAction action, uint32_t now_ms)
{
    if (action == OLED_POWER_TURN_ON)
    {
        state->on = 0U;            /* tenta de novo no proximo acionamento */
    }
    else if (action == OLED_POWER_TURN_OFF)
    {
        state->on = 1U;            /* continua aceso e tenta apagar depois de outro intervalo */
        state->last_activity_ms = now_ms;
    }
}
