idf_component_register(
    SRCS "aquecedor.c" "main.c" "maquina.c" "perifericos.c" "estoque.c"
    INCLUDE_DIRS "."
    REQUIRES esp_adc esp_driver_gpio esp_driver_ledc esp_timer nvs_flash
)
