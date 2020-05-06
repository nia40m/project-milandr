#include "MDR32Fx.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "MDR32F9Qx_rst_clk.h"
#include "MDR32F9Qx_port.h"
#include "MDR32F9Qx_uart.h"
#include "MDR32F9Qx_ssp.h"
#include "MDR32F9Qx_it.h"

#define TO_STRING(x)        #x
#define VAL_TO_STRING(x)    TO_STRING(x)

#define MAJOR_VERSION       0
#define MINOR_VERSION       2

#define HELLO_STRING    \
    "{\"name\":\"Milandr\",\"major\":"  \
    VAL_TO_STRING(MAJOR_VERSION)        \
    ",\"minor\":"                       \
    VAL_TO_STRING(MINOR_VERSION)        \
    "}\n"

/****************************************\
    COMMON functions
\****************************************/
static void delay_us(int time) {
    time *= 2ull;
    while(time--);
}

static void delay_ms(int time) {
    time *= 2000ull;
    while(time--);
}

/****************************************\
    UART functions
\****************************************/
char in_buff[256];
char pos;
char ready_flag;
int16_t input_values[8];

void (*uart_handler)(void) = NULL;

void UART2_IRQHandler(void) {
    uart_handler();
}

void start_handler(void) {
    static char state = 0;

    while(UART_GetFlagStatus(MDR_UART2, UART_FLAG_RXFE) != SET) {
        char c = UART_ReceiveData(MDR_UART2);

        if (c == '"') {
            if (state) {
                state = 0;
                in_buff[pos] = 0;
                ready_flag = 1;
            } else {
                ready_flag = 0;
                pos = 0;
                state = 1;
            }
            continue;
        }

        if (state) {
            in_buff[pos++] = c;
        }
    }
}

void input_handler(void) {
    char *value;

    while(UART_GetFlagStatus(MDR_UART2, UART_FLAG_RXFE) != SET) {
        in_buff[pos++] = UART_ReceiveData(MDR_UART2);
    }

    if (in_buff[pos - 1] != '\n') {
        return;
    }

    if (in_buff[0] == '{' && in_buff[1] == '"' && (value = strchr(in_buff, ':'))) {
        uint8_t num = strtol(in_buff + 2, NULL, 10);

        if (--num < 8) {
            input_values[num] = strtol(value, NULL, 10);
        }
    }

    pos = 0;
}

static void send_string(char *str) {
    int s = strlen(str);
    int i;

    for (i = 0; i < s; i++) {
        while (UART_GetFlagStatus(MDR_UART2, UART_FLAG_TXFF) == SET);
        UART_SendData(MDR_UART2, str[i]);
    }
}

static void init_uart(void) {
    UART_InitTypeDef UART_InitStruct;
    PORT_InitTypeDef PortInit;

    // настройка gpio МК для работы uart
    RST_CLK_PCLKcmd(RST_CLK_PCLK_PORTF, ENABLE);

    PortInit.PORT_PULL_UP = PORT_PULL_UP_OFF;
    PortInit.PORT_PULL_DOWN = PORT_PULL_DOWN_OFF;
    PortInit.PORT_PD_SHM = PORT_PD_SHM_OFF;
    PortInit.PORT_PD = PORT_PD_DRIVER;
    PortInit.PORT_GFEN = PORT_GFEN_OFF;
    PortInit.PORT_FUNC = PORT_FUNC_OVERRID;
    PortInit.PORT_SPEED = PORT_SPEED_MAXFAST;
    PortInit.PORT_MODE = PORT_MODE_DIGITAL;

    PortInit.PORT_OE = PORT_OE_OUT;
    PortInit.PORT_Pin = PORT_Pin_1;
    PORT_Init(MDR_PORTF, &PortInit);

    PortInit.PORT_OE = PORT_OE_IN;
    PortInit.PORT_Pin = PORT_Pin_0;
    PORT_Init(MDR_PORTF, &PortInit);

    // настройка контроллера uart
    RST_CLK_PCLKcmd(RST_CLK_PCLK_UART2, ENABLE);

    UART_BRGInit(MDR_UART2, UART_HCLKdiv1);

    // скорость передачи
    UART_InitStruct.UART_BaudRate = 230400;
    // длина передаваемых данных за раз
    UART_InitStruct.UART_WordLength = UART_WordLength8b;
    // количество стоп-битов
    UART_InitStruct.UART_StopBits = UART_StopBits2;
    // режим четности
    UART_InitStruct.UART_Parity = UART_Parity_No;
    // включение использования FIFO
    UART_InitStruct.UART_FIFOMode = UART_FIFO_ON;
    // включение передатчика и приемника
    UART_InitStruct.UART_HardwareFlowControl =
        UART_HardwareFlowControl_RXE | UART_HardwareFlowControl_TXE;

    UART_Init(MDR_UART2, &UART_InitStruct);

    // запуск uart
    UART_Cmd(MDR_UART2, ENABLE);

    pos = 0;
    ready_flag = 0;
    uart_handler = start_handler;
    UART_DMAConfig(MDR_UART2, UART_IT_FIFO_LVL_2words, UART_IT_FIFO_LVL_2words);
    NVIC_EnableIRQ(UART2_IRQn);
    UART_ITConfig(MDR_UART2, UART_IT_RX, ENABLE);
}


/****************************************\
    SPI functions
\****************************************/
static void init_spi(void) {
    PORT_InitTypeDef PortInit;
    SSP_InitTypeDef spi_initStruct;

    // инициализация тактирования
    RST_CLK_CPU_PLLconfig(RST_CLK_CPU_PLLsrcHSIdiv2, 0);
    RST_CLK_PCLKcmd(RST_CLK_PCLK_RST_CLK, ENABLE);

    // инициализация gpio МК для SPI
    RST_CLK_PCLKcmd(RST_CLK_PCLK_PORTD, ENABLE);

    PORT_DeInit(MDR_PORTD);
    PortInit.PORT_FUNC = PORT_FUNC_ALTER;
    PortInit.PORT_SPEED = PORT_SPEED_FAST;
    PortInit.PORT_MODE = PORT_MODE_DIGITAL;

    PortInit.PORT_OE = PORT_OE_OUT;
    PortInit.PORT_Pin = PORT_Pin_5 | PORT_Pin_6;
    PORT_Init(MDR_PORTD, &PortInit);

    PortInit.PORT_OE = PORT_OE_IN;
    PortInit.PORT_Pin = PORT_Pin_2;
    PORT_Init(MDR_PORTD, &PortInit);

    // инициализация SPI
    RST_CLK_PCLKcmd(RST_CLK_PCLK_SSP2, ENABLE);

    SSP_StructInit(&spi_initStruct);
    SSP_BRGInit(MDR_SSP2, SSP_HCLKdiv32);
    SSP_Init(MDR_SSP2, &spi_initStruct);
    SSP_Cmd(MDR_SSP2, ENABLE);
}

/****************************************\
    GPIO functions
\****************************************/
static void init_gpio(void) {
    RST_CLK_PCLKcmd(RST_CLK_PCLK_PORTB, ENABLE);
    RST_CLK_PCLKcmd(RST_CLK_PCLK_PORTC, ENABLE);
    RST_CLK_PCLKcmd(RST_CLK_PCLK_PORTD, ENABLE);

    PORT_InitTypeDef PortInit;
    PortInit.PORT_PULL_UP = PORT_PULL_UP_OFF;
    PortInit.PORT_PULL_DOWN = PORT_PULL_DOWN_OFF;
    PortInit.PORT_PD_SHM = PORT_PD_SHM_OFF;
    PortInit.PORT_PD = PORT_PD_DRIVER;
    PortInit.PORT_GFEN = PORT_GFEN_OFF;
    PortInit.PORT_FUNC = PORT_FUNC_PORT;
    PortInit.PORT_SPEED = PORT_SPEED_MAXFAST;
    PortInit.PORT_MODE = PORT_MODE_DIGITAL;
    PortInit.PORT_OE = PORT_OE_OUT;

    PortInit.PORT_Pin = PORT_Pin_0 | PORT_Pin_1 | PORT_Pin_3 | PORT_Pin_4;
    PORT_Init(MDR_PORTD, &PortInit);

    PortInit.PORT_Pin = PORT_Pin_0 | PORT_Pin_1 | PORT_Pin_2 | PORT_Pin_3 |
        PORT_Pin_4 | PORT_Pin_5 | PORT_Pin_6 | PORT_Pin_7 | PORT_Pin_8 |
        PORT_Pin_9 | PORT_Pin_10;
    PORT_Init(MDR_PORTB, &PortInit);

    PortInit.PORT_Pin = PORT_Pin_0 | PORT_Pin_1 | PORT_Pin_2 | PORT_Pin_3;
    PORT_Init(MDR_PORTC, &PortInit);
}

/****************************************\
    Module functions
\****************************************/
#define SPI_CLK_PORT    MDR_PORTD
#define SPI_CLK_NUM     PORT_Pin_5

#define SPI_RX_PORT     MDR_PORTD
#define SPI_RX_NUM      PORT_Pin_2

#define SPI_TX_PORT     MDR_PORTD
#define SPI_TX_NUM      PORT_Pin_6

#define SPI_SS_PORT     MDR_PORTB
#define SPI_SS_NUM      PORT_Pin_0

#define SPI_SELECT_PORT MDR_PORTD
#define SPI_SELECT_N_1  PORT_Pin_0
#define SPI_SELECT_N_2  PORT_Pin_3
#define SPI_SELECT_N_3  PORT_Pin_4

#define OBJECT_ID_PORT  MDR_PORTB
#define OBJECT_ID_N_1   PORT_Pin_2
#define OBJECT_ID_N_2   PORT_Pin_4
#define OBJECT_ID_N_3   PORT_Pin_6
#define OBJECT_ID_N_4   PORT_Pin_8

#define OBJECT_START_PORT   MDR_PORTB
#define OBJECT_START_NUM    PORT_Pin_10

#define OBJECT_NEXT_PORT    MDR_PORTC
#define OBJECT_NEXT_NUM     PORT_Pin_1

char module_get_object_id(void) {
    char n = 0;

    n |= PORT_ReadInputDataBit(OBJECT_ID_PORT, OBJECT_ID_N_1) << 0;
    n |= PORT_ReadInputDataBit(OBJECT_ID_PORT, OBJECT_ID_N_2) << 1;
    n |= PORT_ReadInputDataBit(OBJECT_ID_PORT, OBJECT_ID_N_3) << 2;
    n |= PORT_ReadInputDataBit(OBJECT_ID_PORT, OBJECT_ID_N_4) << 3;

    return n;
}

void module_set_object_num(int num) {
    uint32_t reg = PORT_ReadInputData(SPI_SELECT_PORT);

    if (num & (1<<0)) {
        reg |= SPI_SELECT_N_1;
    } else {
        reg &= ~(uint32_t)SPI_SELECT_N_1;
    }

    if (num & (1<<1)) {
        reg |= SPI_SELECT_N_2;
    } else {
        reg &= ~(uint32_t)SPI_SELECT_N_2;
    }

    if (num & (1<<2)) {
        reg |= SPI_SELECT_N_3;
    } else {
        reg &= ~(uint32_t)SPI_SELECT_N_3;
    }

    PORT_Write(SPI_SELECT_PORT, reg);
}

uint16_t module_exchange_object_val(uint16_t new) {
    PORT_SetBits(SPI_SS_PORT, SPI_SS_NUM);

    while (SSP_GetFlagStatus(MDR_SSP2, SSP_FLAG_TFE) != SET);
    while (SSP_GetFlagStatus(MDR_SSP2, SSP_FLAG_BSY) == SET);

    // start clock by sending 1 byte
    SSP_SendData(MDR_SSP2, new);

    while (SSP_GetFlagStatus(MDR_SSP2, SSP_FLAG_BSY) == SET);
    while (SSP_GetFlagStatus(MDR_SSP2, SSP_FLAG_RNE) != SET);

    PORT_ResetBits(SPI_SS_PORT, SPI_SS_NUM);

    return SSP_ReceiveData(MDR_SSP2);
}

void module_cnt_start(void) {
    PORT_SetBits(OBJECT_START_PORT, OBJECT_START_NUM);
    PORT_SetBits(OBJECT_NEXT_PORT, OBJECT_NEXT_NUM);
    delay_us(2);
    PORT_ResetBits(OBJECT_START_PORT, OBJECT_START_NUM);
}

void module_cnt_next(void) {
    PORT_ResetBits(OBJECT_NEXT_PORT, OBJECT_NEXT_NUM);
    delay_us(2);
    PORT_SetBits(OBJECT_NEXT_PORT, OBJECT_NEXT_NUM);
}

int module_object_has_input(char id) {
    return id % 2;
}

/****************************************\
    main function
\****************************************/
int main(void) {
    SystemInit();

    // инициализация используемых контроллеров и gpio
    init_uart();
    init_spi();
    init_gpio();

    // ожидание ответа от ПК
    while (1) {
        send_string(HELLO_STRING);
        delay_ms(2000);

        if (ready_flag) {
            if (!strcmp(in_buff, "Connected")) {
                break;
            }
            ready_flag = 0;
        }
    }

    // собрать информацию о подключенных датчиках
    char ids[8] = {0};
    char detected = 0;

    module_cnt_start();

    for (int i = 0; i < 8; i++) {
        input_values[i] = -1;
        ids[i] = module_get_object_id();

        if (!ids[i]) {
            continue;
        }

        detected++;
        module_cnt_next();
    }

    // отправка данных
    char resp[256] = {0};
    char curr = 0;
    curr = snprintf(resp + curr, 256 - curr, "{\"num\":%u,\"types\":[", detected);
    for (int i = 0; i < 8; i++) {
        curr += snprintf(resp + curr, 256 - curr, "%u,", ids[i]);
    }
    // убираем лишнюю запятую
    curr--;
    snprintf(resp + curr, 256 - curr, "]}\n");

    send_string(resp);

    // ожидание подтверждение от ПК
    while (1) {
        if (ready_flag) {
            if (!strcmp(in_buff, "Ready")) {
                break;
            }
            ready_flag = 0;
        }
    }

    // меняем обработчик
    pos = 0;
    uart_handler = input_handler;

    // цикл опроса
    while (1) {
        for (int i = 0; i < 8; i++) {
            uint16_t value;

            if (!ids[i]) {
                continue;
            }

            module_set_object_num(i);

            if (module_object_has_input(ids[i])) {
                if (input_values[i] != -1) {
                    module_exchange_object_val(input_values[i]);
                }
            } else {
                char data[32];
                value = module_exchange_object_val(0);
                snprintf(data, 32, "{\"%u\":%u}\n", i + 1, value);
                send_string(data);
            }
        }
    }
}
