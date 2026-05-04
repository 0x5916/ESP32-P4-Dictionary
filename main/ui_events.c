#include "ui/ui.h"
#include "ui_overlays.h"
#include "wifi_ui.h"
#include "screen_manager.h"

void ui_events_init(void)
{
    ui_overlays_bind_textarea(objects.search_search_ta);

    ui_overlays_bind_search_open_button(objects.main_search_fake_ta);
    ui_overlays_bind_swipe_back(objects.search, objects.main);

    ui_overlays_bind_navigation_button(objects.main_settings_btn, objects.settings);
    ui_overlays_bind_swipe_back(objects.settings, objects.main);
    
    ui_overlays_bind_swipe_back(objects.wi_fi, objects.settings);

    wifi_ui_init();
}
