#ifndef LCD_H
#define LCD_H
#include <stdint.h>
#define LCD_ADDRESS 0x27

void I2C1Init(void);
uint8_t I2C1_Master_Transmit(uint8_t slave_addr, uint8_t *data, uint8_t size);
void lcd_send_cmd(char cmd);
void lcd_send_data(char data);
void lcd_send_nibble(char nibble);
// Function to initialize the LCD
void lcd_init(void);
// Function to set the cursor position on the LCD
void lcdCursor(int row, int col);
// Function to display a string on the LCD
void lcdString(char *str);
// Function to display an integer on the LCD
void lcdInt(int num);\
// Function to display a float on the LCD
void lcdFloat(float num);

// Custom character functions
void lcdCreateChar(uint8_t location, const uint8_t charmap[8]);
void lcdInitCustomChars(void);

#endif // !LCD_H
