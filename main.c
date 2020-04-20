#include "MDR32Fx.h"

#include "string.h"
#include "stdio.h"

#include "MDR32F9Qx_rst_clk.h"
#include "MDR32F9Qx_port.h"
#include "MDR32F9Qx_uart.h"
#include "MDR32F9Qx_ssp.h"

void send_srting(char *str) {
	int s = strlen(str);
	int i;

	for (i = 0; i < s; i++) {
		while (UART_GetFlagStatus (MDR_UART2, UART_FLAG_TXFE)!= SET);
		UART_SendData(MDR_UART2, str[i]);
	}
}

void init_uart(void) {
	UART_InitTypeDef UART_InitStruct;
	PORT_InitTypeDef PortInit;

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

  RST_CLK_PCLKcmd(RST_CLK_PCLK_UART2, ENABLE);

	UART_BRGInit(MDR_UART2, UART_HCLKdiv1);

	UART_InitStruct.UART_BaudRate = 230400;
	UART_InitStruct.UART_WordLength = UART_WordLength8b;
	UART_InitStruct.UART_StopBits = UART_StopBits2;
	UART_InitStruct.UART_Parity = UART_Parity_No;
	UART_InitStruct.UART_FIFOMode = UART_FIFO_OFF;
	UART_InitStruct.UART_HardwareFlowControl =
		UART_HardwareFlowControl_RXE | UART_HardwareFlowControl_TXE;

	UART_Init(MDR_UART2, &UART_InitStruct);

	UART_Cmd(MDR_UART2, ENABLE);
}

void init_spi(void) {
	RST_CLK_CPU_PLLconfig(RST_CLK_CPU_PLLsrcHSIdiv2,0);
  RST_CLK_PCLKcmd((RST_CLK_PCLK_RST_CLK),ENABLE);

	SSP_InitTypeDef spi_initStruct;

	PORT_InitTypeDef PortInit;

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

	RST_CLK_PCLKcmd(RST_CLK_PCLK_SSP2, ENABLE);

	SSP_StructInit(&spi_initStruct);
	SSP_BRGInit(MDR_SSP2, SSP_HCLKdiv32);
	SSP_Init(MDR_SSP2, &spi_initStruct);
	SSP_Cmd(MDR_SSP2, ENABLE);
}

int main(void) {
	SystemInit();
	init_uart();
	init_spi();

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
  PortInit.PORT_Pin = PORT_Pin_0|PORT_Pin_1|PORT_Pin_3|PORT_Pin_4;
	PORT_Init(MDR_PORTD, &PortInit);

	PortInit.PORT_Pin = PORT_Pin_0|PORT_Pin_1|PORT_Pin_2|PORT_Pin_3|PORT_Pin_4|PORT_Pin_5|PORT_Pin_6|PORT_Pin_7|PORT_Pin_8|PORT_Pin_9|PORT_Pin_10;
	PORT_Init(MDR_PORTB, &PortInit);

	PortInit.PORT_Pin = PORT_Pin_0|PORT_Pin_1|PORT_Pin_2|PORT_Pin_3;
	PORT_Init(MDR_PORTC, &PortInit);

	send_srting("Started\n");
	char a = 0;
	char b;
	char c;
	while (1) {
		//PORT_Write(MDR_PORTD, a % 2? 0xffff:0);
		//PORT_Write(MDR_PORTB, a % 2? 0xffff:0);
		//PORT_Write(MDR_PORTC, a % 2? 0xffff:0);

		PORT_Write(MDR_PORTD, 8);

		for (int i = 0; i < 10000ull; i++);

		while (SSP_GetFlagStatus(MDR_SSP2, SSP_FLAG_TFE)!= SET);
		SSP_SendData(MDR_SSP2, a);

		PORT_Write(MDR_PORTD, 0);

		while (SSP_GetFlagStatus(MDR_SSP2, SSP_FLAG_RNE)!= SET);
		b = SSP_ReceiveData(MDR_SSP2);

		if (b != a + 9) {
			c = 0;
		} else {
			c = 1;
		}

		char buf[50];
		snprintf(buf,50, "%3u Hello world! %3u\n", a++, c);
		send_srting(buf);

		for (int i = 0; i < 500000ull; i++);
	}
}
