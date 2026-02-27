#ifndef SPIFFS_MANAGER_H
#define SPIFFS_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Inicializa e monta o sistema de arquivos SPIFFS.
 *
 * Registra a partição SPIFFS no VFS, monta o sistema de arquivos
 * e obtém informações de uso da partição.
 *
 * @return
 *      - ESP_OK: SPIFFS montado com sucesso.
 *      - Outro código esp_err_t: Falha ao montar o SPIFFS.
 */
esp_err_t spiffs_init(void);

/**
 * @brief Formata a partição SPIFFS.
 *
 * Executa a formatação completa do sistema de arquivos SPIFFS.
 *
 * @return
 *      - ESP_OK: Formatação realizada com sucesso.
 *      - Outro código esp_err_t: Falha na formatação.
 */
esp_err_t spiffs_format(void);

/**
 * @brief Lê completamente um arquivo do SPIFFS para memória.
 *
 * Abre o arquivo em modo binário, aloca memória dinamicamente
 * e copia todo o conteúdo para o buffer.
 *
 * @param[in] filename Nome do arquivo relativo ao SPIFFS.
 * @param[out] buffer Ponteiro que receberá o buffer alocado com os dados.
 *                    O chamador é responsável por liberar com free().
 * @param[out] length Ponteiro que receberá o tamanho do arquivo em bytes.
 *
 * @return
 *      - ESP_OK: Leitura realizada com sucesso.
 *      - ESP_FAIL: Falha ao abrir o arquivo.
 *      - ESP_ERR_NO_MEM: Memória insuficiente para alocação.
 */
esp_err_t spiffs_read_file(const char *filename, char **buffer, size_t *length);

/**
 * @brief Escreve dados em um arquivo no SPIFFS.
 *
 * Cria ou sobrescreve o arquivo especificado e grava os dados
 * fornecidos em modo binário.
 *
 * @param[in] filename Nome do arquivo relativo ao SPIFFS.
 * @param[in] data Ponteiro para os dados a serem gravados.
 * @param[in] length Tamanho dos dados em bytes.
 *
 * @return
 *      - ESP_OK: Escrita realizada com sucesso.
 *      - ESP_FAIL: Falha ao abrir ou escrever o arquivo.
 */
esp_err_t spiffs_write_file(const char *filename, const char *data, size_t length);

/**
 * @brief Remove um arquivo do SPIFFS.
 *
 * @param[in] filename Nome do arquivo relativo ao SPIFFS.
 *
 * @return
 *      - ESP_OK: Arquivo removido com sucesso.
 *      - ESP_FAIL: Falha ao remover o arquivo.
 */
esp_err_t spiffs_delete_file(const char *filename);

/**
 * @brief Verifica se um arquivo existe no SPIFFS.
 *
 * @param[in] filename Nome do arquivo relativo ao SPIFFS.
 *
 * @return
 *      - true: O arquivo existe.
 *      - false: O arquivo não existe.
 */
bool spiffs_file_exists(const char *filename);

#endif
