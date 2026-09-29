#ifndef LCD_H
#define LCD_H
#include <stdint.h>
#define LCD_ADDRESS 0x27

void I2C1Init(void);
uint8_t I2C1_Master_Transmit(uint8_t slave_addr, uint8_t *data, uint8_t size);
void lcd_send_cmd(char cmd);
void lcd_send_data(char data);
void lcd_send_nibble(char nibble);
void lcd_init(void);
void lcdCursor(int row, int col);
void lcdString(char *str);
void lcdInt(int num);
void lcdFloat(float num);
#endif // !LCD_H
