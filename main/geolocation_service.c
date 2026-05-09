#include "geolocation_service.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "settings_service.h"
#include "timezone_data.h"
#include "clock_service.h"

#include <string.h>

static const char *TAG = "geolocation_service";
static bool s_detecting = false;
static uint8_t s_detected_timezone_index = TIMEZONE_INDEX_AUTO;

#define GEOLOCATION_API_URL "http://ip-api.com/json?fields=timezone"
#define MAX_RESPONSE_SIZE 512

typedef struct {
    char response_data[MAX_RESPONSE_SIZE];
    int response_len;
} http_response_t;

static esp_err_t http_event_handle(esp_http_client_event_t *evt)
{
    http_response_t *resp = (http_response_t *)evt->user_data;
    
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (resp->response_len + evt->data_len < MAX_RESPONSE_SIZE) {
                memcpy(&resp->response_data[resp->response_len], evt->data, evt->data_len);
                resp->response_len += evt->data_len;
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

static uint8_t parse_geolocation_response(const char *response)
{
    if (!response || response[0] == '\0') {
        ESP_LOGE(TAG, "[GEO] Empty response");
        return 0;  // Default to UTC
    }

    cJSON *json = cJSON_Parse(response);
    if (!json) {
        ESP_LOGE(TAG, "[GEO] Failed to parse JSON response");
        return 0;
    }

    uint8_t tz_index = 0;
    cJSON *tz_item = cJSON_GetObjectItem(json, "timezone");
    
    if (tz_item && tz_item->valuestring) {
        ESP_LOGI(TAG, "[GEO] Detected timezone: %s", tz_item->valuestring);
        tz_index = timezone_find_by_name(tz_item->valuestring);
        ESP_LOGI(TAG, "[GEO] Mapped to index: %u", tz_index);
    } else {
        ESP_LOGW(TAG, "[GEO] No timezone in response");
    }

    cJSON_Delete(json);
    return tz_index;
}

void geolocation_service_init(void)
{
    ESP_LOGI(TAG, "[INIT] geolocation_service_init()");
    s_detecting = false;
    s_detected_timezone_index = TIMEZONE_INDEX_AUTO;
}

esp_err_t geolocation_service_detect_timezone(void)
{
    // Check if timezone is in AUTO mode
    uint8_t current_tz = 0;
    esp_err_t err = settings_get_u8(SETTINGS_KEY_TIMEZONE, TIMEZONE_INDEX_AUTO, &current_tz);
    
    ESP_LOGI(TAG, "[GEO] Check timezone: current_tz=%u (err=%s)", current_tz, esp_err_to_name(err));
    
    if (current_tz != TIMEZONE_INDEX_AUTO) {
        ESP_LOGD(TAG, "[GEO] Timezone is manually set to %u, skipping auto-detection", current_tz);
        return ESP_OK;
    }

    if (s_detecting) {
        return ESP_OK;
    }

    s_detecting = true;
    ESP_LOGD(TAG, "[GEO] Starting timezone detection");

    http_response_t http_resp = {0};
    
    esp_http_client_config_t config = {
        .url = GEOLOCATION_API_URL,
        .event_handler = http_event_handle,
        .user_data = &http_resp,
        .timeout_ms = 10000,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
    };

    ESP_LOGD(TAG, "[GEO] HTTP client config: url=%s, timeout=%d ms", GEOLOCATION_API_URL, config.timeout_ms);
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "[GEO] Failed to initialize HTTP client");
        s_detecting = false;
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "[GEO] HTTP client initialized, performing request...");

    err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[GEO] HTTP request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        s_detecting = false;
        return err;
    }

    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGD(TAG, "[GEO] HTTP response received: status=%d, bytes=%d", status_code, http_resp.response_len);
    
    if (status_code != 200) {
        ESP_LOGW(TAG, "[GEO] HTTP status code: %d", status_code);
        esp_http_client_cleanup(client);
        s_detecting = false;
        return ESP_FAIL;
    }

    esp_http_client_cleanup(client);

    // Null-terminate response
    if (http_resp.response_len < MAX_RESPONSE_SIZE) {
        http_resp.response_data[http_resp.response_len] = '\0';
    }
    
    ESP_LOGD(TAG, "[GEO] Response data: %s", http_resp.response_data);

    // Parse response and get timezone
    uint8_t tz_index = parse_geolocation_response(http_resp.response_data);
    ESP_LOGD(TAG, "[GEO] Parsed timezone index: %u", tz_index);

    s_detected_timezone_index = tz_index;
    ESP_LOGD(TAG, "[GEO] Timezone cached for AUTO mode: index=%u", tz_index);
    clock_service_notify_time_synced();

    s_detecting = false;
    return err;
}

bool geolocation_service_is_detecting(void)
{
    return s_detecting;
}

uint8_t geolocation_service_get_detected_timezone_index(void)
{
    return s_detected_timezone_index;
}
