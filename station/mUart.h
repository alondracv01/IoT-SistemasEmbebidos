#ifndef M_UART_H
#define M_UART_H

#include "driver/uart.h"

//--------------------------UART handler--------------------------

/**
 * @brief Configure and install the default UART, then, connect it to the
 * console UART.
 */
void UartInit(uart_port_t uart_num, uint32_t baudrate, uint8_t size, uint8_t parity, uint8_t stop, uint8_t txPin, uint8_t rxPin);

/**
 * @brief Delay milliseconds
 */
void DelayMs(uint16_t ms);

/**
 * @brief Prints a character
 */
void UartPutchar(uart_port_t uart_num, char c);

/**
 * @brief Prints a string
 */
void UartPuts(uart_port_t uart_num, char *str);

/**
 * @brief Takes a character from the uart and returns it
 */
char UartGetchar(uart_port_t uart_num);

/**
 * @brief Saves a string in a variable
 */
void UartGets(uart_port_t uart_num, char *str);

/**
 * @brief Turns a numeric value into a string 

 */
void myItoa(uint16_t number, char* str, uint8_t base);

/**
 * @brief Turns a string into a numeric value
 */
uint16_t myAtoi(char *str);

//--------------------------Package structure--------------------------

/**
 * @brief Calculates the CRC32 of a string
 */
uint32_t crc32b(char *message);

/**
 * @brief The structure paquete needs to be a single string to calculate the CRC32 function, from Headset to End
 */
void preprocessing_string_for_crc32(char *str, char *datos, uint8_t comando);

/**
 * @brief Forms a package with a given string, calculates the CRC32 function and sends the value through Uart
 */
void enviar_paquete(char *str, uint8_t longitud);

//--------------------------Reactive behavior--------------------------

/**
 * @brief Creates a string of 4 digits to simulate a timestamp
 */
void enviar_timestamp(void);

/**
 * @brief Read the state of the led in the ESP32 board and turns it into a 4 character string 
 */
void enviar_estado_led(void);

/**
 * @brief Creates a string of values from 0 to 100 (°C)
 */

/**
 * @brief Prints a character
 */
void invertir_estado_led(void);

#endif /* M_UART_H */