idf_component_register(SRCS "main.c" "watch.c" "watch_clock.c"
    PRIV_INCLUDE_DIRS "../../../assets/watch"
    PRIV_REQUIRES board driver esp_app_format esp_timer esp_rom
    EMBED_FILES "../../../assets/pocket-watch/watch-dial.argb.z"
                "../../../assets/pocket-watch/watch-lid.argb.z"
                "../../../assets/pocket-watch/watch-tray.argb.z")
