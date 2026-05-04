idf_component_register(
    SRCS
        main.c

        ui_overlays.c
        ui_events.c
        wifi_ui.c
        screen_manager.c
        custom_keyboard.c
        
        storage/sd_card.c

        dict_lookup.c
        dict_spellcheck.c
        dict_data.c

        wifi_service.c
        settings_service.c
        settings_ui.c
        clock_service.c

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
