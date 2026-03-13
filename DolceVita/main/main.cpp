#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "system.h"

extern "C" void app_main(void)
{
   system_init();
}