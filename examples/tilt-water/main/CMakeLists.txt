idf_component_register(SRCS "main.c" "water.c" PRIV_REQUIRES board esp_timer
    EMBED_FILES "../../../assets/tilt-water/water.glass")
