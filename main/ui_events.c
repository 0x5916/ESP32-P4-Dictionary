#include "ui/ui.h"
#include "search_ui.h"
#include "definition_ui.h"
#include "navigation.h"
#include "status_bar.h"
#include "wifi_ui.h"
#include "screen_manager.h"
#include "esp_log.h"

static const char *TAG = "ui_events";

void ui_events_init(void)
{
    ESP_LOGI(TAG, "[INIT] ui_events_init()");
    
    ESP_LOGD(TAG, "[BIND] Binding search textarea");
    search_ui_bind_textarea(objects.search_search_ta);

    ESP_LOGD(TAG, "[BIND] Binding search open button");
    search_ui_bind_search_open_button(objects.main_search_fake_ta);
    
    ESP_LOGD(TAG, "[BIND] Binding search<->main swipe back");
    navigation_bind_swipe_back(objects.search, objects.main);

    ESP_LOGD(TAG, "[BIND] Binding settings navigation");
    navigation_bind_button(objects.main_settings_btn, objects.settings);
    
    ESP_LOGD(TAG, "[BIND] Binding settings<->main swipe back");
    navigation_bind_swipe_back(objects.settings, objects.main);
    
    ESP_LOGD(TAG, "[BIND] Binding wifi<->settings swipe back");
    navigation_bind_swipe_back(objects.wi_fi, objects.settings);

    ESP_LOGD(TAG, "[BIND] Binding definition<->search swipe back");
    navigation_bind_swipe_back(objects.definition, objects.search);

    ESP_LOGD(TAG, "[BIND] Initializing WiFi UI");
    wifi_ui_init();
    
    ESP_LOGI(TAG, "[INIT] Event binding complete");
}