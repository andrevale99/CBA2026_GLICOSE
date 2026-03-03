#ifndef LVGL_PORT_H
#define LVGL_PORT_H

#include "esp_err.h"
#include "ihm_50c.h"

#ifdef __cplusplus
extern "C"
{
#endif

    esp_err_t lvgl_port_init(ihm_50c_t *ihm);
    void lvgl_port_lock(void);
    void lvgl_port_unlock(void);

#ifdef __cplusplus
}
#endif

#endif // LVGL_PORT_H