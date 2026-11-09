#include <stdbool.h>

#include "aquecedor.h"
#include "config.h"

#define FAIXA_C (TEMP_ALVO_C - TEMP_AMBIENTE_C)

static bool ligado = false;
static int temp_base_c = TEMP_AMBIENTE_C; // temperatura no ultimo marco
static int64_t base_ms = 0;               // quando foi o ultimo marco
static int64_t duracao_ms = 0; // quanto levaria do ambiente ate o alvo

// mais agua no tanque = mais massa para esquentar = mais tempo.
static int64_t duracao_para(int agua_ml) {
  return AQUECIMENTO_BASE_MS + (int64_t)agua_ml * AQUECIMENTO_MS_POR_ML;
}

int64_t aquecedor_restante_ms(int64_t agora_ms) {
  if (!ligado)
    return 0;
  // sobe em linha reta: com agua morna o tempo e proporcional ao que falta.
  int64_t total_ms = (TEMP_ALVO_C - temp_base_c) * duracao_ms / FAIXA_C;
  int64_t restante = total_ms - (agora_ms - base_ms);
  return restante > 0 ? restante : 0;
}

int aquecedor_temperatura(int64_t agora_ms) {
  if (ligado)
    return TEMP_ALVO_C -
           (int)(aquecedor_restante_ms(agora_ms) * FAIXA_C / duracao_ms);

  int esfriou = (int)((agora_ms - base_ms) * RESFRIAMENTO_C_MINUTO / 60000);
  int temp = temp_base_c - esfriou;
  return temp > TEMP_AMBIENTE_C ? temp : TEMP_AMBIENTE_C;
}

void aquecedor_ligar(int64_t agora_ms, int agua_ml) {
  if (ligado)
    return;
  // religou logo depois de desligar: a agua ainda esta morna e aquece antes.
  temp_base_c = aquecedor_temperatura(agora_ms);
  base_ms = agora_ms;
  duracao_ms = duracao_para(agua_ml);
  ligado = true;
}

void aquecedor_ajustar_agua(int64_t agora_ms, int agua_ml) {
  if (!ligado)
    return;
  // guarda a temperatura de agora e segue dela com o novo ritmo.
  temp_base_c = aquecedor_temperatura(agora_ms);
  base_ms = agora_ms;
  duracao_ms = duracao_para(agua_ml);
}

void aquecedor_desligar(int64_t agora_ms) {
  if (!ligado)
    return;
  temp_base_c = aquecedor_temperatura(agora_ms);
  base_ms = agora_ms;
  ligado = false;
}
