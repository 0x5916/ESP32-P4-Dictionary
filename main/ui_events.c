#include "ui/ui.h"
#include "ui_overlays.h"
#include "wifi_ui.h"
#include "screen_manager.h"
#include "esp_log.h"

static const char *TAG = "ui_events";

void ui_events_init(void)
{
    ESP_LOGI(TAG, "[INIT] ui_events_init()");
    
    ESP_LOGD(TAG, "[BIND] Binding search textarea");
    ui_overlays_bind_textarea(objects.search_search_ta);

    ESP_LOGD(TAG, "[BIND] Binding search open button");
    ui_overlays_bind_search_open_button(objects.main_search_fake_ta);
    
    ESP_LOGD(TAG, "[BIND] Binding search<->main swipe back");
    ui_overlays_bind_swipe_back(objects.search, objects.main);

    ESP_LOGD(TAG, "[BIND] Binding settings navigation");
    ui_overlays_bind_navigation_button(objects.main_settings_btn, objects.settings);
    
    ESP_LOGD(TAG, "[BIND] Binding settings<->main swipe back");
    ui_overlays_bind_swipe_back(objects.settings, objects.main);
    
    ESP_LOGD(TAG, "[BIND] Binding wifi<->settings swipe back");
    ui_overlays_bind_swipe_back(objects.wi_fi, objects.settings);

    ESP_LOGD(TAG, "[BIND] Binding definition<->search swipe back");
    ui_overlays_bind_swipe_back(objects.definition, objects.search);

    ESP_LOGD(TAG, "[BIND] Initializing WiFi UI");
    wifi_ui_init();
    
    ESP_LOGI(TAG, "[INIT] Event binding complete");
}
