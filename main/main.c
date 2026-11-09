#include <stdbool.h>

#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "maquina.h"

void app_main(void) {
  maquina_iniciar();

  // loop principal: a maquina nunca bloqueia, so confere tudo a cada 20 ms.
  while (true) {
    maquina_atualizar();
    vTaskDelay(pdMS_TO_TICKS(PERIODO_LOOP_MS));
  }
}
