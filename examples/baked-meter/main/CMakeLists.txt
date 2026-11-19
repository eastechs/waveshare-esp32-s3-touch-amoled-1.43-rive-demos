idf_component_register(SRCS "main.c" "vu_meter.c" PRIV_REQUIRES board esp_timer
    EMBED_FILES "../../../assets/vu-meter/vu-meter.rgb565")
