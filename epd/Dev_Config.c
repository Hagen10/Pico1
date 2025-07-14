/*****************************************************************************
* | File      	:   DEV_Config.c
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :
*----------------
* |	This version:   V3.0
* | Date        :   2019-07-31
* | Info        :   
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of theex Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#include "Dev_Config.h"

/**
 * GPIO
**/
int EPD_RST_PIN;
int EPD_DC_PIN;
int EPD_CS_PIN;
int EPD_BUSY_PIN;
int EPD_PWR_PIN;
int EPD_MOSI_PIN;
int EPD_SCLK_PIN;
/**
 * SPI
**/
int BAUDRATE = 10000000;
spi_inst_t *spi;

/**
 * GPIO read and write
**/
void DEV_Digital_Write(UWORD pin, UBYTE value)
{
	gpio_put(pin, value);
}

UBYTE DEV_Digital_Read(UWORD pin)
{
	return gpio_get(pin);
}

/**
 * SPI
**/
void DEV_SPI_WriteByte(uint8_t value)
{
	spi_write_blocking(spi, &value, 1);
}

void DEV_SPI_Write_nByte(uint8_t *pData, uint32_t len)
{
	spi_write_blocking(spi, pData, len);
}

/**
 * GPIO Mode
**/
void DEV_GPIO_Mode(UWORD Pin, UWORD Mode)
{
	switch (Mode) {
		case 1: 
			gpio_pull_up(Pin);
			break;
		case 0:
			gpio_pull_down(Pin);
			break;
	}
}

/**
 * delay ms
**/
void DEV_Delay_ms(UDOUBLE ms)
{
	sleep_ms(ms);
}

void DEV_GPIO_Init(void)
{
	EPD_RST_PIN     = 12;
	EPD_DC_PIN      = 8;
	EPD_CS_PIN      = 9;
    EPD_PWR_PIN     = 7;
	EPD_BUSY_PIN    = 13;
    EPD_MOSI_PIN    = 11;
	EPD_SCLK_PIN    = 10;

	spi = spi1;

	// Initialize GPIOs
    gpio_init(EPD_CS_PIN); gpio_set_dir(EPD_CS_PIN, GPIO_OUT);
    gpio_init(EPD_DC_PIN); gpio_set_dir(EPD_DC_PIN, GPIO_OUT);
    gpio_init(EPD_RST_PIN); gpio_set_dir(EPD_RST_PIN, GPIO_OUT);
    gpio_init(EPD_PWR_PIN); gpio_set_dir(EPD_PWR_PIN, GPIO_OUT);
    gpio_init(EPD_BUSY_PIN); gpio_set_dir(EPD_BUSY_PIN, GPIO_IN);

    // Initialize SPI
    spi_init(spi, BAUDRATE);
    gpio_set_function(EPD_SCLK_PIN, GPIO_FUNC_SPI); // SCK
    gpio_set_function(EPD_MOSI_PIN, GPIO_FUNC_SPI); // MOSI

	DEV_GPIO_Mode(EPD_BUSY_PIN, 0);
	DEV_GPIO_Mode(EPD_RST_PIN, 1);
	DEV_GPIO_Mode(EPD_DC_PIN, 1);
	DEV_GPIO_Mode(EPD_CS_PIN, 1);
	DEV_GPIO_Mode(EPD_PWR_PIN, 1);

	UBYTE state = DEV_Digital_Read(EPD_BUSY_PIN);

	printf("INITIATED EVERYTHING! -- %d\n", state);

	DEV_Digital_Write(EPD_CS_PIN, 1);
	DEV_Digital_Write(EPD_PWR_PIN, 1);    
}

void DEV_SPI_SendnData(UBYTE *Reg)
{
    UDOUBLE size;
    size = sizeof(Reg);
    for(UDOUBLE i=0 ; i<size ; i++)
    {
        DEV_SPI_SendData(Reg[i]);
    }
}

void DEV_SPI_SendData(UBYTE Reg)
{
	UBYTE i,j=Reg;
	DEV_GPIO_Mode(EPD_MOSI_PIN, 1);
	DEV_Digital_Write(EPD_CS_PIN, 0);
	for(i = 0; i<8; i++)
    {
        DEV_Digital_Write(EPD_SCLK_PIN, 0);     
        if (j & 0x80)
        {
            DEV_Digital_Write(EPD_MOSI_PIN, 1);
        }
        else
        {
            DEV_Digital_Write(EPD_MOSI_PIN, 0);
        }
        
        DEV_Digital_Write(EPD_SCLK_PIN, 1);
        j = j << 1;
    }
	DEV_Digital_Write(EPD_SCLK_PIN, 0);
	DEV_Digital_Write(EPD_CS_PIN, 1);
}

UBYTE DEV_SPI_ReadData()
{
	UBYTE i,j=0xff;
	DEV_GPIO_Mode(EPD_MOSI_PIN, 0);
	DEV_Digital_Write(EPD_CS_PIN, 0);
	for(i = 0; i<8; i++)
	{
		DEV_Digital_Write(EPD_SCLK_PIN, 0);
		j = j << 1;
		if (DEV_Digital_Read(EPD_MOSI_PIN))
		{
				j = j | 0x01;
		}
		else
		{
				j= j & 0xfe;
		}
		DEV_Digital_Write(EPD_SCLK_PIN, 1);
	}
	DEV_Digital_Write(EPD_SCLK_PIN, 0);
	DEV_Digital_Write(EPD_CS_PIN, 1);
	return j;
}

/******************************************************************************
function:	Module Initialize, the library and initialize the pins, SPI protocol
parameter:
Info:
******************************************************************************/
UBYTE DEV_Module_Init(void)
{
	return 0;
}

/******************************************************************************
function:	Module exits, closes SPI and BCM2835 library
parameter:
Info:
******************************************************************************/
void DEV_Module_Exit(void)
{
}