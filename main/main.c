#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "maquina.h"

void app_main(void) {
  maquina_iniciar();

  // loop principal: a maquina nunca bloqueia, so confere botoes/pot a cada 20
  // ms.
  while (true) {
    maquina_atualizar();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
