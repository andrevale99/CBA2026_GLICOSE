#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <stdio.h>

#include "sdmmc_cmd.h"

#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"

#define MOUNT_POINT "/sdcard"

#define SD_MAX_BUFFER_SIZE 1024

typedef struct {
    int mosi_pin;
    int miso_pin;
    int sclk_pin;
    int cs_pin;

    sdmmc_host_t host;

    sdmmc_card_t *card;

    FILE *file;

} sd_manager_config_t;

/**
 * @brief Inicializa o cartão SD via interface SPI e monta o sistema de arquivos FAT.
 *
 * Esta função configura o barramento SPI, inicializa o host SD, monta o sistema
 * de arquivos no ponto de montagem definido por MOUNT_POINT e armazena as
 * informações do cartão na estrutura de configuração.
 *
 * @param[in,out] config Ponteiro para a estrutura de configuração do gerenciador SD.
 *                       Deve conter os pinos SPI (MOSI, MISO, SCLK, CS) válidos.
 *                       Ao final da execução, o campo `card` será preenchido.
 *
 * @return
 *      - ESP_OK: Cartão inicializado e sistema de arquivos montado com sucesso.
 *      - ESP_FAIL: Falha ao montar o sistema de arquivos.
 *      - Outro código esp_err_t: Falha na inicialização do barramento SPI.
 */
esp_err_t sd_init(sd_manager_config_t *sd);

/**
 * @brief Desmonta o sistema de arquivos do cartão SD e libera o barramento SPI.
 *
 * Esta função desmonta o sistema de arquivos previamente montado e
 * desaloca os recursos do barramento SPI associados ao host configurado.
 *
 * @param[in,out] config Ponteiro para a estrutura de configuração do gerenciador SD.
 *
 * @return
 *      - ESP_OK: Desinicialização realizada com sucesso.
 */
esp_err_t sd_deinit(sd_manager_config_t *sd);

/**
 * @brief Executa a formatação (apagamento completo) do cartão SD.
 *
 * Realiza o apagamento completo do cartão utilizando a API do driver SDMMC.
 *
 * @param[in] sd Ponteiro para a estrutura de configuração do gerenciador SD,
 *               que deve conter o campo `card` previamente inicializado.
 *
 * @return
 *      - ESP_OK: Formatação realizada com sucesso.
 *      - Outro código esp_err_t: Falha durante o processo de apagamento.
 */
esp_err_t sd_format(sd_manager_config_t *sd);

/**
 * @brief Lê uma linha de um arquivo no cartão SD.
 *
 * Abre o arquivo especificado em modo leitura, lê até SD_MAX_BUFFER_SIZE
 * caracteres para o buffer e remove o caractere de nova linha, se presente.
 *
 * @param[in,out] config Ponteiro para a estrutura de configuração do gerenciador SD.
 * @param[in] filename Caminho do arquivo a ser lido.
 * @param[out] buffer Buffer onde o conteúdo lido será armazenado.
 *
 * @return
 *      - ESP_OK: Leitura realizada com sucesso.
 *      - ESP_FAIL: Falha ao abrir o arquivo.
 */
esp_err_t sd_read_file(sd_manager_config_t *sd, const char *filename, char *buffer);

/**
 * @brief Escreve dados em um arquivo no cartão SD.
 *
 * Abre o arquivo em modo append e grava a string fornecida.
 * O arquivo é criado caso não exista.
 *
 * @param[in,out] config Ponteiro para a estrutura de configuração do gerenciador SD.
 * @param[in] filename Caminho do arquivo a ser escrito.
 * @param[in] data String a ser gravada no arquivo.
 *
 * @return
 *      - ESP_OK: Escrita realizada com sucesso.
 *      - ESP_FAIL: Falha ao abrir o arquivo.
 */
esp_err_t sd_write_file(sd_manager_config_t *sd, const char *filename, const char *data);

/**
 * @brief Remove um arquivo do cartão SD.
 *
 * @param[in,out] config Ponteiro para a estrutura de configuração do gerenciador SD.
 * @param[in] filename Caminho do arquivo a ser removido.
 *
 * @return
 *      - ESP_OK: Arquivo removido com sucesso.
 *      - ESP_FAIL: Falha ao remover o arquivo.
 */
esp_err_t sd_delete_file(sd_manager_config_t *sd, const char *filename);

/**
 * @brief Verifica se um arquivo existe no cartão SD.
 *
 * Tenta abrir o arquivo em modo leitura para determinar sua existência.
 *
 * @param[in,out] config Ponteiro para a estrutura de configuração do gerenciador SD.
 * @param[in] filename Caminho do arquivo a ser verificado.
 *
 * @return
 *      - true: O arquivo existe.
 *      - false: O arquivo não existe.
 */
bool sd_file_exists(sd_manager_config_t *sd, const char *filename);

#endif