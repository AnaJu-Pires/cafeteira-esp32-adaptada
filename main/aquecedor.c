#include <stdbool.h>

#include "aquecedor.h"
#include "config.h"

#define FAIXA_C (TEMP_ALVO_C - TEMP_AMBIENTE_C)

static bool ligado = false;
static int temp_base_c = TEMP_AMBIENTE_C; // temperatura no ultimo liga/desliga
static int64_t base_ms = 0;               // quando foi o ultimo liga/desliga

int64_t aquecedor_restante_ms(int64_t agora_ms) {
  if (!ligado)
    return 0;
  // sobe em linha reta: com o bloco morno o tempo e proporcional ao que falta.
  int64_t total_ms =
      (int64_t)(TEMP_ALVO_C - temp_base_c) * AQUECIMENTO_MS / FAIXA_C;
  int64_t restante = total_ms - (agora_ms - base_ms);
  return restante > 0 ? restante : 0;
}

int aquecedor_temperatura(int64_t agora_ms) {
  if (ligado)
    return TEMP_ALVO_C -
           (int)(aquecedor_restante_ms(agora_ms) * FAIXA_C / AQUECIMENTO_MS);

  int esfriou = (int)((agora_ms - base_ms) * RESFRIAMENTO_C_MINUTO / 60000);
  int temp = temp_base_c - esfriou;
  return temp > TEMP_AMBIENTE_C ? temp : TEMP_AMBIENTE_C;
}

void aquecedor_ligar(int64_t agora_ms) {
  if (ligado)
    return;
  // religou logo depois de desligar: o bloco ainda esta morno e aquece antes.
  temp_base_c = aquecedor_temperatura(agora_ms);
  base_ms = agora_ms;
  ligado = true;
}

void aquecedor_desligar(int64_t agora_ms) {
  if (!ligado)
    return;
  temp_base_c = aquecedor_temperatura(agora_ms);
  base_ms = agora_ms;
  ligado = false;
}
