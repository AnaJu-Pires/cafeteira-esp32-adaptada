# Guia de demonstração rápida

1. Abra um **ESP-IDF PowerShell** configurado e conecte a ESP32 por cabo de dados.
2. Verifique a porta COM (a porta pode mudar quando trocar de placa).
3. Na raiz deste projeto, compile: `idf.py build`.
4. Grave e abra o monitor: `idf.py -p COM3 flash monitor` (altere a COM).
5. Execute a demonstração: BTN1 para ligar; potenciômetro para selecionar café curto (40 mL / 4 s) ou longo (120 mL / 12 s); BTN2 para preparar; BTN1 para cancelar durante a preparação; BTN1 pressionado 3 s para manutenção; reposição e retorno.
6. Observe o estoque persistente na NVS: ele **não necessariamente recomeça** em 600 mL e 3 cápsulas.
7. Para sair do monitor: `Ctrl + ]`.

**Plano de contingência:** se um LED não funcionar, mostre a transição de estado no monitor e explique que o circuito físico está pendente de diagnóstico. Evite atribuir o defeito ao firmware sem medições. O comportamento dos indicadores e do buzzer precisa ser verificado na placa, separadamente do log.

**Para trocar de placa:** use o **outro repositório**, nunca reutilize esta versão com a pinagem de outra PCB. Confirme qual porta COM foi atribuída e feche o monitor serial antes de gravar.
