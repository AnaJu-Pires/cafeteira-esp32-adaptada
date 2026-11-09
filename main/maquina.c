#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "aquecedor.h"
#include "config.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "estoque.h"
#include "maquina.h"
#include "perifericos.h"

typedef enum {
  DESLIGADA,
  AQUECENDO,
  PRONTA,
  PREPARANDO,
  ERRO,
  MANUTENCAO
} Estado;
typedef enum { CURTO, LONGO } Cafe;
typedef enum { AGUA, CAPSULAS } Reposicao;

// ---------- variaveis da maquina ----------
static Estado estado = DESLIGADA;
static Cafe cafe = CURTO;
static Reposicao reposicao = AGUA;
static Estoque estoque;
static bool falha_memoria = false;
static bool cafe_na_fila = false; // pediu cafe enquanto a agua esquentava

static Botao botao1, botao2;
static int pot = 0;       // 0..4095, ja calibrado
static int zona_repo = 0; // opcao de quantidade escolhida na manutencao
static int volume_pedido = 0;
static int volume_extraido = 0;     // o que ja foi gravado na flash
static int64_t inicio_fluxo_ms = 0; // quando sai a primeira gota
static int64_t proximo_passo_ms = 0;
static int64_t ultima_atividade_ms = 0;
static int64_t ultima_leitura_ms = 0;
static int64_t ultimo_erro_adc_ms = -AVISO_ADC_MS;

// ---------- utilidades ----------

static int64_t agora_ms(void) { return esp_timer_get_time() / 1000; }

static int volume_cafe(void) {
  return cafe == CURTO ? CAFE_CURTO_ML : CAFE_LONGO_ML;
}

static const char *nome_cafe(void) { return cafe == CURTO ? "CURTO" : "LONGO"; }

// tempo total do cafe: pressuriza a capsula e depois extrai com vazao fixa.
static int tempo_preparo_s(void) {
  return PRE_INFUSAO_MS / 1000 + volume_cafe() / VAZAO_ML_SEGUNDO;
}

static bool pode_preparar(void) {
  return !falha_memoria && estoque.capsulas >= 1 &&
         estoque.agua_ml >= volume_cafe();
}

static void mostrar_estoque(void) {
  printf("[ESTOQUE] Agua: %d/%d mL | Capsulas: %d/%d\n", estoque.agua_ml,
         AGUA_MAX_ML, estoque.capsulas, CAPSULAS_MAX);
}

static void mostrar_receita(void) {
  printf("[CAFE] %s selecionado: %d mL, 1 capsula, %d s\n", nome_cafe(),
         volume_cafe(), tempo_preparo_s());
}

// so uma linha com a estimativa: o resto da espera e o led verde piscando.
static void mostrar_aquecimento(int64_t agora) {
  // arredonda para cima para nao mostrar "0 s" ainda aquecendo.
  printf("[AQUECENDO] %d mL de agua, de %d ate %d C | estimativa: %d s\n",
         estoque.agua_ml, aquecedor_temperatura(agora), TEMP_ALVO_C,
         (int)((aquecedor_restante_ms(agora) + 999) / 1000));
}

static void entrar_erro(void) {
  estado = ERRO;
  som_tocar(SOM_ERRO);
}

// ---------- estoque na nvs ----------

static void erro_ao_salvar(esp_err_t erro) {
  // sem nvs nao da para garantir o estoque: trava preparo e reposicao.
  falha_memoria = true;
  printf("[ERRO] Falha ao salvar na NVS: %s\n", esp_err_to_name(erro));
  printf("[ERRO] Preparo e reposicao bloqueados. Reinicie a placa.\n");
  entrar_erro();
}

// so troca o estoque da ram se o novo valor foi gravado na flash.
static bool confirmar_estoque(Estoque novo) {
  esp_err_t erro = estoque_salvar(&novo);
  if (erro != ESP_OK) {
    erro_ao_salvar(erro);
    return false;
  }
  estoque = novo;
  return true;
}

// ---------- potenciometro: escolha de opcoes ----------

// escolhe uma de n opcoes pela posicao do pot. "atual" (ou -1 se nao ha
// historico) evita trocar de opcao por ruido perto do limite entre elas.
static int pot_zona(int n, int atual) {
  int zona = atual;
  if (zona < 0 || zona >= n) {
    zona = pot * n / POT_ESCALA;
    if (zona >= n)
      zona = n - 1;
  }
  while (zona < n - 1 && pot > (zona + 1) * POT_ESCALA / n + POT_HISTERESE)
    zona++;
  while (zona > 0 && pot < zona * POT_ESCALA / n - POT_HISTERESE)
    zona--;
  return zona;
}

static void sincronizar_cafe(void) {
  cafe = pot_zona(2, -1) == 0 ? CURTO : LONGO;
}

// ---------- manutencao (reposicao) ----------

static int espaco_livre(void) {
  return reposicao == AGUA ? AGUA_MAX_ML - estoque.agua_ml
                           : CAPSULAS_MAX - estoque.capsulas;
}

// agua: passos de 200 ml e, no fim, o valor exato que completa o tanque.
static int opcoes_reposicao(void) {
  int livre = espaco_livre();
  return reposicao == AGUA ? (livre + AGUA_PASSO_ML - 1) / AGUA_PASSO_ML
                           : livre;
}

static int valor_opcao(int zona) {
  if (reposicao == CAPSULAS)
    return zona + 1;
  int valor = (zona + 1) * AGUA_PASSO_ML;
  int livre = espaco_livre();
  return valor > livre ? livre : valor;
}

static void escolher_reposicao(bool com_historico) {
  int n = opcoes_reposicao();
  zona_repo = n == 0 ? 0 : pot_zona(n, com_historico ? zona_repo : -1);
}

static void mostrar_reposicao(void) {
  const char *item = reposicao == AGUA ? "Agua" : "Capsulas";
  int n = opcoes_reposicao();
  if (n == 0) {
    printf("[MANUTENCAO] %s: cheio, nada a repor. BTN1 troca de item.\n", item);
    return;
  }
  printf("[MANUTENCAO] %s (POT %d%%):", item, pot * 100 / (POT_ESCALA - 1));
  for (int i = 0; i < n; i++)
    printf(i == zona_repo ? " [+%d]" : " +%d", valor_opcao(i));
  printf(" %s | BTN2 confirma\n", reposicao == AGUA ? "mL" : "capsula(s)");
}

static void entrar_manutencao(int64_t agora) {
  if (falha_memoria) {
    printf("[AVISO] Manutencao indisponivel: NVS com falha.\n");
    som_tocar(SOM_ERRO);
    return;
  }
  // ao sair ela poderia fazer um cafe sozinha sem ninguem estar esperando.
  if (cafe_na_fila) {
    cafe_na_fila = false;
    printf("[FILA] Cafe retirado da fila.\n");
  }
  estado = MANUTENCAO;
  reposicao = AGUA;
  escolher_reposicao(false);
  ultima_atividade_ms = agora;
  printf("\n========== MANUTENCAO ==========\n");
  printf("BTN1 curto: troca agua/capsulas | POT: quantidade\n");
  printf("BTN2 curto: confirma | BTN1 3 s: sai\n");
  mostrar_estoque();
  mostrar_reposicao();
}

static void sair_manutencao(int64_t agora) {
  ultima_atividade_ms = agora;
  sincronizar_cafe();
  // o aquecedor nao para na manutencao: pode ter entrado ainda frio.
  if (aquecedor_restante_ms(agora) > 0) {
    estado = AQUECENDO;
    printf("[MANUTENCAO] Encerrada. Agua ainda aquecendo.\n");
  } else {
    estado = PRONTA;
    printf("[MANUTENCAO] Encerrada. Maquina pronta.\n");
  }
  mostrar_estoque();
  mostrar_receita();
  if (estado == AQUECENDO)
    mostrar_aquecimento(agora);
}

static void reabastecer(int64_t agora) {
  ultima_atividade_ms = agora;
  if (opcoes_reposicao() == 0) {
    printf("[RECUSADO] %s ja esta cheio. Troque de item ou saia.\n",
           reposicao == AGUA ? "O reservatorio" : "O compartimento");
    som_tocar(SOM_ERRO);
    return;
  }

  int quantidade = valor_opcao(zona_repo);
  Estoque novo = estoque;
  if (reposicao == AGUA)
    novo.agua_ml += quantidade;
  else
    novo.capsulas += quantidade;
  if (!confirmar_estoque(novo))
    return;

  // agua nova muda o tempo que ainda falta, se estiver aquecendo.
  if (reposicao == AGUA)
    aquecedor_ajustar_agua(agora, estoque.agua_ml);

  printf("[OK] Reposto +%d %s.\n", quantidade,
         reposicao == AGUA ? "mL de agua" : "capsula(s)");
  mostrar_estoque();
  escolher_reposicao(false);
  mostrar_reposicao();
  som_tocar(SOM_CONFIRMAR);
}

// ---------- energia ----------

static void ligar(int64_t agora) {
  if (falha_memoria) {
    printf("[RECUSADO] NVS com falha. Nao e seguro ligar.\n");
    som_tocar(SOM_ERRO);
    return;
  }
  sincronizar_cafe();
  aquecedor_ligar(agora, estoque.agua_ml);
  ultima_atividade_ms = agora;
  printf("\n=========== MAQUINA LIGADA ===========\n");
  mostrar_estoque();
  mostrar_receita();
  printf("[INFO] POT escolhe curto/longo | BTN2 prepara | BTN1 desliga\n");
  printf("[INFO] BTN1 por 3 s abre a manutencao.\n");
  som_tocar(SOM_LIGAR);

  // desligou e religou rapido: a agua ainda esta quente e nao precisa esperar.
  if (aquecedor_restante_ms(agora) == 0) {
    estado = PRONTA;
    printf("[PRONTA] Agua ja esta quente. BTN2 prepara.\n");
  } else {
    estado = AQUECENDO;
    mostrar_aquecimento(agora);
  }
}

static void desligar(int64_t agora, bool automatico) {
  estado = DESLIGADA;
  cafe_na_fila = false;
  aquecedor_desligar(agora);
  som_parar();
  if (automatico)
    printf("[ENERGIA] Desligada por inatividade (%d s).\n", TIMEOUT_MS / 1000);
  else
    printf("[ENERGIA] Maquina desligada.\n");
  printf("[INFO] Aperte BTN1 para ligar.\n");
}

// ---------- preparo do cafe ----------

// em erro por falta de recurso: volta a pronta quando houver o suficiente.
static void conferir_recursos(void) {
  if (estado == ERRO && pode_preparar()) {
    estado = PRONTA;
    printf("[OK] Recursos suficientes para o cafe %s. BTN2 prepara.\n",
           nome_cafe());
  }
}

static void mostrar_falta_recursos(void) {
  printf("\n[RECUSADO] Nao da para fazer cafe %s:\n", nome_cafe());
  if (falha_memoria)
    printf(" - NVS com falha\n");
  if (estoque.capsulas < 1)
    printf(" - sem capsulas\n");
  if (estoque.agua_ml < volume_cafe())
    printf(" - agua insuficiente: %d mL, precisa de %d mL\n", estoque.agua_ml,
           volume_cafe());
  if (!falha_memoria)
    printf("[INFO] BTN1 por 3 s abre a manutencao para repor.\n");
}

static void iniciar_preparo(int64_t agora) {
  if (!pode_preparar()) {
    mostrar_falta_recursos();
    entrar_erro();
    return;
  }

  // a capsula e descontada ao iniciar e a agua durante a extracao.
  Estoque novo = estoque;
  novo.capsulas--;
  novo.preparando = true;
  if (!confirmar_estoque(novo))
    return;

  estado = PREPARANDO;
  volume_pedido = volume_cafe();
  volume_extraido = 0;
  inicio_fluxo_ms = agora + PRE_INFUSAO_MS;
  proximo_passo_ms = inicio_fluxo_ms + PASSO_EXTRACAO_MS;
  printf("\n========== PREPARANDO CAFE %s ==========\n", nome_cafe());
  printf("[PREPARO] %d mL em %d s, capsula usada. BTN1 cancela.\n",
         volume_pedido, tempo_preparo_s());
  printf("[PREPARO] Pressurizando a capsula (%d s)...\n",
         PRE_INFUSAO_MS / 1000);
}

// quanto ja saiu de verdade, contando o pedaco que ainda nao foi gravado.
static int volume_ate_agora(int64_t agora) {
  int64_t fluxo_ms = agora - inicio_fluxo_ms;
  if (fluxo_ms <= 0)
    return 0; // ainda pressurizando: nenhuma gota saiu
  int volume = (int)(fluxo_ms * VAZAO_ML_SEGUNDO / 1000);
  return volume < volume_pedido ? volume : volume_pedido;
}

static void cancelar_preparo(int64_t agora) {
  // desconta tambem a agua que passou desde a ultima gravacao.
  int volume_final = volume_ate_agora(agora);
  Estoque novo = estoque;
  novo.agua_ml -= volume_final - volume_extraido;
  novo.preparando = false;
  if (!confirmar_estoque(novo))
    return;
  volume_extraido = volume_final;
  estado = PRONTA;
  ultima_atividade_ms = agora;
  printf("[CANCELADO] Cafe interrompido em %d/%d mL.\n", volume_extraido,
         volume_pedido);
  printf("[INFO] Capsula e agua ja usadas nao voltam.\n");
  mostrar_estoque();
  som_tocar(SOM_CANCELAR);
}

static void atualizar_preparo(int64_t agora) {
  // a cada passo: desconta a agua e salva, para sobreviver a reset. num reset
  // no meio do passo, perde no maximo 10 ml na conta da agua.
  while (estado == PREPARANDO && agora >= proximo_passo_ms) {
    int novo_volume = volume_extraido + PASSO_EXTRACAO_ML;
    bool terminou = novo_volume >= volume_pedido;
    Estoque novo = estoque;
    novo.agua_ml -= PASSO_EXTRACAO_ML;
    novo.preparando = !terminou;
    if (!confirmar_estoque(novo))
      return;

    volume_extraido = novo_volume;
    proximo_passo_ms += PASSO_EXTRACAO_MS;
    printf("[EXTRACAO] %d/%d mL | agua restante: %d mL\n", volume_extraido,
           volume_pedido, estoque.agua_ml);

    if (terminou) {
      estado = PRONTA;
      ultima_atividade_ms = agora;
      printf("[PRONTO] Cafe %s pronto!\n", nome_cafe());
      mostrar_estoque();
      som_tocar(SOM_PRONTO);
    }
  }
}

// ---------- aquecimento e fila ----------

// btn2 aquecendo: poe o cafe na fila ou, se ja estava, tira.
static void alternar_fila(void) {
  if (cafe_na_fila) {
    cafe_na_fila = false;
    printf("[FILA] Cafe retirado da fila.\n");
    som_tocar(SOM_CANCELAR);
    return;
  }
  // sem recurso nao entra na fila, mas continua aquecendo em vez de ir para
  // erro: de erro ela voltaria a pronta ainda com a agua fria.
  if (!pode_preparar()) {
    mostrar_falta_recursos();
    som_tocar(SOM_ERRO);
    return;
  }
  cafe_na_fila = true;
  printf("[FILA] Cafe %s na fila: sai quando a agua aquecer. BTN2 retira.\n",
         nome_cafe());
  som_tocar(SOM_CONFIRMAR);
}

static void atualizar_aquecimento(int64_t agora) {
  if (estado != AQUECENDO || aquecedor_restante_ms(agora) > 0)
    return;

  estado = PRONTA;
  ultima_atividade_ms = agora; // os 60 s de inatividade so contam daqui
  printf("[PRONTA] Agua a %d C. BTN2 prepara.\n", TEMP_ALVO_C);
  if (cafe_na_fila) {
    cafe_na_fila = false;
    iniciar_preparo(agora);
  } else {
    som_tocar(SOM_CONFIRMAR);
  }
}

// ---------- botoes ----------

static void btn1_clique(int64_t agora) {
  switch (estado) {
  case DESLIGADA:
    ligar(agora);
    break;
  case AQUECENDO:
  case PRONTA:
  case ERRO:
    desligar(agora, false);
    break;
  case PREPARANDO:
    cancelar_preparo(agora);
    break;
  case MANUTENCAO:
    reposicao = reposicao == AGUA ? CAPSULAS : AGUA;
    escolher_reposicao(false);
    mostrar_reposicao();
    break;
  }
}

static void btn1_longo(int64_t agora) {
  switch (estado) {
  case AQUECENDO: // repor agua nao depende da agua estar quente
  case PRONTA:
  case ERRO:
    entrar_manutencao(agora);
    break;
  case MANUTENCAO:
    sair_manutencao(agora);
    break;
  case PREPARANDO:
    printf(
        "[RECUSADO] Sem manutencao durante o preparo. BTN1 curto cancela.\n");
    break;
  case DESLIGADA:
    printf("[AVISO] Maquina desligada. Para ligar, aperte e solte BTN1 "
           "rapido.\n");
    break;
  }
}

static void btn2_clique(int64_t agora) {
  switch (estado) {
  case DESLIGADA:
    printf("[AVISO] Maquina desligada. Aperte BTN1 para ligar.\n");
    break;
  case AQUECENDO:
    // agua fria nao extrai direito: o cafe espera na fila ate esquentar.
    alternar_fila();
    break;
  case PRONTA:
  case ERRO:
    iniciar_preparo(agora);
    break;
  case PREPARANDO:
    break;
  case MANUTENCAO:
    reabastecer(agora);
    break;
  }
}

static void atualizar_botoes(int64_t agora) {
  botao_atualizar(&botao1, agora);
  botao_atualizar(&botao2, agora);

  // qualquer acao do usuario conta como atividade (reinicia os 60 s).
  if (botao1.clique || botao1.longo || botao2.clique)
    ultima_atividade_ms = agora;

  if (botao1.clique)
    btn1_clique(agora);
  if (botao1.longo)
    btn1_longo(agora);
  if (botao2.clique)
    btn2_clique(agora);
}

// ---------- potenciometro ----------

static void atualizar_potenciometro(int64_t agora) {
  if (agora - ultima_leitura_ms < LEITURA_POT_MS)
    return;
  ultima_leitura_ms = agora;

  int bruto = 0;
  int leitura = potenciometro_ler(&bruto);
  if (leitura < 0) {
    if (agora - ultimo_erro_adc_ms >= AVISO_ADC_MS) {
      printf("[ADC] Falha ao ler o potenciometro.\n");
      ultimo_erro_adc_ms = agora;
    }
    return;
  }
  pot = leitura;

#if DEBUG_POT
  static int64_t ultimo_debug_ms = 0;
  if (agora - ultimo_debug_ms >= 1000) {
    printf("[DEBUG POT] adc bruto: %d | calibrado: %d/%d\n", bruto, pot,
           POT_ESCALA - 1);
    ultimo_debug_ms = agora;
  }
#else
  (void)bruto;
#endif

  if (estado == MANUTENCAO) {
    int n = opcoes_reposicao();
    int zona = n == 0 ? 0 : pot_zona(n, zona_repo);
    if (zona != zona_repo) {
      zona_repo = zona;
      ultima_atividade_ms = agora;
      mostrar_reposicao();
    }
  } else if ((estado == AQUECENDO && !cafe_na_fila) || estado == PRONTA ||
             estado == ERRO) {
    // com cafe na fila o pot nao muda nada: sai o que foi pedido.
    Cafe novo = pot_zona(2, cafe) == 0 ? CURTO : LONGO;
    if (novo != cafe) {
      cafe = novo;
      ultima_atividade_ms = agora;
      mostrar_receita();
      if (!pode_preparar() && !falha_memoria)
        printf("[AVISO] Recursos insuficientes para este cafe.\n");
    }
    conferir_recursos();
  }
}

// ---------- leds ----------

static void atualizar_leds(int64_t agora) {
  bool pisca = (agora / PISCA_RAPIDO_MS) % 2 == 0;
  bool verde = false, amarelo = false;
  switch (estado) {
  case AQUECENDO:
    // igual a maquina real: verde pisca devagar e fica fixo quando esta pronta.
    verde = (agora / PISCA_LENTO_MS) % 2 == 0;
    amarelo = cafe_na_fila; // amarelo fixo: tem cafe esperando na fila
    break;
  case PRONTA:
    verde = true;
    break;
  case PREPARANDO:
    amarelo = pisca;
    break;
  case MANUTENCAO:
    if (reposicao == AGUA)
      verde = pisca;
    else
      amarelo = pisca;
    break;
  case ERRO:
  case DESLIGADA:
    break;
  }
  leds_mostrar(verde, amarelo, estado == ERRO || falha_memoria);
}

// ---------- interface publica ----------

void maquina_iniciar(void) {
  // depois de um reset a maquina sempre volta desligada.
  perifericos_iniciar();

  esp_err_t erro = estoque_iniciar(&estoque);
  if (erro != ESP_OK) {
    falha_memoria = true;
    estoque = (Estoque){0};
    printf("[ERRO] Nao foi possivel abrir a NVS: %s\n", esp_err_to_name(erro));
    printf("[ERRO] Preparo e reposicao bloqueados. Reinicie a placa.\n");
    som_tocar(SOM_ERRO);
  } else if (estoque.preparando) {
    // cafe interrompido por reset: nao retoma, so limpa o estado de preparo.
    printf("\n[RECUPERACAO] Reinicio durante o preparo. Cafe descartado.\n");
    printf("[RECUPERACAO] Capsula e agua ja usadas ficam descontadas.\n");
    Estoque recuperado = estoque;
    recuperado.preparando = false;
    confirmar_estoque(recuperado);
  }

  int64_t agora = agora_ms();
  botao_iniciar(&botao1, BTN1_PIN, SEGURAR_BTN1_MS, agora);
  botao_iniciar(&botao2, BTN2_PIN, 0, agora);
  ultima_atividade_ms = agora;
  ultima_leitura_ms = agora;

  int leitura = potenciometro_ler(NULL);
  pot = leitura < 0 ? 0 : leitura;

  printf("\n[SISTEMA] Cafeteira iniciada.\n");
  mostrar_estoque();
  if (!falha_memoria)
    printf("[INFO] Aperte e solte BTN1 para ligar.\n");
}

void maquina_atualizar(void) {
  int64_t agora = agora_ms();
  atualizar_botoes(agora);
  atualizar_potenciometro(agora);
  atualizar_aquecimento(agora);
  atualizar_preparo(agora);

  // aquecendo ou preparando a maquina esta trabalhando, nao conta como ociosa.
  bool ociosa = estado == PRONTA || estado == ERRO || estado == MANUTENCAO;
  if (ociosa && agora - ultima_atividade_ms >= TIMEOUT_MS)
    desligar(agora, true);

  som_atualizar(agora);
  atualizar_leds(agora);
}
