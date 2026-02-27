#ifndef _WIFI_LIB_H
#define _WIFI_LIB_H

#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        char ssid[32];
        char password[64];
    } wifi_net_cred_t;

    typedef void (*wifi_event_cb_t)(wifi_event_t event, void *arg);

    /**
     * @brief Inicializa o Wi-Fi em modo Station (STA).
     *
     * Configura o NVS, inicializa o driver Wi-Fi, registra os handlers de evento
     * e carrega a primeira credencial da lista para conexão.
     *
     * @param[in] cred_list Lista de credenciais Wi-Fi (SSID/senha).
     * @param[in] list_size Número de elementos na lista de credenciais.
     *
     * @return
     *      - ESP_OK: Inicialização realizada com sucesso.
     *      - ESP_ERR_INVALID_ARG: Argumentos inválidos.
     *      - Outro código esp_err_t: Falha na inicialização.
     */
    esp_err_t wifi_init_sta(const wifi_net_cred_t *cred_list, size_t list_size);

    /**
     * @brief Inicializa o Wi-Fi em modo Access Point (AP).
     *
     * Configura e inicia o ESP32 como ponto de acesso usando as
     * credenciais fornecidas.
     *
     * @param[in] cred Estrutura contendo SSID e senha do AP.
     *
     * @return
     *      - ESP_OK: AP iniciado com sucesso.
     *      - Outro código esp_err_t: Falha na inicialização.
     */
    esp_err_t wifi_init_ap(const wifi_net_cred_t cred);

    /**
     * @brief Inicia a conexão Wi-Fi no modo Station.
     *
     * Aplica a configuração atual e aguarda o resultado da conexão
     * através do event group.
     *
     * @return
     *      - ESP_OK: Conectado com sucesso.
     *      - ESP_FAIL: Falha na conexão.
     *      - ESP_ERR_TIMEOUT: Tempo de espera excedido.
     *      - Outro código esp_err_t: Erro ao iniciar conexão.
     */
    esp_err_t wifi_connect(void);

    /**
     * @brief Verifica se o Wi-Fi está conectado.
     *
     * @return
     *      - true: Conectado.
     *      - false: Não conectado.
     */
    bool wifi_is_connected(void);

    /**
     * @brief Habilita ou desabilita reconexão automática do Wi-Fi.
     *
     * @param[in] enable true para habilitar, false para desabilitar.
     *
     * @return
     *      - ESP_OK: Operação realizada.
     */
    esp_err_t wifi_enable_auto_reconnect(bool enable);

    /**
     * @brief Realiza varredura de redes Wi-Fi disponíveis.
     *
     * Executa um scan bloqueante e preenche o buffer de resultados.
     *
     * @param[out] results Buffer para armazenar os registros encontrados.
     * @param[in,out] count Entrada: tamanho máximo do buffer.
     *                      Saída: número de APs encontrados.
     *
     * @return
     *      - ESP_OK: Scan concluído com sucesso.
     *      - ESP_ERR_INVALID_ARG: Argumentos inválidos.
     *      - Outro código esp_err_t: Falha no scan.
     */
    esp_err_t wifi_scan(wifi_ap_record_t *results, uint16_t *count);

    /**
     * @brief Obtém o endereço IP da interface Station.
     *
     * @param[out] ip_str Buffer que receberá o IP em formato string.
     * @param[in] len Tamanho do buffer (mínimo recomendado: 16).
     *
     * @return
     *      - ESP_OK: IP obtido com sucesso.
     *      - ESP_ERR_INVALID_ARG: Argumentos inválidos.
     *      - Outro código esp_err_t: Falha ao obter IP.
     */
    esp_err_t wifi_get_ip(char *ip_str, size_t len);

    /**
     * @brief Obtém o endereço MAC da interface Station.
     *
     * @param[out] mac_out Buffer que receberá o MAC em formato
     *                     "XX:XX:XX:XX:XX:XX".
     *
     * @return
     *      - ESP_OK: MAC obtido com sucesso.
     *      - ESP_ERR_INVALID_ARG: Argumento inválido.
     *      - Outro código esp_err_t: Falha ao obter MAC.
     */
    esp_err_t wifi_get_mac(char *mac_out);

    /**
     * @brief Para o driver Wi-Fi e libera recursos.
     *
     * Desregistra handlers de evento, remove o event group e
     * limpa estados internos.
     *
     * @return
     *      - ESP_OK: Wi-Fi parado com sucesso.
     *      - Outro código esp_err_t: Falha ao parar.
     */
    esp_err_t wifi_stop(void);

    /**
     * @brief Desconecta da rede Wi-Fi atual.
     *
     * @return
     *      - ESP_OK: Desconectado com sucesso.
     *      - Outro código esp_err_t: Falha ao desconectar.
     */
    esp_err_t wifi_disconnect(void);

    /**
     * @brief Salva ou atualiza uma credencial Wi-Fi no arquivo.
     *
     * Se o SSID já existir, a senha é atualizada. Caso contrário,
     * uma nova entrada é adicionada.
     *
     * @param[in] ssid Nome da rede Wi-Fi.
     * @param[in] password Senha da rede.
     *
     * @return
     *      - ESP_OK: Credencial salva com sucesso.
     *      - ESP_ERR_INVALID_ARG: Argumentos inválidos.
     *      - ESP_FAIL: Falha na operação.
     */
    esp_err_t wifi_save_credential(const char *ssid, const char *password);

    /**
     * @brief Carrega credenciais Wi-Fi do arquivo.
     *
     * Lê o arquivo de credenciais e preenche o buffer fornecido.
     *
     * @param[out] cred_list Buffer para armazenar as credenciais.
     * @param[in] max_count Número máximo de entradas no buffer.
     * @param[out] out_count Número de credenciais carregadas.
     *
     * @return
     *      - ESP_OK: Pelo menos uma credencial carregada.
     *      - ESP_FAIL: Nenhuma credencial encontrada ou erro.
     *      - ESP_ERR_INVALID_ARG: Argumentos inválidos.
     */
    esp_err_t wifi_load_credentials(wifi_net_cred_t *cred_list, size_t max_count, size_t *out_count);

    /**
     * @brief Remove o arquivo de credenciais Wi-Fi.
     *
     * @return
     *      - ESP_OK: Arquivo removido.
     *      - ESP_FAIL: Falha ou arquivo inexistente.
     */
    esp_err_t wifi_clear_credentials(void);

#ifdef __cplusplus
}
#endif

#endif // _CONFIG_H
