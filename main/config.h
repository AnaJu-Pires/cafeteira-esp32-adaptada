#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"
#include "hal/adc_types.h"

// ---------- pinos (PCB do projeto lavadora: ESP32 WROOM-32, 38 pinos) ----------
#define LED_VERDE GPIO_NUM_17
#define LED_AMARELO GPIO_NUM_18
#define LED_VERMELHO GPIO_NUM_16
#define BTN1_PIN GPIO_NUM_32
#define BTN2_PIN GPIO_NUM_33
#define BUZZER_PIN GPIO_NUM_25 // saida PWM para base do BC337 via resistor
#define POT_CANAL ADC_CHANNEL_6 // GPIO34 = ADC1 canal 6 (entrada analogica)

// ---------- estoque ----------
#define AGUA_MAX_ML 1000
#define CAPSULAS_MAX 5
#define AGUA_INICIAL_ML 600
#define CAPSULAS_INICIAIS 3
#define AGUA_PASSO_ML 200 // reposicao de agua de 200 em 200

// ---------- receitas ----------
#define CAFE_CURTO_ML 40
#define CAFE_LONGO_ML 120
#define VAZAO_ML_SEGUNDO 10

// ---------- botoes e inatividade ----------
#define DEBOUNCE_MS 40
#define SEGURAR_BTN1_MS 3000
#define TIMEOUT_MS 60000

// ---------- potenciometro ----------
// POT_ADC_MINIMO/MAXIMO: leitura bruta do ADC nos dois extremos do seu pot.
// Se o seu nao chega em 0..4095, ajuste aqui (use DEBUG_POT 1 para medir).
#define POT_ADC_MINIMO 0
#define POT_ADC_MAXIMO 4095
#define POT_ESCALA 4096   // faixa normalizada: 0..4095
#define POT_HISTERESE 150 // margem para nao oscilar entre duas opcoes
#define LEITURA_POT_MS 100
#define AMOSTRAS_ADC 32
#define DEBUG_POT 0 // 1 = mostra o valor bruto do ADC uma vez por segundo

#endif
