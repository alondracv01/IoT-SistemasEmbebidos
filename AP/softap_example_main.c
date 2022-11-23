#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>
#include "esp_eth.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <time.h>


#define EXAMPLE_ESP_WIFI_SSID      "TAMARA"
#define EXAMPLE_ESP_WIFI_PASS      "12345678"
#define EXAMPLE_MAX_STA_CONN       10

#define LED_GPIO (2)
#define MAX 50

static const char *TAG = "softAP_WebServer";

void myItoa(uint16_t number, char* str, uint8_t base)
{//convierte un valor numerico en una cadena de texto
    char *str_aux = str, n, *end_ptr, ch;
    int i=0, j=0;

    do{
        n=number % base;
        number=number/base;
        n+='0';
        if(n>'9')
            n=n+7;
        *(str++)=n;
        j++;
    }while(number>0);

    *(str--)='\0';
    
    end_ptr = str;
  
    for (i = 0; i < j / 2; i++) {
        ch = *end_ptr;
        *end_ptr = *str_aux;
        *str_aux = ch;
          str_aux++;
        end_ptr--;
    }
}

#define TIMESTAMP_LEN 10
void enviar_timestamp(char *cade){
    char str[TIMESTAMP_LEN];
    sprintf(str, "%d\n", xTaskGetTickCount());
    strcpy(cade, str);
}

void enviar_estado_led(char *cade){
    gpio_set_direction(LED_GPIO, GPIO_MODE_INPUT_OUTPUT);
    int led = gpio_get_level(LED_GPIO);
    char led_state = led+'0';
    cade[0] = led_state;
    cade[1] = '\0';
}

#define TEMPERATURA_MAX 100.0
void enviar_temperatura(char *cade){
/*     srand((unsigned int)time(NULL));
    int num = rand() % 100;
    myItoa(num, cade, 10); */
     srand((unsigned int)time(NULL));
    float num = ((float)rand()/(float)(RAND_MAX)) * TEMPERATURA_MAX;
    sprintf(cade, " %f", num);
}

void invertir_estado_led(uint8_t led_state){
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, !led_state);
} 

static esp_err_t resp_dir_html(httpd_req_t *req)
{
    httpd_resp_send_chunk(req,
                          "<!DOCTYPE html>"
                          "<html>"
                          "<head>"
                          "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" /> "
                          "</head>"

                          "<body style=\"margin-top:70px;height:2500px;\">"

                          "<h1 style=\"text-align:center;padding-bottom: 0.7em;  font-family: \"Raleway\", sans-serif;\"><span style=\"font-weight: 300;word-spacing: 3px;line-height: 2em;padding-bottom: 0.35em;\">Proyecto Sistemas Embebidos</span>Temperatura de una habitacion</h1>"
                          "<label for=\"comandos\">Temperatura de la habiatacion:</label>"
                          "<form method='get'>"
                          "<input type=\"text\" id=\"fname\" name=\"fname\"><br>"
                          "<button type=\"submit\" onclick='dirigir(this.form)'>Enviar comando</button>"
                          "</form>"
                          "<img src=\"./img/temperatura.png\" alt=\"Temperatura\" style=\"width:500px;height:600px;\">"

                          "<script>"
                            "function dirigir(form)"
                            "{"
                                "if(form.comandos.value=='1')"
                                "{"
                                    "window.open('./0x10')"
                                "}else if(form.comandos.value=='2')"
                                "{"
                                    "window.open('./0x11')"
                                "}else if(form.comandos.value=='3')"
                                "{"
                                    "window.open('./0x12')"
                                "}else if(form.comandos.value=='4')"
                                "{"
                                    "window.open('./0x13')"
                                "}else"
                                "{"
                                    "window.open('./hello')"
                                "}"
                            "}"
                          "</script>"
                          "</body>"
                          "</html>",
    -1);
    return ESP_OK;
}

static esp_err_t webserver_get_handler(httpd_req_t *req)
{
    return resp_dir_html(req);
}

/* An HTTP GET handler */
static esp_err_t get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-2: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-1") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-1", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-1: %s", buf);
        }
        free(buf);
    }

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[32];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "query1", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query1=%s", param);
            }
            if (httpd_query_key_value(buf, "query3", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query3=%s", param);
            }
            if (httpd_query_key_value(buf, "query2", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query2=%s", param);
            }
        }
        free(buf);
    }

    /* Set some custom headers */
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    char cadena[20];
    char temp[10];
    if (strcmp(resp_str, "Timestamp: ") == 0)
    {
        enviar_timestamp(temp);
        strcat(strcpy(cadena, resp_str), temp);
    }else if (strcmp(resp_str, "Led State: ") == 0)
    {
        enviar_estado_led(temp);
        strcat(strcpy(cadena, resp_str), temp);
    }else if (strcmp(resp_str, "Temperatura: ") == 0)
    {
        enviar_temperatura(temp);
        strcat(strcpy(cadena, resp_str), temp);
    }else if (strcmp(resp_str, "Estado del led invertido") == 0)
    {
        enviar_estado_led(temp);
        invertir_estado_led(temp[0]-'0');
        strcpy(cadena, "LED invertido");
    }else{
        strcpy(cadena, resp_str);
    }
    
    httpd_resp_send(req, cadena, HTTPD_RESP_USE_STRLEN);

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}


static const httpd_uri_t hello = {
    .uri       = "/hello",
    .method    = HTTP_GET,
    .handler   = get_handler,
    .user_ctx  = "Hola Mundo!!"
};

static const httpd_uri_t timestamp = {
    .uri       = "/0x10",
    .method    = HTTP_GET,
    .handler   = get_handler,
    .user_ctx  = "Timestamp: "
};

static const httpd_uri_t ledState = {
    .uri       = "/0x11",
    .method    = HTTP_GET,
    .handler   = get_handler,
    .user_ctx  = "Led State: "
};

static const httpd_uri_t temperatura = {
    .uri       = "/0x12",
    .method    = HTTP_GET,
    .handler   = get_handler,
    .user_ctx  = "Temperatura: "
};

static const httpd_uri_t invLed = {
    .uri       = "/0x13",
    .method    = HTTP_GET,
    .handler   = get_handler,
    .user_ctx  = "Estado del led invertido"
};

httpd_uri_t file_server = {
    .uri = "/p5",
    .method = HTTP_GET,
    .handler = webserver_get_handler,
    .user_ctx = NULL
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Iniciar el servidor httpd 
    ESP_LOGI(TAG, "Iniciando el servidor en el puerto: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Manejadores de URI
        ESP_LOGI(TAG, "Registrando manejadores de URI");
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &timestamp);
        httpd_register_uri_handler(server, &ledState);
        httpd_register_uri_handler(server, &temperatura);
        httpd_register_uri_handler(server, &invLed);
        httpd_register_uri_handler(server, &file_server);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "estacion "MACSTR" se unio, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "estacion "MACSTR" se desconecto, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

esp_err_t wifi_init_softap(void)
{
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Inicializacion de softAP terminada. SSID: %s password: %s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    return ESP_OK;
}


void app_main(void)
{
    httpd_handle_t server = NULL;

    ESP_ERROR_CHECK(nvs_flash_init());
    esp_netif_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "init softAP");
    ESP_ERROR_CHECK(wifi_init_softap());

    server = start_webserver();
}


/* #include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>
#include "esp_eth.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define EXAMPLE_ESP_WIFI_SSID      "TAMARA"
#define EXAMPLE_ESP_WIFI_PASS      "12345678"
#define EXAMPLE_MAX_STA_CONN        10

static const char *TAG = "softAP_WebServer";
 */
/* An HTTP GET handler */
/* static esp_err_t hello_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

    // Get header value string length and allocate memory for length + 1,
    // extra byte for null termination 
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        // Copy null terminated value string into buffer 
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-2: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-1") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-1", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-1: %s", buf);
        }
        free(buf);
    }

    // Read URL query string length and allocate memory for length + 1,
    // extra byte for null termination 
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[32];
            // Get value of expected key from query string 
            if (httpd_query_key_value(buf, "query1", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query1=%s", param);
            }
            if (httpd_query_key_value(buf, "query3", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query3=%s", param);
            }
            if (httpd_query_key_value(buf, "query2", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query2=%s", param);
            }
        }
        free(buf);
    }

    // Set some custom headers 
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    // Send response with custom headers and body set as the
    // string passed in user context
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    // After sending the HTTP response the old HTTP request
    // headers are lost. Check if HTTP request headers can be read now. 
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}
 */
/* An HTTP POST handler */
/* static esp_err_t hello_post_handler(httpd_req_t *req)
{
    char content[100];
    httpd_req_recv(req, content, sizeof(content));
    printf("Content received: %s\n", content);

    vTaskDelay(1000 / portTICK_PERIOD_MS);
 
    httpd_resp_send(req, "URI POST Response", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
} */

/* static const httpd_uri_t package = {
    .uri       = "/practica5",
    .method    = HTTP_POST,
    .handler   = hello_post_handler,
    .user_ctx  = "post "
}; */

/* static const httpd_uri_t hello = {
    .uri       = "/hello",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    .user_ctx  = "Hola Mundo!!"
};

static esp_err_t resp_dir_html(httpd_req_t *req)
{
    httpd_resp_send_chunk(req,
                          "<!DOCTYPE html>"
                          "<html>"
                          "<head>"
                          "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" /> "
                          "</head>"

                          "<body style=\"margin-top:70px;height:1500px;\">"

                          "<label for=\"comandos\">Elija un comando:</label>"
                          "<select name=\"comandos\" id=\"comandos\">"
                            "<option value=\"1\">0x10</option>"
                            "<option value=\"2\">0x11</option>"
                            "<option value=\"3\">0x12</option>"
                            "<option value=\"4\">0x13</option>"
                          "</select>"
                          "</body>"
                          "</html>",
    -1);
    return ESP_OK;
}

static esp_err_t webserver_get_handler(httpd_req_t *req)
{
    return resp_dir_html(req);
} */

/* static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Iniciar el servidor httpd 
    ESP_LOGI(TAG, "Iniciando el servidor en el puerto: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Manejadores de URI
        ESP_LOGI(TAG, "Registrando manejadores de URI");
        httpd_register_uri_handler(server, &hello);
        //httpd_register_uri_handler(server, &resp_dir_html);

        httpd_uri_t file_server = {
                .uri = "/p5", // Match all URIs of type /path/to/file
                .method = HTTP_GET,
                .handler = webserver_get_handler,
                .user_ctx = NULL // Pass server data as context
            };

        httpd_register_uri_handler(server, &file_server);

        return server;
    }

  //    httpd_uri_t file_server = {
        .uri = "", // Match all URIs of type /path/to/file
        .method = HTTP_GET,
        .handler = webserver_get_handler,
        .user_ctx = server_data // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_server); 

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}
 */
/* static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "estacion "MACSTR" se unio, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "estacion "MACSTR" se desconecto, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

esp_err_t wifi_init_softap(void)
{
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Inicializacion de softAP terminada. SSID: %s password: %s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    return ESP_OK;
} */

//Comandos
/* void enviar_timestamp(char *timestamp){
    sprintf(timestamp, "Timestamp : %d", xTaskGetTickCount());
}

#define LED_GPIO 2
void enviar_estado_led(char *led_char){
    gpio_set_direction(LED_GPIO, GPIO_MODE_INPUT_OUTPUT);
    int led = gpio_get_level(LED_GPIO); 
    sprintf(led_char, "Led state : %d\n", led);
}

#define TEMPERATURA_MAX 100.0
void enviar_temperatura(char *temperatura){
    srand((unsigned int)time(NULL));
    float num = ((float)rand()/(float)(RAND_MAX)) * TEMPERATURA_MAX;
    sprintf(temperatura, "Temperatura : %f", num);
}

const char* invertir_estado_led(){
    static uint8_t led_state=1;
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, led_state);
    led_state=!led_state;
    return "Estado del led invertido";
}   */

/* void app_main(void)
{
    httpd_handle_t server = NULL;

    ESP_ERROR_CHECK(nvs_flash_init());
    esp_netif_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "init softAP");
    ESP_ERROR_CHECK(wifi_init_softap());

    server = start_webserver();
} */

/* #include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define EXAMPLE_ESP_WIFI_SSID      "WIFI_ARTC"
#define EXAMPLE_ESP_WIFI_PASS      "04072509"
#define EXAMPLE_ESP_WIFI_CHANNEL   CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_MAX_STA_CONN       CONFIG_ESP_MAX_STA_CONN

static const char *TAG = "wifi softAP";

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
}
 */