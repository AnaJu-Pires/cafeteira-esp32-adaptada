#include <stdint.h>
#include <stdio.h>

#include "config.h"
#include "estoque.h"
#include "nvs.h"
#include "nvs_flash.h"

#define NVS_ESPACO "cafeteira"
#define NVS_CHAVE "dados"
#define VERSAO_DADOS 1

// Um registro so evita salvar agua e capsulas separadamente.
typedef struct {
  uint32_t versao;
  int32_t agua_ml;
  int32_t capsulas;
  uint32_t preparando;
} Registro;

static bool dados_validos(const Registro *registro) {
  return registro->versao == VERSAO_DADOS && registro->agua_ml >= 0 &&
         registro->agua_ml <= AGUA_MAX_ML && registro->capsulas >= 0 &&
         registro->capsulas <= CAPSULAS_MAX && registro->preparando <= 1;
}

static void estoque_inicial(Estoque *dados) {
  dados->agua_ml = AGUA_INICIAL_ML;
  dados->capsulas = CAPSULAS_INICIAIS;
  dados->preparando = false;
}

esp_err_t estoque_salvar(const Estoque *dados) {
  Registro registro = {.versao = VERSAO_DADOS,
                       .agua_ml = dados->agua_ml,
                       .capsulas = dados->capsulas,
                       .preparando = dados->preparando ? 1 : 0};

  nvs_handle_t memoria;
  esp_err_t erro = nvs_open(NVS_ESPACO, NVS_READWRITE, &memoria);
  if (erro != ESP_OK)
    return erro;

  erro = nvs_set_blob(memoria, NVS_CHAVE, &registro, sizeof(registro));
  if (erro == ESP_OK)
    erro = nvs_commit(memoria);
  nvs_close(memoria);
  return erro;
}

esp_err_t estoque_iniciar(Estoque *dados) {
  esp_err_t erro = nvs_flash_init();
  if (erro == ESP_ERR_NVS_NO_FREE_PAGES ||
      erro == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // flash de outro projeto ou formato antigo: a NVS ja nao e legivel.
    printf("[NVS] Particao incompativel. Formatando.\n");
    erro = nvs_flash_erase();
    if (erro == ESP_OK)
      erro = nvs_flash_init();
  }
  if (erro != ESP_OK)
    return erro;

  nvs_handle_t memoria;
  erro = nvs_open(NVS_ESPACO, NVS_READWRITE, &memoria);
  if (erro != ESP_OK)
    return erro;

  Registro registro = {0};
  size_t tamanho = sizeof(registro);
  erro = nvs_get_blob(memoria, NVS_CHAVE, &registro, &tamanho);
  nvs_close(memoria);

  if (erro == ESP_ERR_NVS_NOT_FOUND) {
    estoque_inicial(dados);
    printf("[NVS] Primeiro uso: criando estoque inicial.\n");
    return estoque_salvar(dados);
  }
  if (erro != ESP_OK)
    return erro;

  if (tamanho != sizeof(registro) || !dados_validos(&registro)) {
    estoque_inicial(dados);
    printf("[NVS] Dados invalidos. Voltando ao estoque inicial.\n");
    return estoque_salvar(dados);
  }

  dados->agua_ml = registro.agua_ml;
  dados->capsulas = registro.capsulas;
  dados->preparando = registro.preparando == 1;
  printf("[NVS] Estoque recuperado da memoria.\n");
  return ESP_OK;
}
