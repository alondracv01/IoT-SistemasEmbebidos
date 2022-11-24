/* WiFi station Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <time.h>
#include "sdkconfig.h"
#include "rom/ets_sys.h"
#include "dht11.h"
#include "mUart.h"
/* The examples use WiFi configuration that you can set via project configuration menu
   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      "TAMARA Y ALONDRA"
#define EXAMPLE_ESP_WIFI_PASS      "12345678"
#define EXAMPLE_ESP_MAXIMUM_RETRY  10

char temperatura[10] = "40";
char temperaturaDeseada[4] = "28";
char humedad[10] = "40";
uint16_t tempdes = 0;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define DHT_GPIO GPIO_NUM_16

#define UART_RX_PIN     (3)
#define UART_TX_PIN     (1)

#define UART_RX_PIN_2    (16)
#define UART_TX_PIN_2    (17)

#define UARTS_BAUD_RATE         (115200)
#define TASK_STACK_SIZE         (1048)
#define READ_BUF_SIZE           (1024)

#define BUF_SIZE (1024)
#define LED_GPIO (2)

#define MAX 50

#define CABECERA 0x5A
#define FIN 0xB2

#define DEFAULT_COMMAND 0x30 // '0'
#define DEFAULT_LENGTH 0x34  // '4'
#define DEFAULT_DATA "0000"


static const char *TAG = "wifi station";

static int s_retry_num = 0;

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

void UartInit(uart_port_t uart_num, uint32_t baudrate, uint8_t size, uint8_t parity, uint8_t stop, uint8_t txPin, uint8_t rxPin){
    uart_config_t uart_config = {
        .baud_rate = (int) baudrate,
        .data_bits = (uart_word_length_t)(size-5),
        .parity    = (uart_parity_t)parity,
        .stop_bits = (uart_stop_bits_t)stop,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    ESP_ERROR_CHECK(uart_driver_install(uart_num, READ_BUF_SIZE, READ_BUF_SIZE, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uart_num, txPin, rxPin,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

void delayMs(uint16_t ms){
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void UartPutchar(uart_port_t uart_num, char c){
    uart_write_bytes(uart_num, &c, sizeof(c));
}

char UartGetchar(uart_port_t uart_num){
    char c;
    uart_read_bytes(uart_num, &c, sizeof(c), 0);
    return c;
} 

void UartPuts(uart_port_t uart_num, char *str){
    while(*str!='\0'){   
        UartPutchar(uart_num,*(str++));
    }
}

void UartGets(uart_port_t uart_num, char *str){
    uint8_t cad=50;
    char c;
    const char *in=str;
    int i = 0;

    c=UartGetchar(uart_num);
    while (i < 50) {
        if (c == '\n'){
            break;
        }
        if(str < (in + cad - 1)){
            *str = c;
            str++;
        }
        c=UartGetchar(uart_num);
        i++;
    }
    *str = 0;
    str -= i;
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

uint8_t package_validation(char *str, char *datos, uint8_t *comando){
    int contador=0, data_length=0;
    uint32_t crc_recibido = 0, crc_calculado;
    char crc32_aux[13]; //MAX
    if (str[0] != CABECERA){ 
        return 0;
    }else{ //COMANDO
        if (str[1] != 0x13){
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
                    //return 0;
                }
            }
        }
    }
    *comando = str[1];
    return 1;
}

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

uint16_t myAtoi(char *str){
    uint16_t res = 0;
    while(*str){
        if((*str >= 48) && (*str <= 57)){
            res *= 10;
            res += *(str++)-48;
        }
        else
            break;
    }
    return res;
}

void enviar_temperatura(char *cade){
    srand((unsigned int)time(NULL));
    int num = rand() % 100;
    myItoa(num, cade, 10);
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

esp_err_t client_event_get_handler (esp_http_client_event_handle_t evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len; 
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA: %.*s\n", evt->data_len, (char *)evt->data);

        if (!esp_http_client_is_chunked_response(evt->client)) {
            // If user_data buffer is configured, copy the response into the buffer
            if (evt->user_data) {
                memcpy(evt->user_data + output_len, evt->data, evt->data_len);
            } else {
                if (output_buffer == NULL) {
                    output_buffer = (char *) malloc(esp_http_client_get_content_length(evt->client));
                    output_len = 0;
                    if (output_buffer == NULL) {
                        ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                        return ESP_FAIL;
                    }
                }
                memcpy(output_buffer + output_len, evt->data, evt->data_len);
            }
            output_len += evt->data_len;
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        if (output_buffer != NULL) {
            // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
            // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void rest_post (char *data)
{
    char local_response_buffer[4] = {0};
    esp_http_client_config_t config_post ={
        .url = "http://192.168.4.1/proyecto",
        .method = HTTP_METHOD_POST,
        .event_handler = client_event_get_handler,
        .cert_pem = NULL,
        .is_async = true,
        .user_data = local_response_buffer,
        .timeout_ms = 5000};

    esp_http_client_handle_t client = esp_http_client_init(&config_post);
    esp_err_t err;
    esp_http_client_set_post_field(client, data, strlen(data));
    while (1) {
        err = esp_http_client_perform(client);
        if (err != ESP_ERR_HTTP_EAGAIN) {
            break;
        }
    }
    if (err == ESP_OK) {
            ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
            strcpy(temperaturaDeseada, local_response_buffer);
            tempdes = myAtoi(temperaturaDeseada);
    } else {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}


void app_main(void)
{
    char temp[] = "0000";

    setDHTPin(4);
    printf("Starting DHT measurement!\n");

    uint8_t comando = 0x10;
    uint32_t crc32_calculado;
    char str_aux_for_crc32[13], paquete_recibido[MAX], datos_recibidos[5];
    int tempera = 0;

    struct paquete * package;

    UartInit(0, UARTS_BAUD_RATE, 8, 0, 1, UART_TX_PIN,   UART_RX_PIN);
    UartInit(2, UARTS_BAUD_RATE, 8, 0, 1, UART_TX_PIN_2, UART_RX_PIN_2);

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    while (1)
    {
        tempera = getTemp();
        if (tempera <= 50 && tempera >= 0)
        {
            if (tempdes == tempera)
            {
                comando = 0x10;
            } else if (tempdes < tempera)
            {
                comando = 0x11;
            } else{
                comando = 0x12;
            }
        }

        preprocessing_string_for_crc32(str_aux_for_crc32, DEFAULT_DATA, comando);
        crc32_calculado = crc32b(str_aux_for_crc32);
        package = formar_paquete(CABECERA, comando, DEFAULT_LENGTH, DEFAULT_DATA, FIN, crc32_calculado); 
        UartPuts(2, package);
        printf("\nComando: %x\n", comando);
        
        myItoa(tempera, temperatura, 10);
        preprocessing_string_for_crc32(str_aux_for_crc32, temperatura, 0x13);
        crc32_calculado = crc32b(str_aux_for_crc32);
        temp[2] = temperatura[0];
        temp[3] = temperatura[1];
        package = formar_paquete(CABECERA, 0x13, DEFAULT_LENGTH, &temp, FIN, crc32_calculado);
        printf("Temperatura deseada: %s\n",temperaturaDeseada);
        printf("Temperatura leida:  %s\n",temperatura);
        rest_post(package);
        delayMs(3000);
    }
}