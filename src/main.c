
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "wifi.h"

#define BUTTON_PIN GPIO_NUM_27
#define LED_PIN GPIO_NUM_18

static const char *TAG = "GAME";
static int led_state = 0;
static httpd_handle_t server = NULL;

static void set_led(int state)
{
    led_state = state;
    gpio_set_level(LED_PIN, led_state);
}

static esp_err_t home_handler(httpd_req_t *req)
{
    char page[1200];

    snprintf(page, sizeof(page),
        "<!DOCTYPE html><html><head>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>ESP32 Game Dashboard</title>"
        "<style>"
        "body{font-family:Arial;text-align:center;padding:40px;"
        "background:#f1f5f9}"
        "button{font-size:22px;padding:18px 35px;"
        "margin:12px;border:0;border-radius:12px;"
        "color:white;cursor:pointer}"
        ".on{background:#16a34a}"
        ".off{background:#dc2626}"
        "</style></head><body>"
        "<h1>ESP32 Game Dashboard</h1>"
        "<h2>LED Status: %s</h2>"
        "<form action='/on'><button class='on'>LED ON</button></form>"
        "<form action='/off'><button class='off'>LED OFF</button></form>"
        "<p>TPJ655 - IoT Output Control</p>"
        "</body></html>",
        led_state ? "ON" : "OFF");

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_sendstr(req, page);
}

static esp_err_t on_handler(httpd_req_t *req)
{
    set_led(1);
    ESP_LOGI(TAG, "Dashboard: LED ON");
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t off_handler(httpd_req_t *req)
{
    set_led(0);
    ESP_LOGI(TAG, "Dashboard: LED OFF");
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

static void start_webserver(void)
{
    if (server != NULL) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t home = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = home_handler
        };

        httpd_uri_t on = {
            .uri = "/on",
            .method = HTTP_GET,
            .handler = on_handler
        };

        httpd_uri_t off = {
            .uri = "/off",
            .method = HTTP_GET,
            .handler = off_handler
        };

        httpd_register_uri_handler(server, &home);
        httpd_register_uri_handler(server, &on);
        httpd_register_uri_handler(server, &off);

        ESP_LOGI(TAG, "Web dashboard started");
    }
}

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "Wi-Fi disconnected, retrying...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT &&
             event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event =
            (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG, "ESP32 IP ADDRESS: " IPSTR,
                 IP2STR(&event->ip_info.ip));

        start_webserver();
    }
}

static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,
        &wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP,
        &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(
        WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main(void)
{
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    set_led(0);

    gpio_reset_pin(BUTTON_PIN);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY);

    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    wifi_init();

    int last_button = 1;

    while (1)
    {
        int button = gpio_get_level(BUTTON_PIN);

        if (last_button == 1 && button == 0)
        {
            set_led(!led_state);
            ESP_LOGI(TAG, "Button: LED %s",
                     led_state ? "ON" : "OFF");
        }

        last_button = button;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
