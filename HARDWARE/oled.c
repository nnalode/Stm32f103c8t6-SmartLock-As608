#include "oled.h"
#include "oled_font.H"
#include "main.h"
#include "Delay.h"

uint8_t framebuffer[OLED_WIDTH * OLED_HEIGHT / 8] = {0};

/**
 * @brief  向 OLED 写命令
 * @param  I2C_Command: 命令字节
 * @author yl+ds
 */
void WriteCmd(unsigned char I2C_Command) {
    HAL_I2C_Mem_Write(&hi2c1, OLED0561_ADD, COM, I2C_MEMADD_SIZE_8BIT, &I2C_Command, 1, 100);
}

/**
 * @brief  向 OLED 写数据
 * @param  I2C_Data: 数据字节
 * @author yl+ds
 */
void WriteDat(unsigned char I2C_Data) {
    HAL_I2C_Mem_Write(&hi2c1, OLED0561_ADD, DAT, I2C_MEMADD_SIZE_8BIT, &I2C_Data, 1, 100);
}

/**
 * @brief  OLED 初始化序列
 * @author yl+ds
 */
void OLED_Init(void) {
    HAL_Delay(100);
    WriteCmd(0xAE);
    WriteCmd(0x20);
    WriteCmd(0x10);
    WriteCmd(0xb0);
    WriteCmd(0xc8);
    WriteCmd(0x00);
    WriteCmd(0x10);
    WriteCmd(0x40);
    WriteCmd(0x81);
    WriteCmd(0xff);
    WriteCmd(0xa1);
    WriteCmd(0xa6);
    WriteCmd(0xa8);
    WriteCmd(0x3F);
    WriteCmd(0xa4);
    WriteCmd(0xd3);
    WriteCmd(0x00);
    WriteCmd(0xd5);
    WriteCmd(0xf0);
    WriteCmd(0xd9);
    WriteCmd(0x22);
    WriteCmd(0xda);
    WriteCmd(0x12);
    WriteCmd(0xdb);
    WriteCmd(0x20);
    WriteCmd(0x8d);
    WriteCmd(0x14);
    WriteCmd(0xaf);
    OLED_Fill(0x00);
    OLED_ON();
    OLED_Update();
}

/**
 * @brief  设置 OLED 坐标位置
 * @param  x: 列（0~127）
 * @param  y: 页（0~7）
 */
void OLED_SetPos(unsigned char x, unsigned char y) {
    WriteCmd(0xb0 + y);
    WriteCmd(((x & 0xf0) >> 4) | 0x10);
    WriteCmd((x & 0x0f) | 0x01);
}

/**
 * @brief  填充整个 framebuffer
 * @param  fill_Data: 填充值
 * @author yl+ds
 */
void OLED_Fill(unsigned char fill_Data) {
    for (uint16_t i = 0; i < sizeof(framebuffer); i++) {
        framebuffer[i] = fill_Data;
    }
}

/**
 * @brief  清屏（framebuffer 清零）
 */
void OLED_CLS(void) {
    OLED_Fill(0x00);
}

/**
 * @brief  开启 OLED 显示
 */
void OLED_ON(void) {
    WriteCmd(0X8D);
    WriteCmd(0X14);
    WriteCmd(0XAF);
}

/**
 * @brief  关闭 OLED 显示
 */
void OLED_OFF(void) {
    WriteCmd(0X8D);
    WriteCmd(0X10);
    WriteCmd(0XAE);
}

/**
 * @brief  在指定位置显示字符串
 * @param  x,y: 起始像素坐标
 * @param  ch: 字符串
 * @param  TextSize: 1:6x8  2:8x16
 * @author yl+ds
 */
void OLED_ShowStr(uint8_t x, uint8_t y, const char ch[], uint8_t TextSize) {
    uint8_t j = 0;
    uint8_t step_x = (TextSize == 1) ? 6 : 8;
    uint8_t step_y = (TextSize == 1) ? 8 : 16;
    while (ch[j] != '\0') {
        OLED_ShowChar(x, y, ch[j], TextSize);
        x += step_x;
        if (x + step_x > OLED_WIDTH) {
            x = 0;
            y += step_y;
        }
        j++;
    }
}

/**
 * @brief  显示 BMP 图片
 * @param  x0,y0,x1,y1: 区域
 * @param  BMP: 图像数据
 */
void OLED_DrawBMP(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1, unsigned char BMP[]) {
    unsigned int j = 0;
    unsigned char x, y;
    if (y1 % 8 == 0) y = y1 / 8;
    else y = y1 / 8 + 1;
    for (y = y0; y < y1; y++) {
        OLED_SetPos(x0, y);
        for (x = x0; x < x1; x++) {
            WriteDat(BMP[j++]);
        }
    }
}

/**
 * @brief  显示一个字符（6x8 或 8x16）
 * @param  x,y: 坐标
 * @param  chr: 字符
 * @param  Char_Size: 1:6x8  16:8x16
 * @author yl+ds
 */
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t Char_Size) {
    uint8_t c = chr - ' ';
    uint8_t i, j;
    if (Char_Size == 16) {
        uint8_t *pfont = (uint8_t*)&F8X16[c * 16];
        for (i = 0; i < 16; i++) {
            for (j = 0; j < 8; j++) {
                if (pfont[i] & (0x80 >> j)) {
                    OLED_SetPixel(x + j, y + i, 1);
                }
            }
        }
    } else {
        uint8_t *pfont = (uint8_t*)&F6x8[c][0];
        for (i = 0; i < 8; i++) {
            for (j = 0; j < 6; j++) {
                if (pfont[j] & (1 << i)) {
                    OLED_SetPixel(x + j, y + i, 1);
                }
            }
        }
    }
}

/**
 * @brief  幂函数（内部使用）
 */
u32 oled_pow(u8 m, u8 n) {
    u32 result = 1;
    while (n--) result *= m;
    return result;
}

/**
 * @brief  显示数字
 * @param  x,y: 坐标
 * @param  num: 数字
 * @param  len: 位数
 * @param  size2: 字体大小（1:6x8, 2:8x16? 实际按 size2 参数使用）
 */
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size2) {
    uint8_t t, temp;
    uint8_t enshow = 0;
    uint8_t step = (size2 == 1) ? 6 : 8;
    for (t = 0; t < len; t++) {
        temp = (num / oled_pow(10, len - t - 1)) % 10;
        if (enshow == 0 && t < (len - 1)) {
            if (temp == 0) {
                OLED_ShowChar(x + step * t, y, ' ', size2);
                continue;
            } else enshow = 1;
        }
        OLED_ShowChar(x + step * t, y, temp + '0', size2);
    }
}

/**
 * @brief  清除给定页的两行
 * @param  Line: 页号
 */
void OLED_ClearGivenChar(uint8_t Line) {
    unsigned char n, m;
    for (m = Line; m < Line + 2; m++) {
        WriteCmd(0xb0 + m);
        WriteCmd(0x00);
        WriteCmd(0x10);
        for (n = 0; n < 128; n++) {
            WriteDat(0x00);
        }
    }
}

/**
 * @brief  将 framebuffer 刷新到 OLED
 * @author yl+ds
 */
void OLED_Update(void) {
    uint8_t page, column;
    for (page = 0; page < 8; page++) {
        WriteCmd(0xB0 + page);
        WriteCmd(0x00);
        WriteCmd(0x10);
        for (column = 0; column < 128; column++) {
            WriteDat(framebuffer[page * 128 + column]);
        }
    }
}

/**
 * @brief  设置帧缓冲中的一个像素
 * @param  x,y: 坐标
 * @param  color: 0:灭  1:亮
 * @author yl+ds
 */
void OLED_SetPixel(uint8_t x, uint8_t y, uint8_t color) {
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) return;
    uint16_t page = y / 8;
    uint16_t col = x;
    uint8_t bit = y % 8;
    uint16_t idx = page * 128 + col;
    if (color) framebuffer[idx] |= (1 << bit);
    else framebuffer[idx] &= ~(1 << bit);
}