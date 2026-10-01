#include "lcd.h"
#include "stm32f401xc.h"
#include "sytick.h" // for delay_ms

// I2C1 initialization function
void I2C1Init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
  RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
  GPIOB->MODER &= ~(0xFU << (6 * 2));
  GPIOB->MODER |= (0xAU << (6 * 2));
  GPIOB->OSPEEDR |= (0xFU << (6 * 2));
  GPIOB->OTYPER |= (0x3U << 6);
  GPIOB->PUPDR &= ~(0xFU << (6 * 2));
  GPIOB->AFR[0] &= ~(0xFFU << GPIO_AFRL_AFSEL6_Pos);
  GPIOB->AFR[0] |= (0x44U << GPIO_AFRL_AFSEL6_Pos);
  I2C1->CR1 |= I2C_CR1_SWRST;
  I2C1->CR1 &= ~I2C_CR1_SWRST;

  I2C1->CR2 &= ~I2C_CR2_FREQ; // Clear FREQ bits
  I2C1->CR2 |= 16;            // 16 MHz

  // Configure Clock Control Register (CCR) for 100 kHz Standard Mode
  // Thigh = Tlow = 5000ns (for 100 kHz)
  // CCR = PCLK1 / (2 * I2C_Speed) -> 16,000,000 / (2 * 100,000) = 80
  I2C1->CCR &= ~I2C_CCR_FS;  // 0: Standard Mode
  I2C1->CCR &= ~I2C_CCR_CCR; // Clear CCR bits
  I2C1->CCR |= 80;           // Set CCR to 80

  // Configure Maximum Rise Time (TRISE)
  // In Standard Mode, max rise time is 1000ns.
  // TRISE = (Max_Rise_Time / T_PCLK1) + 1 -> (1000ns / 62.5ns) + 1 = 16 + 1 =
  // 17
  I2C1->TRISE = 17;

  // Enable I2C1 Peripheral
  I2C1->CR1 |= I2C_CR1_PE;
}

uint8_t I2C1_Master_Transmit(uint8_t slave_addr, uint8_t *data, uint8_t size) {
  // 1. Generate START condition
  I2C1->CR1 |= I2C_CR1_START;
  while (!(I2C1->SR1 & I2C_SR1_SB))
    ;

  // 2. Send Slave Address (shifted left by 1, with LSB = 0 for Write)
  I2C1->DR = (slave_addr << 1);
  while (!(I2C1->SR1 & I2C_SR1_ADDR))
    ;

  // Clear ADDR flag sequence
  volatile uint32_t temp = I2C1->SR1;
  temp = I2C1->SR2;
  (void)temp;

  // 3. Transmit Data Sequence
  for (uint8_t i = 0; i < size; i++) {
    while (!(I2C1->SR1 & I2C_SR1_TXE))
      ;
    I2C1->DR = data[i];
  }

  // Wait for the final byte to leave the shift register
  while (!(I2C1->SR1 & I2C_SR1_BTF))
    ;

  // 4. Generate STOP condition
  I2C1->CR1 |= I2C_CR1_STOP;

  return 0;
}

void lcd_send_cmd(char cmd) {
  char data_u = (cmd & 0xF0);
  char data_l = ((cmd << 4) & 0xF0);
  uint8_t d[1];

  // Upper Nibble
  d[0] = data_u | 0x0C;
  I2C1_Master_Transmit(LCD_ADDRESS, d, 1); // EN=1, RS=0
  delay_us(2);                             // Let data settle
  d[0] = data_u | 0x08;
  I2C1_Master_Transmit(LCD_ADDRESS, d, 1); // EN=0, RS=0
  delay_us(2);

  // Lower Nibble
  d[0] = data_l | 0x0C;
  I2C1_Master_Transmit(LCD_ADDRESS, d, 1); // EN=1, RS=0
  delay_us(2);
  d[0] = data_l | 0x08;
  I2C1_Master_Transmit(LCD_ADDRESS, d, 1); // EN=0, RS=0
  delay_us(40); // Standard command execution wait time
}

void lcd_send_data(char data) {
  char data_u = (data & 0xF0);
  char data_l = ((data << 4) & 0xF0);
  uint8_t d[1];

  // Upper Nibble
  d[0] = data_u | 0x0D;
  I2C1_Master_Transmit(LCD_ADDRESS, d, 1); // EN=1, RS=1
  delay_us(2);
  d[0] = data_u | 0x09;
  I2C1_Master_Transmit(LCD_ADDRESS, d, 1); // EN=0, RS=1
  delay_us(2);

  // Lower Nibble
  d[0] = data_l | 0x0D;
  I2C1_Master_Transmit(LCD_ADDRESS, d, 1); // EN=1, RS=1
  delay_us(2);
  d[0] = data_l | 0x09;
  I2C1_Master_Transmit(LCD_ADDRESS, d, 1); // EN=0, RS=1
  delay_us(40);
}

void lcd_send_nibble(char nibble) {
  uint8_t d[1];
  d[0] = (nibble & 0xF0) | 0x0C; // EN=1, RS=0
  I2C1_Master_Transmit(LCD_ADDRESS, d, 1);
  delay_us(2);
  d[0] = (nibble & 0xF0) | 0x08; // EN=0, RS=0
  I2C1_Master_Transmit(LCD_ADDRESS, d, 1);
  delay_us(40);
}

void lcd_init(void) {
  delay_ms(50);

  // Force 8-bit mode 3 times to sync up state machines
  lcd_send_nibble(0x30);
  delay_ms(5);
  lcd_send_nibble(0x30);
  delay_us(150);
  lcd_send_nibble(0x30);
  delay_ms(10);

  // Set to 4-bit mode configuration
  lcd_send_nibble(0x20);
  delay_ms(10);
  lcd_send_cmd(0x28); // 4-bit, 2-line, 5×8 font
  delay_ms(1);
  lcd_send_cmd(0x08); // display OFF
  delay_ms(1);
  lcd_send_cmd(0x01); // clear display
  delay_ms(2);
  lcd_send_cmd(0x06); // entry mode
  delay_ms(1);
  lcd_send_cmd(0x0C); // display ON, cursor OFF
  delay_ms(1);

  lcdInitCustomChars();
}

void lcdCreateChar(uint8_t location, const uint8_t charmap[8]) {
  location &= 0x07;
  lcd_send_cmd(0x40 | (location << 3));
  for (int i = 0; i < 8; i++) {
    lcd_send_data(charmap[i]);
  }
}

void lcdInitCustomChars(void) {
  // 0: Initial tip character for scale bar (centered full line)
  static const uint8_t char_tip_init[8] = {
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04
  };

  // 1: 1 vertical line (column 0)
  static const uint8_t char_1_line[8] = {
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10
  };

  // 2: 2 vertical lines (columns 0, 1)
  static const uint8_t char_2_lines[8] = {
    0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18
  };

  // 3: 3 vertical lines (columns 0, 1, 2)
  static const uint8_t char_3_lines[8] = {
    0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C
  };

  // 4: 4 vertical lines (columns 0, 1, 2, 3)
  static const uint8_t char_4_lines[8] = {
    0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E
  };

  // 5: 5 vertical lines (columns 0, 1, 2, 3, 4: |||||)
  static const uint8_t char_5_lines[8] = {
    0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F
  };

  // 6: Scale Even Cell (ticks at 1, 3 with 1-pixel uniform spacing)
  static const uint8_t char_scale_even[8] = {
    0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };

  // 7: Scale Odd Cell (ticks at 0, 2; half-line at 4 with 1-pixel uniform spacing)
  static const uint8_t char_scale_odd[8] = {
    0x15, 0x15, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00
  };

  lcdCreateChar(0, char_tip_init);
  lcdCreateChar(1, char_1_line);
  lcdCreateChar(2, char_2_lines);
  lcdCreateChar(3, char_3_lines);
  lcdCreateChar(4, char_4_lines);
  lcdCreateChar(5, char_5_lines);
  lcdCreateChar(6, char_scale_even);
  lcdCreateChar(7, char_scale_odd);

  // Return LCD to DDRAM mode
  lcdCursor(0, 0);
}

void lcdCursor(int row, int col) {
  switch (row) {
  case 0:
    col |= 0x80;
    break; // DDRAM row 0 starts at 0x00
  case 1:
    col |= 0xC0;
    break; // DDRAM row 1 starts at 0x40
  }
  lcd_send_cmd(col);
}

void lcdString(char *str) {
  while (*str)
    lcd_send_data(*str++);
}

void lcdInt(int num) {
  char buf[12];
  int i = 0;

  if (num == 0) {
    lcdString("0");
    return;
  }

  int isNegative = 0;
  long long n = num;
  if (n < 0) {
    isNegative = 1;
    n = -n;
  }

  while (n > 0) {
    buf[i++] = (n % 10) + '0';
    n /= 10;
  }

  if (isNegative) {
    buf[i++] = '-';
  }

  for (int j = i - 1; j >= 0; j--) {
  }

  char reversed[13];
  int r = 0;
  for (int j = i - 1; j >= 0; j--) {
    reversed[r++] = buf[j];
  }
  reversed[r] = '\0';

  lcdString(reversed);
}

void lcdFloat(float num) {
  if (num < 0) {
    lcdString("-");
    num = -num;
  }

  int intPart = (int)num;
  float frac = num - (float)intPart;
  int fracPart = (int)(frac * 1000.0f + 0.0005f);

  if (fracPart >= 1000) {
    intPart += 1;
    fracPart = 0;
  }

  lcdInt(intPart);
  lcdString(".");
  if (fracPart < 10) {
    lcdString("00");
  } else if (fracPart < 100) {
    lcdString("0");
  }
  lcdInt(fracPart);
}
