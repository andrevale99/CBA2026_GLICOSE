#ifndef DS3231_H
#define DS3231_H

#include <time.h>

#include "freertos/FreeRTOS.h"

#include "driver/i2c_master.h"

#include "esp_err.h"
#include "esp_log.h"

#define DS3231_ADDRESS 0x68

#define DS3231_REG_TIME_SECONDS 0x00
#define DS3231_REG_TIME_MINUTES 0x01
#define DS3231_REG_TIME_HOURS   0x02
#define DS3231_REG_DATE         0x04
#define DS3231_REG_MONTH       0x05
#define DS3231_REG_YEAR        0x06

#define DS3231_TIMEOUT_MS 100

/**
 * @brief Inicializa o dispositivo DS3231 no barramento I2C.
 *
 * Verifica a presença do DS3231 no endereço informado e adiciona
 * o dispositivo ao barramento I2C configurado.
 *
 * @param[in] handle Ponteiro para o handle do barramento I2C master.
 * @param[in] address Endereço I2C do DS3231 (7 bits).
 *
 * @return
 *      - ESP_OK: Dispositivo encontrado e adicionado com sucesso.
 *      - Outro código esp_err_t: Falha na comunicação ou na adição do dispositivo.
 */
esp_err_t ds3231_init(i2c_master_bus_handle_t *handle, uint16_t address);

/**
 * @brief Configura a data e hora no RTC DS3231.
 *
 * Converte os campos da estrutura `struct tm` para BCD e escreve
 * nos registradores de tempo do dispositivo.
 *
 * @param[in] timeinfo Ponteiro para a estrutura contendo data e hora
 *                     no formato padrão C (`struct tm`).
 *
 * @return
 *      - ESP_OK: Tempo configurado com sucesso.
 *      - Outro código esp_err_t: Falha na transmissão I2C.
 */
esp_err_t ds3231_set_time(const struct tm *timeinfo);

/**
 * @brief Lê a data e hora atuais do RTC DS3231.
 *
 * Realiza a leitura dos registradores de tempo do dispositivo,
 * converte os valores de BCD para decimal e preenche a estrutura
 * `struct tm`.
 *
 * @param[out] timeinfo Ponteiro para a estrutura que receberá
 *                      a data e hora lidas.
 *
 * @return
 *      - ESP_OK: Leitura realizada com sucesso.
 *      - Outro código esp_err_t: Falha na comunicação I2C.
 */
esp_err_t ds3231_get_time(struct tm *timeinfo);


#endif