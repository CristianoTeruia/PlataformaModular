V6 - instrumentacao adicional de exportacao e progresso dos sensores

S = ultimo estagio da tarefa de sensores: 1 AHT20, 2 BMP280, 3 MPU6500, 4 EEPROM, 5 chegou a alternancia do LED.
E = estagio da exportacao: 0 inativo/nao iniciado, 1 solicitacao recebida, 2 validacao/mutex, 3 tentativa EXPORT_BEGIN, 4 cabecalho, 5 registros/resumo, 6 rotina retornou.
T = ultimo retorno CDC: 0 OK, 1 BUSY, 2 falha/timeout, 9 ainda sem envio.
R = retorno da exportacao: 0 HAL_OK, 1 erro, 2 timeout, 9 ainda sem retorno.

Os codigos S/E/T/R aparecem na pagina de armazenamento do OLED (alternancia de pagina a cada 3 s).
Esta versao e instrumentacao experimental. Nao foi compilada no STM32CubeIDE nem validada em hardware.
Nao execute comandos CLEAR; a instrumentacao nao requer apagar EEPROM. Preserve a V5.
