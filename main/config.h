#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"
#include "hal/adc_types.h"

// ---------- pinos (pcb do projeto lavadora: esp32 wroom-32, 38 pinos) --------
#define LED_VERDE GPIO_NUM_17
#define LED_AMARELO GPIO_NUM_18
#define LED_VERMELHO GPIO_NUM_16
#define BTN1_PIN GPIO_NUM_32
#define BTN2_PIN GPIO_NUM_33
#define BUZZER_PIN GPIO_NUM_25  // saida pwm para base do bc337 via resistor
#define POT_CANAL ADC_CHANNEL_6 // gpio34 = adc1 canal 6 (entrada analogica)

// ---------- estoque ----------
#define AGUA_MAX_ML 1000
#define CAPSULAS_MAX 5
#define AGUA_INICIAL_ML 600
#define CAPSULAS_INICIAIS 3
#define AGUA_PASSO_ML 200 // reposicao de agua de 200 em 200

// ---------- receitas ----------
// volumes dos botoes de uma nespresso: espresso 40 ml e lungo 110 ml.
#define CAFE_CURTO_ML 40
#define CAFE_LONGO_ML 110

// ---------- extracao ----------
// antes da primeira gota a bomba pressuriza a capsula (estimativa de 5 s).
// com 2 ml/s o curto leva 25 s (real: 25 a 30 s) e o longo 60 s (real: 30 a
// 60 s, as fontes variam bastante).
#define PRE_INFUSAO_MS 5000
#define VAZAO_ML_SEGUNDO 2
// grava na flash a cada 10 ml: a cada 5 s, para nao gastar a nvs a toa.
#define PASSO_EXTRACAO_ML 10
#define PASSO_EXTRACAO_MS (PASSO_EXTRACAO_ML * 1000 / VAZAO_ML_SEGUNDO)

// ---------- aquecimento ----------
// essenza mini: cerca de 25 s para aquecer. nespresso informa preparo a 86 c.
// o que esquenta e o bloco (thermoblock), e a agua so passa por ele na hora do
// cafe. por isso o tempo e o mesmo com o tanque cheio ou quase vazio.
#define TEMP_AMBIENTE_C 25
#define TEMP_ALVO_C 86
#define AQUECIMENTO_MS 25000 // tempo para ir do ambiente ate o alvo
// desligada ela esfria aos poucos (estimativa, nao e dado de fabricante).
#define RESFRIAMENTO_C_MINUTO 6

// ---------- botoes e inatividade ----------
#define DEBOUNCE_MS 40
#define SEGURAR_BTN1_MS 3000
#define TIMEOUT_MS 60000

// ---------- potenciometro ----------
// leitura bruta do adc nos dois extremos do pot. se o seu nao chega em
// 0..4095, ajuste aqui (ligue o debug do pot, logo abaixo, para medir).
#define POT_ADC_MINIMO 0
#define POT_ADC_MAXIMO 4095
#define POT_ESCALA 4096   // faixa normalizada: 0..4095
#define POT_HISTERESE 150 // margem para nao oscilar entre duas opcoes
#define LEITURA_POT_MS 100
#define AMOSTRAS_ADC 32
#define AVISO_ADC_MS 5000 // intervalo minimo entre avisos de falha do adc
#define DEBUG_POT 0       // 1 = mostra o valor bruto do adc uma vez por segundo

// ---------- tempos do loop e dos leds ----------
#define PERIODO_LOOP_MS 20
#define PISCA_RAPIDO_MS 300 // preparando ou em manutencao
#define PISCA_LENTO_MS 800  // aquecendo: pisca devagar ate ficar pronta

// ---------- conferencias em tempo de compilacao ----------
// valor errado aqui vira erro ao compilar, e nao bug ou divisao por 0 na placa.
_Static_assert(AGUA_INICIAL_ML <= AGUA_MAX_ML, "agua inicial acima do tanque");
_Static_assert(CAPSULAS_INICIAIS <= CAPSULAS_MAX, "capsulas iniciais demais");
_Static_assert(CAFE_CURTO_ML % PASSO_EXTRACAO_ML == 0, "curto fora do passo");
_Static_assert(CAFE_LONGO_ML % PASSO_EXTRACAO_ML == 0, "longo fora do passo");
_Static_assert(VAZAO_ML_SEGUNDO > 0, "vazao precisa ser positiva");
_Static_assert(TEMP_ALVO_C > TEMP_AMBIENTE_C, "alvo abaixo do ambiente");
_Static_assert(AQUECIMENTO_MS > 0, "aquecimento precisa ser positivo");
_Static_assert(AMOSTRAS_ADC > 2, "a media descarta 2 amostras");
_Static_assert(POT_ADC_MAXIMO > POT_ADC_MINIMO, "faixa do pot invertida");

#endif
