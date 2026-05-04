idf_component_register(
    SRCS
        main.c

        ui_overlays.c
        ui_events.c

        screen_manager.c
        custom_keyboard.c
        clock_service.c

        dict_lookup.c
        dict_spellcheck.c

        wifi_service.c
        storage/sd_card.c

        ui/ui.c
        ui/styles.c
        ui/images.c
        ui/screens.c
    INCLUDE_DIRS . "ui")

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(
    ${LVGL_LIB} 
    PRIVATE
        -DLV_LVGL_H_INCLUDE_SIMPLE
)
