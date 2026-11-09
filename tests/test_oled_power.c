/* Teste no PC da politica de energia do OLED (Core/Src/oled_power.c). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "oled_power.h"

static long fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("FALHA: " __VA_ARGS__); printf("\n"); } } while (0)

#define T 30000U

int main(void)
{
    OledPower s;
    OledPowerAction a;

    /* 1. apaga exatamente ao atingir o tempo, uma unica vez */
    OledPower_Init(&s, 1000);
    CHECK(s.on == 1, "inicia aceso");
    CHECK(OledPower_Update(&s, 1000 + T - 1, 0, 0, T) == OLED_POWER_NONE, "ainda aceso 1 ms antes do tempo");
    CHECK(OledPower_Update(&s, 1000 + T, 0, 0, T) == OLED_POWER_TURN_OFF && s.on == 0, "apaga no tempo exato");
    CHECK(OledPower_Update(&s, 1000 + T + 5, 0, 0, T) == OLED_POWER_NONE, "nao repete a acao de apagar");

    /* 2. botao acende (so na borda) e reinicia o tempo */
    CHECK(OledPower_Update(&s, 50000, 1, 0, T) == OLED_POWER_TURN_ON && s.on == 1, "botao acende");
    CHECK(OledPower_Update(&s, 50020, 1, 0, T) == OLED_POWER_NONE, "botao mantido nao repete");
    CHECK(OledPower_Update(&s, 50000 + T - 1, 0, 0, T) == OLED_POWER_NONE, "tempo reinicia a partir do botao");
    CHECK(OledPower_Update(&s, 50000 + T, 0, 0, T) == OLED_POWER_TURN_OFF, "apaga de novo apos o tempo");

    /* 3. aceso: o botao so estende o tempo, sem acao */
    OledPower_Init(&s, 0);
    CHECK(OledPower_Update(&s, 20000, 1, 0, T) == OLED_POWER_NONE, "botao com display aceso: sem acao");
    CHECK(OledPower_Update(&s, 20000, 0, 0, T) == OLED_POWER_NONE, "solta o botao");
    CHECK(OledPower_Update(&s, 20000 + T - 1, 0, 0, T) == OLED_POWER_NONE && s.on == 1, "tempo estendido pelo botao");
    CHECK(OledPower_Update(&s, 20000 + T, 0, 0, T) == OLED_POWER_TURN_OFF, "apaga depois do tempo estendido");

    /* 4. evento (ex.: inicio de exportacao USB) acende e estende */
    CHECK(OledPower_Update(&s, 90000, 0, 1, T) == OLED_POWER_TURN_ON, "evento acende");
    CHECK(OledPower_Update(&s, 100000, 0, 1, T) == OLED_POWER_NONE, "evento com display aceso estende o tempo");
    CHECK(OledPower_Update(&s, 100000 + T - 1, 0, 0, T) == OLED_POWER_NONE, "ainda aceso");

    /* 5. tempo 0 = sempre aceso */
    OledPower_Init(&s, 0);
    CHECK(OledPower_Update(&s, 4000000000U, 0, 0, 0) == OLED_POWER_NONE && s.on == 1, "timeout 0 nunca apaga");

    /* 6. virada do contador de 32 bits */
    OledPower_Init(&s, 0xFFFFFFFFU - 10000U);
    CHECK(OledPower_Update(&s, 0xFFFFFFFFU - 10000U + 29999U, 0, 0, T) == OLED_POWER_NONE, "antes da virada");
    CHECK(OledPower_Update(&s, 20000U - 1U, 0, 0, T) == OLED_POWER_TURN_OFF, "apaga corretamente apos a virada (%u ms)", (unsigned)(20000U + 10000U));

    /* 7. falha de I2C ao executar a acao */
    OledPower_Init(&s, 0);
    a = OledPower_Update(&s, T, 0, 0, T);
    CHECK(a == OLED_POWER_TURN_OFF && s.on == 0, "pede para apagar");
    OledPower_ActionFailed(&s, a, T);
    CHECK(s.on == 1, "falha ao apagar: continua aceso");
    CHECK(OledPower_Update(&s, T + T - 1, 0, 0, T) == OLED_POWER_NONE, "nova tentativa so apos outro intervalo");
    CHECK(OledPower_Update(&s, T + T, 0, 0, T) == OLED_POWER_TURN_OFF, "tenta apagar de novo");
    OledPower_Init(&s, 0); (void)OledPower_Update(&s, T, 0, 0, T);
    a = OledPower_Update(&s, T + 100, 1, 0, T);
    CHECK(a == OLED_POWER_TURN_ON && s.on == 1, "pede para acender");
    OledPower_ActionFailed(&s, a, T + 100);
    CHECK(s.on == 0, "falha ao acender: volta a apagado");
    CHECK(OledPower_Update(&s, T + 200, 0, 0, T) == OLED_POWER_NONE, "botao solto: sem acao");
    CHECK(OledPower_Update(&s, T + 300, 1, 0, T) == OLED_POWER_TURN_ON, "novo toque tenta acender de novo");

    /* 8. propriedades com sequencias aleatorias contra um modelo de referencia */
    {
        srand(2026);
        for (int run = 0; run < 200; run++) {
            uint32_t now = (uint32_t)rand() * 7919U, last = now, timeout = (run % 5 == 0) ? 0 : 1000U + (uint32_t)(rand() % 40000);
            int model_on = 1, prev_btn = 0, bad = 0;
            OledPower_Init(&s, now);
            for (int step = 0; step < 2000; step++) {
                int btn = (rand() % 40 == 0) ? !prev_btn : prev_btn;
                int ev = (rand() % 300 == 0);
                now += (uint32_t)(rand() % 400) + 1U;
                OledPowerAction act = OledPower_Update(&s, now, (uint8_t)btn, (uint8_t)ev, timeout);
                int edge = (btn && !prev_btn); prev_btn = btn;
                OledPowerAction want = OLED_POWER_NONE;
                if (edge || ev) { last = now; if (!model_on) { model_on = 1; want = OLED_POWER_TURN_ON; } }
                else if (model_on && timeout != 0 && (uint32_t)(now - last) >= timeout) { model_on = 0; want = OLED_POWER_TURN_OFF; }
                if (act != want || s.on != model_on) bad++;
                if (act == OLED_POWER_TURN_ON && s.on != 1) bad++;
                if (act == OLED_POWER_TURN_OFF && s.on != 0) bad++;
            }
            CHECK(bad == 0, "sequencia aleatoria %d divergiu do modelo (%d passos)", run, bad);
        }
    }

    printf("verificacoes: %ld, falhas: %ld\n", checks, fails);
    return fails ? 1 : 0;
}
