#ifndef ESTOQUE_H
#define ESTOQUE_H

#include <stdbool.h>

#include "esp_err.h"

typedef struct {
  int agua_ml;
  int capsulas;
  bool preparando; // fica salvo para saber se houve interrupcao
} Estoque;

// le da memoria flash. na primeira execucao, salva o estoque inicial.
esp_err_t estoque_iniciar(Estoque *dados);

// salva agua, capsulas e preparo juntos, na memoria nvs.
esp_err_t estoque_salvar(const Estoque *dados);

#endif
