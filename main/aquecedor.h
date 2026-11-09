#ifndef AQUECEDOR_H
#define AQUECEDOR_H

#include <stdint.h>

// simula o aquecimento da agua: esquenta quando ligado e esfria aos poucos
// quando desligado. nao tem sensor, e so conta com o tempo.
void aquecedor_ligar(int64_t agora_ms, int agua_ml);
void aquecedor_desligar(int64_t agora_ms);

// a quantidade de agua mudou com o aquecedor ligado (reposicao).
void aquecedor_ajustar_agua(int64_t agora_ms, int agua_ml);

// temperatura simulada da agua, em graus celsius.
int aquecedor_temperatura(int64_t agora_ms);

// quanto falta para chegar na temperatura de preparo (0 = ja esta quente).
int64_t aquecedor_restante_ms(int64_t agora_ms);

#endif
