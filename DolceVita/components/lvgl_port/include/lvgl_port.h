#ifndef LVGL_PORT_H
#define LVGL_PORT_H

#include "esp_err.h"
#include "ihm_50c.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =====================================================================
 * lvgl_port — Integração LVGL v9 + ESP-IDF v5.3 + IHM-50C
 *
 * Uso obrigatório:
 *   lvgl_port_init(ihm);
 *
 *   lvgl_port_lock();
 *   ui_main_create();     <- toda criação LVGL dentro do lock
 *   lvgl_port_unlock();
 *
 * LVGL roda na lvgl_task (core 1) de forma autônoma após o init.
 * Nunca acesse objetos LVGL de outra task sem o lock.
 * ===================================================================== */

esp_err_t lvgl_port_init(ihm_50c_t *ihm);

void lvgl_port_lock(void);
void lvgl_port_unlock(void);

/**
 * @brief Tenta adquirir o lock com timeout.
 * @param timeout_ms  0 = tenta sem bloquear.
 * @return true se adquirido.
 */
bool lvgl_port_lock_timeout(uint32_t timeout_ms);

bool        lvgl_port_is_ready(void);
TaskHandle_t lvgl_port_get_task_handle(void);

#ifdef __cplusplus
}
#endif

#endif /* LVGL_PORT_H */