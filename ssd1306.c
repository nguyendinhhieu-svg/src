#include "ssd1306.h"

#define SDA_HIGH() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET)
#define SDA_LOW()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET)

#define SCL_HIGH() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET)
#define SCL_LOW()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET)


static void I2C_Delay(void)
{
    for (volatile uint16_t i = 0; i < 20; i++)
    {
        __NOP();
    }
}


static void I2C_Start(void)
{
    SDA_HIGH();
    SCL_HIGH();
    I2C_Delay();

    SDA_LOW();
    I2C_Delay();

    SCL_LOW();
    I2C_Delay();
}


static void I2C_Stop(void)
{
    SDA_LOW();
    SCL_LOW();
    I2C_Delay();

    SCL_HIGH();
    I2C_Delay();

    SDA_HIGH();
    I2C_Delay();
}


static void I2C_WriteByte(uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (byte & 0x80)
            SDA_HIGH();
        else
            SDA_LOW();

        I2C_Delay();

        SCL_HIGH();
        I2C_Delay();

        SCL_LOW();
        I2C_Delay();

        byte <<= 1;
    }

    /* ACK clock */
    SDA_HIGH();
    I2C_Delay();

    SCL_HIGH();
    I2C_Delay();

    SCL_LOW();
    I2C_Delay();
}


void SSD1306_WriteCommand(uint8_t cmd)
{
    I2C_Start();

    I2C_WriteByte(SSD1306_I2C_ADDR);

    I2C_WriteByte(0x00);

    I2C_WriteByte(cmd);

    I2C_Stop();
}


void SSD1306_WriteData(uint8_t data)
{
    I2C_Start();

    I2C_WriteByte(SSD1306_I2C_ADDR);

    I2C_WriteByte(0x40);

    I2C_WriteByte(data);

    I2C_Stop();
}


void SSD1306_Clear(void)
{
    for (uint8_t page = 0; page < 8; page++)
    {
        SSD1306_WriteCommand(0xB0 + page);

        SSD1306_WriteCommand(0x00);

        SSD1306_WriteCommand(0x10);

        I2C_Start();

        I2C_WriteByte(SSD1306_I2C_ADDR);

        I2C_WriteByte(0x40);

        for (uint8_t col = 0; col < 128; col++)
        {
            I2C_WriteByte(0x00);
        }

        I2C_Stop();
    }
}


void SSD1306_Fill(uint8_t data)
{
    for (uint8_t page = 0; page < 8; page++)
    {
        SSD1306_WriteCommand(0xB0 + page);

        SSD1306_WriteCommand(0x00);

        SSD1306_WriteCommand(0x10);

        I2C_Start();

        I2C_WriteByte(SSD1306_I2C_ADDR);

        I2C_WriteByte(0x40);

        for (uint8_t col = 0; col < 128; col++)
        {
            I2C_WriteByte(data);
        }

        I2C_Stop();
    }
}


void SSD1306_GotoXY(uint8_t x, uint8_t y)
{
    if (x >= 128)
        x = 0;

    if (y >= 8)
        y = 0;

    SSD1306_WriteCommand(0xB0 + y);

    SSD1306_WriteCommand(0x00 + (x & 0x0F));

    SSD1306_WriteCommand(0x10 + ((x >> 4) & 0x0F));
}


/* =====================================================
   FONT 5x7
   ===================================================== */

static const uint8_t Font5x7[][5] =
{
    {0x00,0x00,0x00,0x00,0x00},

    {0x3E,0x51,0x49,0x45,0x3E},
    {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},
    {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E},

    {0x7E,0x11,0x11,0x11,0x7E},
    {0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41},
    {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},
    {0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E},
    {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},
    {0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},
    {0x61,0x51,0x49,0x45,0x43}
};


static int GetFontIndex(char c)
{
    if (c == ' ')
        return 0;

    if (c >= '0' && c <= '9')
        return 1 + (c - '0');

    if (c >= 'A' && c <= 'Z')
        return 11 + (c - 'A');

    return 0;
}


void SSD1306_Puts(const char *str, uint8_t color)
{
    while (*str)
    {
        int index = GetFontIndex(*str);

        for (uint8_t col = 0; col < 5; col++)
        {
            uint8_t data = Font5x7[index][col];

            if (!color)
                data = ~data;

            SSD1306_WriteData(data);
        }

        SSD1306_WriteData(0x00);

        str++;
    }
}


void SSD1306_UpdateScreen(void)
{
    /* Dữ liệu được gửi trực tiếp đến OLED */
}


void SSD1306_Init(void)
{
    HAL_Delay(100);

    SSD1306_WriteCommand(0xAE);

    SSD1306_WriteCommand(0x20);
    SSD1306_WriteCommand(0x02);

    SSD1306_WriteCommand(0xB0);

    SSD1306_WriteCommand(0xC8);

    SSD1306_WriteCommand(0x00);
    SSD1306_WriteCommand(0x10);

    SSD1306_WriteCommand(0x40);

    SSD1306_WriteCommand(0x81);
    SSD1306_WriteCommand(0x7F);

    SSD1306_WriteCommand(0xA1);

    SSD1306_WriteCommand(0xA6);

    SSD1306_WriteCommand(0xA8);
    SSD1306_WriteCommand(0x3F);

    SSD1306_WriteCommand(0xD3);
    SSD1306_WriteCommand(0x00);

    SSD1306_WriteCommand(0xD5);
    SSD1306_WriteCommand(0x80);

    SSD1306_WriteCommand(0xD9);
    SSD1306_WriteCommand(0xF1);

    SSD1306_WriteCommand(0xDA);
    SSD1306_WriteCommand(0x12);

    SSD1306_WriteCommand(0xDB);
    SSD1306_WriteCommand(0x40);

    SSD1306_WriteCommand(0x8D);
    SSD1306_WriteCommand(0x14);

    SSD1306_WriteCommand(0xAF);

    SSD1306_Clear();
}