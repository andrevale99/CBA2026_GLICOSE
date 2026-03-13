#include "system.h"
#include "rtc_task.h"
#include "ble_task.h"

void system_init(void)
{
    ble_start_task();
    rtc_start_task();
}
