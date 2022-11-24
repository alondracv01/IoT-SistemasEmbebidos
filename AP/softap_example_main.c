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


#define EXAMPLE_ESP_WIFI_SSID      "TAMARA Y ALONDRA"
#define EXAMPLE_ESP_WIFI_PASS      "12345678"
#define EXAMPLE_MAX_STA_CONN       10

#define LED_GPIO (2)
#define MAX 50

#define CABECERA 0x5A
#define FIN 0xB2

#define DEFAULT_COMMAND 0x30 // '0'
#define DEFAULT_LENGTH 0x34  // '4'
#define DEFAULT_DATA "0000"

char temperatura[4] = "35";
char estado [25] = "No encendido):";

static const char *TAG = "softAP_WebServer";

struct paquete{
    uint8_t  cabecera;
    uint8_t  comando;
    uint8_t  longitud;
    char     dato1;
    char     dato2;
    char     dato3;
    char     dato4;
    uint8_t  fin;
    uint8_t  CRC32_1;
    uint8_t  CRC32_2;
    uint8_t  CRC32_3;
    uint8_t  CRC32_4;
};

struct paquete* formar_paquete(uint8_t cabecera, uint8_t com, uint8_t length, char data[], uint8_t end, uint32_t crc){
    struct paquete *p;
    p=(struct paquete*)malloc(sizeof(struct paquete));
    p->cabecera = cabecera;
    p->comando = com;
    p->longitud = length;
    p->dato1 = data[0];
    p->dato2 = data[1];
    p->dato3 = data[2];
    p->dato4 = data[3];
    p->fin = end;
    p->CRC32_1 =  crc & 0xff;
    p->CRC32_2 = (crc & 0xff00)>>8;
    p->CRC32_3 = (crc & 0xff0000)>>16;
    p->CRC32_4 = (crc & 0xff000000)>>24;
    return (p);
}

void invertir_estado_led(uint8_t led_state){
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, !led_state);
} 

void myItoa(uint16_t number, char* str, uint8_t base){
    //convierte un valor numerico en una cadena de texto
    char *str_aux = str, n, *end_ptr, ch;
    int i=0, j=0;

    do{
        n=number % base;
        number /= base;
        n += '0';
        if(n >'9')
            n += 7;
        *(str++)=n;
        j++;
    }while(number>0);

    *(str--)='\0';
    end_ptr = str;
  
    for (i = 0; i < j / 2; i++) {
        ch = *end_ptr;
        *(end_ptr--) = *str_aux;
        *(str_aux++) = ch;
    }
}

uint32_t crc32b(char *message) {
   int i, j;
   unsigned int byte, crc, mask;
   i = 0;
   crc = 0xFFFFFFFF;
   while (message[i]) {
      byte = message[i];            // Get next byte.
      crc = crc ^ byte;
      for (j = 7; j >= 0; j--) {    // Do eight times.
         mask = -(crc & 1);
         crc = (crc >> 1) ^ (0xEDB88320 & mask);
      }
      i++; 
   }
   return ~crc;
}

void preprocessing_string_for_crc32(char *str, char *datos, uint8_t comando){
    *str = CABECERA;
    *(++str) = comando;
    *(++str) = strlen(datos) + '0';    
    while(*datos){
        *(++str) = *(datos++);
    }
    *(++str) = FIN;
    *(++str) = 0;
}

uint8_t package_validation(char *str, char *datos, char *comando){
    int contador=0, data_length=0;
    uint32_t crc_recibido = 0, crc_calculado;
    char crc32_aux[13]; //MAX
    if (str[0] != CABECERA){ 
        return 0;
    }else{ //COMANDO
        if (str[1] != 0 && str[1] != 1 && str[1] != 2){
            return 0;
        } else { //LONGITUD
            if (str[2] != '0') {
                data_length =  str[2] - '0'; 
                //DATOS
                while (contador < data_length){
                    datos[contador] = str[contador+3];
                    contador++;
                }
                datos[contador] = '\0';
                contador = 0;
            } else{
                data_length = 1;
            }
            if (str[data_length+3] != FIN){
                return 0;
            }else{
                crc_recibido |= (str[data_length+4] | str[data_length+5] << 8 | str[data_length+6] << 16  | str[data_length+7] << 24);
                preprocessing_string_for_crc32(crc32_aux, datos, str[1]);
                crc_calculado = crc32b(crc32_aux);
                if(crc_recibido!=crc_calculado){
                    return 0;
                }
            }
        }
    }
    *comando = str[1] + '0';
    return 1;
}

void delayMs(uint16_t ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

static esp_err_t resp_dir_html(httpd_req_t *req)
{
    httpd_resp_sendstr_chunk(req,
                        "<!DOCTYPE html>"
                        "<html>"
                        "<head>"
                        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" /> "
                        "</head>"

                        "<body style=\"margin-top:70px;height:2500px;background-color:#FFE4E1;\">"

                        "<h1 style=\"text-align:center;padding-bottom: 0.7em;  font-family: \"Raleway\", sans-serif;\"><span style=\"font-weight: 300;word-spacing: 3px;line-height: 2em;padding-bottom: 0.35em;\">Proyecto Sistemas Embebidos</span><br>Temperatura de una habitacion</h1>"
                        "<div align='center'>"
                        "<table>"
                        "<tr>"
                        "<td style=\"padding:30px;vertical-align:top;\">"
                        "<h3>Estado deseado en la habitacion</h3>"
                        "<label for=\"comandos\">Temperatura deseada en la habitacion:</label><br><br>"
                        "<form method='get'>"
                        "<input type=\"number\" id=\"fname\" name=\"fname\" style=\"background-color:#FFF0F5;border-color:#FFC0CB;\"><br><br>"
                        "<button type=\"submit\" onclick='dirigir(this.form)' style=\"background-color:#FF69B4;border-color:#FF69B4;\">Enviar comando</button><br><br>"
                        "</form>"
                        "</td>"
                        "<td style=\"padding:30px;vertical-align:top;\">"
                        "<h3>Estado actual en la habitacion</h3>"
                        "<label>Temperatura actual en la habitacion: </label>"
                        "<label id=\"temperatura\" name=\"temperatura\">");
    httpd_resp_sendstr_chunk(req, temperatura);
    httpd_resp_sendstr_chunk(req,
                        "</label><label> C</label><br><br>"
                        "<label>Mini split: </label>"
                        "<label id=\"estado\" name=\"estado\">");
    httpd_resp_sendstr_chunk(req, estado);
    httpd_resp_sendstr_chunk(req,
                        "</label>"
                        "</td>"
                        "</tr>"
                        "</table>"
                        "</div>"

                        "<script>"
                            "setTimeout(function(){"
                                "window.location.reload(1);"
                            "}, 5000);"
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
                        "</html>");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

static esp_err_t webserver_get_handler(httpd_req_t *req)
{
    return resp_dir_html(req);
}

esp_err_t post_handler(httpd_req_t *req)
{
    /* Destination buffer for content of HTTP POST request.
     * httpd_req_recv() accepts char* only, but content could
     * as well be any binary data (needs type casting).
     * In case of string data, null termination will be absent, and
     * content length would give length of string */

    /* Truncate if content length larger than the buffer */
    size_t recv_size = MIN(req->content_len, sizeof(temperatura));

    int ret = httpd_req_recv(req, temperatura, recv_size);
    if (ret <= 0) {  /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        return ESP_FAIL;
    }

    /* Send a simple response */
    const char resp[] = "URI POST Response";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
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

    if (strcmp("Refrigeracion encendida", resp_str) == 0)
    {
        if (strcmp(estado, resp_str) != 0)
        {
            strcpy(estado, resp_str);
        }
    } else if (strcmp("Calefaccion encendida", resp_str) == 0)
    {
        if (strcmp(estado, resp_str) != 0)
        {
            strcpy(estado, resp_str);
        }
    } else if (strcmp("No encendido", resp_str) == 0){
        if (strcmp(estado, resp_str) != 0)
        {
            strcpy(estado, resp_str);
        }
    } else{
        if (strcmp(temperatura, resp_str) != 0)
        {
            strcpy(temperatura, resp_str);
        }
    }

    strcpy(cadena, resp_str);
    
    httpd_resp_send(req, cadena, HTTPD_RESP_USE_STRLEN);

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

httpd_uri_t file_server = {
    .uri = "/proyecto",
    .method = HTTP_GET,
    .handler = webserver_get_handler,
    .user_ctx = temperatura
};

httpd_uri_t uri_post = {
    .uri      = "/proyecto",
    .method   = HTTP_POST,
    .handler  = post_handler,
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
        httpd_register_uri_handler(server, &file_server);
        httpd_register_uri_handler(server, &uri_post);
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