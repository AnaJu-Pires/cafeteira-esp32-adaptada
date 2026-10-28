#ifndef ESTOQUE_H
#define ESTOQUE_H

#include <stdbool.h>

#include "esp_err.h"

typedef struct {
  int agua_ml;
  int capsulas;
  bool preparando; // fica salvo para saber se houve interrupcao
} Estoque;

// Le da memoria flash. Na primeira execucao, salva o estoque inicial.
esp_err_t estoque_iniciar(Estoque *dados);

// Salva agua, capsulas e preparo juntos, na memoria NVS.
esp_err_t estoque_salvar(const Estoque *dados);

#endif
