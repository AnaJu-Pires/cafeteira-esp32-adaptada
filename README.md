# Cafeteira ESP32 — PCB adaptada

Adaptação do simulador de cafeteira escrito pela colega para uma **segunda PCB ESP32 de 38 pinos**, reutilizando LEDs, dois botões, potenciômetro e buzzer montados no projeto da lavadora.

> **Créditos:** lógica da cafeteira baseada no trabalho da colega responsável; adaptação de pinagem, polaridade dos botões e acionamento dos LEDs para a PCB alternativa. Preencha os nomes após combinar os créditos e obtenha autorização antes de tornar o código público.

## Hardware e diferenças da PCB original

| Periférico | GPIO | Observação |
|---|---:|---|
| LED verde | 17 | Ativo em LOW |
| LED amarelo | 18 | Ativo em LOW |
| LED vermelho | 16 | Ativo em LOW |
| BTN1 | 32 | Pull-down, pressionado em HIGH |
| BTN2 | 33 | Pull-down, pressionado em HIGH |
| Potenciômetro | 34 | ADC1_CH6 |
| Buzzer | 25 | PWM, com transistor BC337 |

A lógica da cafeteira foi mantida; a adaptação física fica principalmente em `main/config.h` e `main/perifericos.c`.

## Estado de validação

A usuária confirmou **compilação com ESP-IDF 5.5.6 e funcionamento dos comandos principais na placa física**, com logs de preparo, manutenção, cancelamento e estoque. A versão presente inclui a correção dos LEDs para acionamento ativo em LOW; **confirme visualmente o resultado dessa última correção** antes da aula.

## Preparação e gravação

Use ESP-IDF 5.5.6 no VS Code ou ESP-IDF PowerShell. Na raiz desta pasta:

```powershell
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash monitor
```

Troque `COM3` pela porta atual. Execute `set-target` somente na primeira configuração ou se estiver configurado para outro chip.

Alternativa: `./gravar.cmd COM3` no PowerShell ESP-IDF, depois da configuração inicial.

## Controles

- BTN1 curto: liga/desliga; durante preparo, cancela.
- BTN2 curto: inicia preparo; confirma reposição na manutenção.
- BTN1 por 3 s: entra ou sai da manutenção, quando permitido.
- POT: café curto (40 mL / 4 s), café longo (120 mL / 12 s) ou quantidade para reposição.
- NVS: armazena estoque; valores sobrevivem ao reinício.
- Inatividade: desligamento após 60 s fora do preparo.

Veja [GUIA_DEMONSTRACAO.md](GUIA_DEMONSTRACAO.md) para a sequência de testes.

Não misture este projeto com o da placa original. A gravação na PCB original pode fazer botões e LEDs não responderem porque **os GPIOs e as polaridades são diferentes**.
