#ifndef AQUECEDOR_H
#define AQUECEDOR_H

#include <stdint.h>

// simula o thermoblock de uma cafeteira de capsula: o bloco esquenta quando
// ligado e esfria aos poucos quando desligado. a agua so passa por ele na hora
// do cafe, entao o tanque nao muda o tempo. nao tem sensor, so conta o tempo.
void aquecedor_ligar(int64_t agora_ms);
void aquecedor_desligar(int64_t agora_ms);

// temperatura simulada do bloco, em graus celsius.
int aquecedor_temperatura(int64_t agora_ms);

// quanto falta para chegar na temperatura de preparo (0 = ja esta quente).
int64_t aquecedor_restante_ms(int64_t agora_ms);

#endif
