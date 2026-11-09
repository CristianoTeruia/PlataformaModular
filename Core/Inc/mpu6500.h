/**
 * @file mpu6500.h
 * @brief Driver do acelerômetro e giroscópio MPU6500 (I²C1, endereço 0x68).
 *
 * Fundos de escala fixos: ±2 g (16384 LSB/g) e ±250 °/s (131 LSB/(°/s)). Os resultados ficam
 * nas variáveis `mpu6500_*` de app_state.h.
 */
#ifndef MPU6500_H
#define MPU6500_H
#include "app_state.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Detecta e configura o sensor.
 *
 * Lê o WHO_AM_I (esperado 0x70), tira o sensor do modo de repouso, espera 100 ms e configura
 * os fundos de escala de ±2 g e ±250 °/s.
 * @retval HAL_OK    Sensor configurado.
 * @retval HAL_ERROR WHO_AM_I diferente de 0x70.
 * @return Status da HAL em caso de falha de comunicação I²C.
 */
HAL_StatusTypeDef MPU6500_Init(void);

/**
 * @brief Lê aceleração e velocidade angular e aplica os offsets.
 *
 * Lê 14 bytes a partir de ACCEL_XOUT_H (0x3B), converte para g e °/s, subtrai os offsets de
 * MPU6500_Calibrate() e atualiza as variáveis `mpu6500_*`. A temperatura interna do sensor é
 * descartada.
 * @retval HAL_OK Leitura válida.
 * @return Status da HAL em caso de falha de comunicação I²C.
 */
HAL_StatusTypeDef MPU6500_Read(void);

/**
 * @brief Coloca o sensor em repouso (bit SLEEP do `PWR_MGMT_1`) para economizar energia.
 *
 * O sensor dorme entre as amostras e mantém os registradores de configuração.
 * @return Status da HAL da escrita.
 */
HAL_StatusTypeDef MPU6500_Sleep(void);

/**
 * @brief Acorda o sensor e espera 100 ms para a estabilização do acelerômetro e do giroscópio.
 * @return Status da HAL da escrita.
 */
HAL_StatusTypeDef MPU6500_Wake(void);

/**
 * @brief Calcula os offsets (zeragem) com a placa em repouso.
 *
 * Faz a média de 500 leituras de MPU6500_Read(), uma a cada cerca de 5 ms. Em X e Y da
 * aceleração e nos três eixos do giroscópio o offset é essa média; em Z da aceleração é a média
 * menos 1 g, pois em repouso o eixo Z deve medir +1 g. Pressupõe a placa parada e com o eixo Z
 * na vertical.
 *
 * O registro de sensores chama esta função na primeira leitura após a partida e novamente
 * sempre que o sensor volta de uma falha.
 *
 * @warning A média é calculada sobre leituras que já descontam os offsets vigentes, e o resultado
 *          substitui os offsets. Isso só zera corretamente quando os offsets vigentes são nulos
 *          (primeira execução após a partida). Executada de novo sem reiniciar, o novo offset vale
 *          (média bruta - offset anterior): com a placa em repouso, as leituras voltam aos valores
 *          brutos, e uma terceira execução zera de novo, de modo que as execuções alternam entre
 *          zerar e desfazer a zeragem. O comportamento foi reproduzido no PC com esta função e
 *          leituras simuladas.
 *
 * @retval HAL_OK Offsets atualizados.
 * @return Status da primeira leitura que falhar; neste caso os offsets não são alterados.
 */
HAL_StatusTypeDef MPU6500_Calibrate(void);
#ifdef __cplusplus
}
#endif
#endif
