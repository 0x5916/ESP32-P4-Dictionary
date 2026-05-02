set(LV_DEMO_DIR ../managed_components/lvgl__lvgl/demos)
file(GLOB_RECURSE LV_DEMOS_SOURCES ${LV_DEMO_DIR}/*.c)

idf_component_register(
    SRCS
        main.c

        ui_overlays.c
        ui_events.c

        screen_manager.c
        custom_keyboard.c
        
        dictionary_state.c
        storage/sd_card.c
        dictionary/dict_manager.c
        dictionary/dict_parser.c
        dictionary/dict_search.c

        ui/ui.c
        ui/styles.c
        ui/images.c
        ui/screens.c
        ${LV_DEMOS_SOURCES}
    INCLUDE_DIRS . "ui" ${LV_DEMO_DIR})

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(
    ${LVGL_LIB} 
    PRIVATE
        -DLV_LVGL_H_INCLUDE_SIMPLE
        -DLV_USE_DEMO_MUSIC
)
