#ifndef PERIFERICOS_H
#define PERIFERICOS_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"

// Botao com debounce. A cada botao_atualizar() os eventos valem so uma vez:
//   clique = soltou sem ter sido pressao longa
//   longo  = completou o tempo de pressao longa (ainda segurado)
typedef struct {
  gpio_num_t pino;
  int64_t longo_ms; // 0 = botao sem pressao longa
  bool leitura_bruta;
  bool pressionado;
  bool longo_enviado;
  bool clique;
  bool longo;
  int64_t ultima_mudanca_ms;
  int64_t inicio_ms;
} Botao;

typedef enum {
  SOM_LIGAR,
  SOM_PRONTO,
  SOM_ERRO,
  SOM_CANCELAR,
  SOM_CONFIRMAR
} Som;

void perifericos_iniciar(void);

void botao_iniciar(Botao *botao, gpio_num_t pino, int64_t longo_ms,
                   int64_t agora_ms);
void botao_atualizar(Botao *botao, int64_t agora_ms);

// Retorna 0..POT_ESCALA-1 ja calibrado, ou -1 se o ADC falhar.
// "bruto" recebe a media crua do ADC e pode ser NULL.
int potenciometro_ler(int *bruto);

void leds_mostrar(bool verde, bool amarelo, bool vermelho);

void som_tocar(Som som);
void som_atualizar(int64_t agora_ms);
void som_parar(void);

#endif
