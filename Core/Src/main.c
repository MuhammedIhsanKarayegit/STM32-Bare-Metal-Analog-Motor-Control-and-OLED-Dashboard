#include "stm32f1xx.h"

#define OLED_ADDR 0x78

// --- FONT TABLOSU ---
const uint8_t Font[][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x7C, 0x12, 0x11, 0x12, 0x7C}, // A
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P (Power için)
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C (Yüzde sembolü niyetine yuvarlak)
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x00, 0x00, 0x00, 0x00}  // Space
};

void delay_ms(uint32_t time) { for(uint32_t i=0; i<time*4000; i++); }

// --- I2C & OLED (Aynı Kütüphane) ---
void I2C1_Init(void) {
    RCC->APB2ENR |= (1<<3) | (1<<0); RCC->APB1ENR |= (1<<21);
    GPIOB->CRL &= ~(0xFF << 24); GPIOB->CRL |= (0xEE << 24);
    I2C1->CR1 |= (1<<15); I2C1->CR1 &= ~(1<<15);
    I2C1->CR2 = 8; I2C1->CCR = 40; I2C1->TRISE = 9; I2C1->CR1 |= (1<<0);
}
void I2C_Send(uint8_t type, uint8_t data) {
    I2C1->CR1 |= (1<<8); while (!(I2C1->SR1 & (1<<0)));
    I2C1->DR = OLED_ADDR; while (!(I2C1->SR1 & (1<<1)));
    uint8_t temp = I2C1->SR1 | I2C1->SR2;
    I2C1->DR = type; while (!(I2C1->SR1 & (1<<2)));
    I2C1->DR = data; while (!(I2C1->SR1 & (1<<2)));
    I2C1->CR1 |= (1<<9);
}
#define CMD(c) I2C_Send(0x00, c)
#define DATA(d) I2C_Send(0x40, d)

void OLED_Init(void) {
    delay_ms(100);
    CMD(0xAE); CMD(0x20); CMD(0x02); CMD(0xB0); CMD(0xC8);
    CMD(0x00); CMD(0x10); CMD(0x40); CMD(0x81); CMD(0xFF);
    CMD(0xA1); CMD(0xA6); CMD(0xA8); CMD(0x1F); CMD(0xD3);
    CMD(0x00); CMD(0xD5); CMD(0xF0); CMD(0xD9); CMD(0x22);
    CMD(0xDA); CMD(0x02); CMD(0xDB); CMD(0x20); CMD(0x8D);
    CMD(0x14); CMD(0xAF);
}
void OLED_SetCursor(uint8_t page, uint8_t col) {
    CMD(0xB0 + page); CMD(0x00 | (col & 0x0F)); CMD(0x10 | (col >> 4));
}
void OLED_PrintChar(uint8_t charIndex) {
    for(int i=0; i<5; i++) DATA(Font[charIndex][i]); DATA(0x00);
}
void OLED_PrintNumber(int num) {
    char buffer[4];
    buffer[0] = (num / 1000) % 10; buffer[1] = (num / 100) % 10;
    buffer[2] = (num / 10) % 10; buffer[3] = num % 10;
    for(int i=0; i<4; i++) OLED_PrintChar(buffer[i]);
}
void OLED_Clear(void) {
    CMD(0x20); CMD(0x00); CMD(0x21); CMD(0); CMD(127); CMD(0x22); CMD(0); CMD(3);
    for(int i=0; i<512; i++) DATA(0x00);
}

// --- ADC (Potansiyometre) ---
void ADC1_Init(void) {
    RCC->APB2ENR |= (1 << 9) | (1 << 2);
    GPIOA->CRL &= ~(0xF << 0);
    RCC->CFGR |= (2 << 14);
    ADC1->CR2 |= (1 << 0); delay_ms(2);
    ADC1->CR2 |= (1 << 3); while(ADC1->CR2 & (1 << 3));
    ADC1->CR2 |= (1 << 2); while(ADC1->CR2 & (1 << 2));
}
uint16_t ADC_Read(void) {
    ADC1->SQR3 = 0; ADC1->CR2 |= (1 << 0);
    while(!(ADC1->SR & (1 << 1))); return ADC1->DR;
}

// --- YENİ KONU: PWM MOTOR KONTROLÜ (TIM1 - PA8) ---
void TIM1_PWM_Init(void) {
    // 1. Clock Aç
    RCC->APB2ENR |= (1 << 11); // TIM1 Clock Enable
    RCC->APB2ENR |= (1 << 2);  // GPIOA Clock Enable

    // 2. GPIO Ayarı (PA8 -> Alternate Function Push-Pull)
    // PA8, CRH register'ının en başındadır (Bit 0-3)
    GPIOA->CRH &= ~(0xF << 0); // Temizle
    // Mode=11 (50MHz Output), CNF=10 (Alt. Func. Push-Pull) -> 1011 = 0xB
    GPIOA->CRH |= (0xB << 0);

    // 3. Timer Ayarları
    // Prescaler (Hız bölücü). İşlemci 8MHz çalışıyor (varsayılan).
    // Biz PWM frekansını duyulmayacak kadar hızlı yapalım (örn: 1 kHz civarı)
    // PWM Frekansı = 8MHz / ((PSC+1) * (ARR+1))
    TIM1->PSC = 7; // 8'e böl -> 1 MHz sayma hızı

    // Auto-Reload Register (Periyot).
    // Potansiyometreden 0-4095 okuyoruz.
    // ARR'yi 4095 yaparsak, ADC değerini doğrudan motora verebiliriz! Matematik yok!
    TIM1->ARR = 4095;

    // 4. PWM Modu Ayarı (Channel 1)
    // CCMR1 register'ında OC1M bitlerini 110 (PWM Mode 1) yapıyoruz.
    TIM1->CCMR1 |= (6 << 4);
    TIM1->CCMR1 |= (1 << 3); // OC1PE (Preload Enable)

    // 5. Çıkışı Aktif Et
    TIM1->CCER |= (1 << 0); // CC1E (Capture/Compare 1 Enable)

    // **ÖNEMLİ:** TIM1 gelişmiş bir timer'dır, "Ana Şalteri" (MOE) vardır.
    TIM1->BDTR |= (1 << 15); // MOE (Main Output Enable)

    // 6. Sayacı Başlat
    TIM1->CR1 |= (1 << 0); // CEN (Counter Enable)
}

int main(void) {
    I2C1_Init();
    OLED_Init();
    OLED_Clear();
    ADC1_Init();
    TIM1_PWM_Init(); // Motor sürücüsünü başlat

    OLED_SetCursor(0, 0);
    OLED_PrintChar(11); // P (Power)
    OLED_PrintChar(13); // :

    int adcVal = 0;

    while(1) {
        // 1. Potansiyometreyi Oku (0 - 4095 arası)
        adcVal = ADC_Read();

        // 2. Motora Gönder (CCR register'ı Duty Cycle'ı belirler)
        // Eğer ARR = 4095 ise, CCR = 2048 olduğunda motor %50 hızla döner.
        TIM1->CCR1 = adcVal;

        // 3. Ekrana Yaz
        OLED_SetCursor(0, 30);
        OLED_PrintNumber(adcVal);

        // Basit bir bar yapalım (Görsel Şov)
        // 4095'i 128'e böl (Ekran genişliği) -> Yaklaşık 32
        int barWidth = adcVal / 32;
        OLED_SetCursor(2, 0);
        for(int i=0; i<128; i++) {
            if(i < barWidth) DATA(0xFF); // Dolu
            else DATA(0x00); // Boş
        }

        delay_ms(50);
    }
}
