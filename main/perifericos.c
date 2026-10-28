#include <limits.h>

#include "config.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "perifericos.h"

// buzzer: PWM (LEDC) com 50% de duty e frequencia = nota.
#define PWM_MODO LEDC_LOW_SPEED_MODE
#define PWM_TIMER LEDC_TIMER_0
#define PWM_CANAL LEDC_CHANNEL_0
#define PWM_RESOLUCAO LEDC_TIMER_10_BIT
#define PWM_DUTY 512 // 50% em 10 bits

typedef struct {
  int frequencia; // Hz (0 = pausa)
  int tempo_ms;
} Nota;

static const Nota notas_ligar[] = {
    {523, 120}, {0, 45}, {659, 120}, {0, 45}, {784, 200}};
static const Nota notas_pronto[] = {{784, 140}, {0, 70}, {988, 240}};
static const Nota notas_erro[] = {
    {330, 100}, {0, 70}, {330, 100}, {0, 70}, {330, 160}};
static const Nota notas_cancelar[] = {{294, 230}};
static const Nota notas_confirmar[] = {{880, 100}};

static adc_oneshot_unit_handle_t adc;

// melodia em andamento (tocada nota a nota por som_atualizar)
static const Nota *melodia = NULL;
static int total_notas = 0;
static int nota_atual = 0;
static int64_t fim_nota_ms = 0;

void perifericos_iniciar(void) {
  gpio_config_t saidas = {.pin_bit_mask = (1ULL << LED_VERDE) |
                                          (1ULL << LED_AMARELO) |
                                          (1ULL << LED_VERMELHO),
                          .mode = GPIO_MODE_OUTPUT};
  ESP_ERROR_CHECK(gpio_config(&saidas));

  // Botoes da PCB ligados a 3V3: pull-down interno; pressionado = nivel alto.
  gpio_config_t entradas = {.pin_bit_mask =
                                (1ULL << BTN1_PIN) | (1ULL << BTN2_PIN),
                            .mode = GPIO_MODE_INPUT,
                            .pull_up_en = GPIO_PULLUP_DISABLE,
                            .pull_down_en = GPIO_PULLDOWN_ENABLE};
  ESP_ERROR_CHECK(gpio_config(&entradas));

  ledc_timer_config_t timer = {.speed_mode = PWM_MODO,
                               .timer_num = PWM_TIMER,
                               .duty_resolution = PWM_RESOLUCAO,
                               .freq_hz = 1000,
                               .clk_cfg = LEDC_AUTO_CLK};
  ESP_ERROR_CHECK(ledc_timer_config(&timer));

  ledc_channel_config_t canal = {.gpio_num = BUZZER_PIN,
                                 .speed_mode = PWM_MODO,
                                 .channel = PWM_CANAL,
                                 .timer_sel = PWM_TIMER,
                                 .duty = 0};
  ESP_ERROR_CHECK(ledc_channel_config(&canal));

  adc_oneshot_unit_init_cfg_t adc_cfg = {.unit_id = ADC_UNIT_1};
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_cfg, &adc));
  adc_oneshot_chan_cfg_t pot_cfg = {.atten = ADC_ATTEN_DB_12,
                                    .bitwidth = ADC_BITWIDTH_12};
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, POT_CANAL, &pot_cfg));

  leds_mostrar(false, false, false);
}

// ---------- botoes ----------

void botao_iniciar(Botao *botao, gpio_num_t pino, int64_t longo_ms,
                   int64_t agora_ms) {
  bool apertado = gpio_get_level(pino) == 1;
  *botao =
      (Botao){.pino = pino,
              .longo_ms = longo_ms,
              .leitura_bruta = apertado,
              .pressionado = apertado,
              // botao ja apertado no boot nao gera clique nem pressao longa
              .longo_enviado = apertado,
              .ultima_mudanca_ms = agora_ms,
              .inicio_ms = agora_ms};
}

void botao_atualizar(Botao *botao, int64_t agora_ms) {
  bool leitura = gpio_get_level(botao->pino) == 1;
  botao->clique = false;
  botao->longo = false;

  if (leitura != botao->leitura_bruta) {
    botao->leitura_bruta = leitura;
    botao->ultima_mudanca_ms = agora_ms;
  }

  // so aceita a mudanca depois de DEBOUNCE_MS estavel.
  if (leitura != botao->pressionado &&
      agora_ms - botao->ultima_mudanca_ms >= DEBOUNCE_MS) {
    botao->pressionado = leitura;
    if (leitura) {
      botao->inicio_ms = agora_ms;
      botao->longo_enviado = false;
    } else if (!botao->longo_enviado) {
      botao->clique = true;
    }
  }

  if (botao->pressionado && botao->longo_ms > 0 && !botao->longo_enviado &&
      agora_ms - botao->inicio_ms >= botao->longo_ms) {
    botao->longo_enviado = true;
    botao->longo = true;
  }
}

// ---------- potenciometro ----------

int potenciometro_ler(int *bruto) {
  int soma = 0, menor = INT_MAX, maior = INT_MIN;

  // o ADC do ESP32 tem ruido: media sem a menor e a maior amostra.
  for (int i = 0; i < AMOSTRAS_ADC; i++) {
    int leitura;
    if (adc_oneshot_read(adc, POT_CANAL, &leitura) != ESP_OK)
      return -1;
    soma += leitura;
    if (leitura < menor)
      menor = leitura;
    if (leitura > maior)
      maior = leitura;
  }

  int media = (soma - menor - maior) / (AMOSTRAS_ADC - 2);
  if (bruto)
    *bruto = media;

  // converte a faixa calibrada do pot para 0..POT_ESCALA-1.
  if (media <= POT_ADC_MINIMO)
    return 0;
  if (media >= POT_ADC_MAXIMO)
    return POT_ESCALA - 1;
  return (media - POT_ADC_MINIMO) * (POT_ESCALA - 1) /
         (POT_ADC_MAXIMO - POT_ADC_MINIMO);
}

// ---------- leds ----------

// PCB adaptada: LEDs ativos em LOW (0 liga, 1 desliga).
void leds_mostrar(bool verde, bool amarelo, bool vermelho) {
  gpio_set_level(LED_VERDE, !verde);
  gpio_set_level(LED_AMARELO, !amarelo);
  gpio_set_level(LED_VERMELHO, !vermelho);
}

// ---------- buzzer ----------

static void pwm_silencio(void) {
  ledc_set_duty(PWM_MODO, PWM_CANAL, 0);
  ledc_update_duty(PWM_MODO, PWM_CANAL);
}

void som_parar(void) {
  melodia = NULL;
  pwm_silencio();
}

#define TOCAR(vetor)                                                           \
  melodia = vetor;                                                             \
  total_notas = sizeof(vetor) / sizeof(vetor[0])

void som_tocar(Som som) {
  switch (som) {
  case SOM_LIGAR:
    TOCAR(notas_ligar);
    break;
  case SOM_PRONTO:
    TOCAR(notas_pronto);
    break;
  case SOM_ERRO:
    TOCAR(notas_erro);
    break;
  case SOM_CANCELAR:
    TOCAR(notas_cancelar);
    break;
  case SOM_CONFIRMAR:
    TOCAR(notas_confirmar);
    break;
  }
  nota_atual = 0;
  fim_nota_ms = 0;
}

// chamada a cada ciclo: troca de nota quando a atual terminou (sem delay).
void som_atualizar(int64_t agora_ms) {
  if (!melodia || agora_ms < fim_nota_ms)
    return;
  if (nota_atual >= total_notas) {
    som_parar();
    return;
  }

  Nota nota = melodia[nota_atual++];
  if (nota.frequencia == 0) {
    pwm_silencio();
  } else {
    ledc_set_freq(PWM_MODO, PWM_TIMER, nota.frequencia);
    ledc_set_duty(PWM_MODO, PWM_CANAL, PWM_DUTY);
    ledc_update_duty(PWM_MODO, PWM_CANAL);
  }
  fim_nota_ms = agora_ms + nota.tempo_ms;
}
