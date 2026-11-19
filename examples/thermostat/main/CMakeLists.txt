idf_component_register(SRCS "main.c" "thermostat.c" "thermostat_view.c"
    PRIV_REQUIRES board rive_thorvg
    EMBED_FILES "../../../assets/thermostat/thermostat.riv")
