# Testes no PC (host). Requer apenas gcc e make; usa os cabecalhos do HAL/CMSIS que ja estao no repositorio.
ROOT   := ..
CC     ?= gcc
CFLAGS ?= -O1 -Wall
DEFS   := -DUSE_HAL_DRIVER -DSTM32F411xE
# -isystem silencia avisos dos cabecalhos de terceiros (HAL/CMSIS/FreeRTOS) compilados no PC.
INC    := -I$(ROOT)/Core/Inc \
          -isystem $(ROOT)/Drivers/STM32F4xx_HAL_Driver/Inc \
          -isystem $(ROOT)/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy \
          -isystem $(ROOT)/Drivers/CMSIS/Device/ST/STM32F4xx/Include \
          -isystem $(ROOT)/Drivers/CMSIS/Include \
          -isystem $(ROOT)/Middlewares/Third_Party/FreeRTOS/Source/include \
          -isystem $(ROOT)/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 \
          -isystem $(ROOT)/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
BUILD  := build

.PHONY: test clean
test: $(BUILD)/test_registry $(BUILD)/test_oled_power
	./$(BUILD)/test_registry
	./$(BUILD)/test_oled_power

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_registry: test_registry.c $(ROOT)/Core/Src/sensor_registry.c $(ROOT)/Core/Src/bh1750.c $(ROOT)/Core/Src/ens160.c | $(BUILD)
	$(CC) $(CFLAGS) $(DEFS) $(INC) $^ -o $@

$(BUILD)/test_oled_power: test_oled_power.c $(ROOT)/Core/Src/oled_power.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT)/Core/Inc $^ -o $@

clean:
	rm -rf $(BUILD)
