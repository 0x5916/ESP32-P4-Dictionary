#include "ui/ui.h"
#include "ui_overlays.h"
#include "wifi_ui.h"

void ui_events_init(void)
{
    ui_overlays_bind_textarea(objects.search_search_ta);

    ui_overlays_bind_search_open_button(objects.main_search_fake_ta);
    ui_overlays_bind_search_back_button(objects.search_back_btn);

    ui_overlays_bind_navigation_button(objects.main_settings_btn, objects.settings);
    ui_overlays_bind_search_back_button(objects.settings_back_btn);
    
    ui_overlays_bind_search_back_button(objects.wi_fi_back_btn);

    wifi_ui_init();
}
