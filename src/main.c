/**
 * ============================================================================
 * Project    : PWM-Based Smart Cooling System
 * Target     : NXP LPC1768 (ARM Cortex-M3)
 * Peripherals: 12-bit ADC (AD0.1), PWM1 (PWM1.5), GPIO (HD44780 16x2 LCD)
 * Sensors    : LM35 Precision Centigrade Temperature Sensor
 * Actuators  : 12V / 5V Brushless DC Fan via Logic-Level MOSFET Driver
 * ============================================================================
 */

#include <LPC17xx.h>
#include <stdio.h>

/* ============================================================================
 * LCD PIN DEFINITIONS (8-bit Mode on GPIO Port 0)
 * ============================================================================
 * RS : P0.10
 * EN : P0.11
 * D0 - D7 : P0.15 - P0.22
 */
#define LCD_RS          (1 << 10)
#define LCD_EN          (1 << 11)
#define LCD_DATA_MASK   (0xFF << 15)

/* ============================================================================
 * ADC BIT DEFINITIONS (AD0.1 on P0.24)
 * ============================================================================
 */
#define SBIT_START      24
#define SBIT_DONE       31
#define SBIT_RESULT     4
#define SBIT_CLCKDIV    8
#define SBIT_PDN        21

/* ============================================================================
 * FAN SPEED PROFILES (PWM MATCH VALUES FOR MR0 = 1000)
 * ============================================================================
 */
#define PWM_PERIOD      1000
#define FAN_DUTY_LOW    250    /* 25% Duty Cycle */
#define FAN_DUTY_MED    550    /* 55% Duty Cycle */
#define FAN_DUTY_HIGH   900    /* 90% Duty Cycle */

/* Finite State Machine (FSM) States for Fan Speed with Hysteresis */
typedef enum {
    FAN_LEVEL_1 = 0,  /* Low Speed (25%) */
    FAN_LEVEL_2,      /* Medium Speed (55%) */
    FAN_LEVEL_3       /* High Speed (90%) */
} FanSpeedLevel_t;

/* ============================================================================
 * FUNCTION DECLARATIONS
 * ============================================================================
 */
void LCD_Init(void);
void LCD_Command(unsigned char cmd);
void LCD_Data(unsigned char data);
void LCD_PrintString(const char *str);

void ADC_Init(void);
float ADC_ReadTemperatureRaw(void);
float ADC_ReadTemperatureFiltered(void);

void PWM_Init(void);
void PWM_SetFanSpeed(int duty);

void Delay_Simple(void);
void Delay_Ms(int ms);

/* ============================================================================
 * MAIN APPLICATION
 * ============================================================================
 */
int main(void)
{
    float temperature = 0.0f;
    FanSpeedLevel_t current_level = FAN_LEVEL_1;
    char lcd_buffer[20];
    const char *status_str = "L1 (25%)";

    /* Initialize Hardware Peripherals */
    ADC_Init();
    LCD_Init();
    PWM_Init();

    /* Initial splash display */
    LCD_Command(0x80); /* Row 1, Column 0 */
    LCD_PrintString("SMART COOLING");

    /* Default fan start: Low Speed (25%) */
    PWM_SetFanSpeed(FAN_DUTY_LOW);

    while (1)
    {
        /* Sample temperature with 8-point moving average noise filter */
        temperature = ADC_ReadTemperatureFiltered();

        /*
         * ====================================================================
         * HYSTERESIS THERMAL CONTROL LOGIC (Prevents Rapid Fan Chatter)
         * ====================================================================
         * Thresholds:
         * - L1 -> L2 transition at >= 25.5 deg C | L2 -> L1 at < 24.5 deg C
         * - L2 -> L3 transition at >= 30.5 deg C | L3 -> L2 at < 29.5 deg C
         */
        switch (current_level)
        {
            case FAN_LEVEL_1:
                if (temperature >= 25.5f) {
                    current_level = FAN_LEVEL_2;
                    PWM_SetFanSpeed(FAN_DUTY_MED);
                    status_str = "L2 (55%)";
                }
                break;

            case FAN_LEVEL_2:
                if (temperature < 24.5f) {
                    current_level = FAN_LEVEL_1;
                    PWM_SetFanSpeed(FAN_DUTY_LOW);
                    status_str = "L1 (25%)";
                } else if (temperature >= 30.5f) {
                    current_level = FAN_LEVEL_3;
                    PWM_SetFanSpeed(FAN_DUTY_HIGH);
                    status_str = "L3 (90%)";
                }
                break;

            case FAN_LEVEL_3:
                if (temperature < 29.5f) {
                    current_level = FAN_LEVEL_2;
                    PWM_SetFanSpeed(FAN_DUTY_MED);
                    status_str = "L2 (55%)";
                }
                break;
        }

        /*
         * ====================================================================
         * LCD STATUS UPDATE
         * ====================================================================
         */
        LCD_Command(0xC0); /* Row 2, Column 0 */

        /* Fixed-point integer formatting avoids soft-FPU runtime overhead */
        int temp_int  = (int)temperature;
        int temp_frac = (int)((temperature - (float)temp_int) * 10.0f);
        if (temp_frac < 0) temp_frac = 0;

        sprintf(lcd_buffer, "T:%2d.%1dC %s ", temp_int, temp_frac, status_str);
        LCD_PrintString(lcd_buffer);

        Delay_Ms(300);
    }
}

/* ============================================================================
 * LCD FUNCTIONS (HD44780 Driver)
 * ============================================================================
 */
void LCD_Init(void)
{
    /* Configure P0.10 (RS), P0.11 (EN), and P0.15-P0.22 (D0-D7) as GPIO outputs */
    LPC_GPIO0->FIODIR |= LCD_RS | LCD_EN | LCD_DATA_MASK;

    Delay_Ms(20); /* Wait for LCD internal reset */

    LCD_Command(0x38); /* 8-bit mode, 2-line display, 5x7 font */
    LCD_Command(0x0C); /* Display ON, Cursor OFF */
    LCD_Command(0x06); /* Auto-increment cursor */
    LCD_Command(0x01); /* Clear display memory */

    Delay_Ms(5);
}

void LCD_Command(unsigned char cmd)
{
    LPC_GPIO0->FIOCLR = LCD_RS;                     /* RS = 0 for Instruction */
    LPC_GPIO0->FIOCLR = LCD_DATA_MASK;             /* Clear data lines */
    LPC_GPIO0->FIOSET = ((unsigned int)cmd << 15); /* Shift byte to P0.15-P0.22 */

    /* Pulse EN pin */
    LPC_GPIO0->FIOSET = LCD_EN;
    Delay_Simple();
    LPC_GPIO0->FIOCLR = LCD_EN;
    Delay_Simple();
}

void LCD_Data(unsigned char data)
{
    LPC_GPIO0->FIOSET = LCD_RS;                      /* RS = 1 for Character Data */
    LPC_GPIO0->FIOCLR = LCD_DATA_MASK;              /* Clear data lines */
    LPC_GPIO0->FIOSET = ((unsigned int)data << 15); /* Shift byte to P0.15-P0.22 */

    /* Pulse EN pin */
    LPC_GPIO0->FIOSET = LCD_EN;
    Delay_Simple();
    LPC_GPIO0->FIOCLR = LCD_EN;
    Delay_Simple();
}

void LCD_PrintString(const char *str)
{
    while (*str) {
        LCD_Data((unsigned char)*str++);
    }
}

/* ============================================================================
 * ADC FUNCTIONS (12-bit SAR ADC)
 * ============================================================================
 */
void ADC_Init(void)
{
    /* 1. Enable power to ADC peripheral */
    LPC_SC->PCONP |= (1 << 12);

    /* 2. Configure P0.24 as AD0.1 function (PINSEL1[17:16] = 01) */
    LPC_PINCON->PINSEL1 &= ~(3 << 16);
    LPC_PINCON->PINSEL1 |=  (1 << 16);

    /* 3. Disable pull-up / pull-down on P0.24 to prevent signal distortion */
    LPC_PINCON->PINMODE1 &= ~(3 << 16);
    LPC_PINCON->PINMODE1 |=  (2 << 16); /* 10: Neither pull-up nor pull-down */

    /* 4. Configure ADC: Select AD0.1, CLKDIV=4, Power ON (PDN=1) */
    LPC_ADC->ADCR = (1 << 1) |
                    (4 << SBIT_CLCKDIV) |
                    (1 << SBIT_PDN);
}

float ADC_ReadTemperatureRaw(void)
{
    uint32_t adc_raw = 0;
    float voltage = 0.0f;

    /* Start conversion now */
    LPC_ADC->ADCR &= ~(7 << SBIT_START);
    LPC_ADC->ADCR |=  (1 << SBIT_START);

    /* Poll DONE bit in Global Data Register */
    while (!(LPC_ADC->ADGDR & (1U << SBIT_DONE)));

    /* Extract 12-bit result (bits 15:4) */
    adc_raw = (LPC_ADC->ADGDR >> SBIT_RESULT) & 0xFFF;

    /* Reference Voltage: 3.3V, 12-bit ADC (4095 steps) */
    voltage = ((float)adc_raw * 3.3f) / 4095.0f;

    /*
     * LM35 Linear Sensitivity: 10 mV / deg C = 0.010 V / deg C
     * Temperature = Voltage / 0.010 = Voltage * 100.0
     */
    return (voltage * 100.0f);
}

float ADC_ReadTemperatureFiltered(void)
{
    /* 8-sample moving average filter suppresses electrical switching noise */
    float sum = 0.0f;
    for (int i = 0; i < 8; i++) {
        sum += ADC_ReadTemperatureRaw();
        Delay_Ms(2);
    }
    return (sum / 8.0f);
}

/* ============================================================================
 * PWM FUNCTIONS (PWM1.5 on P1.24)
 * ============================================================================
 */
void PWM_Init(void)
{
    /* 1. Enable power to PWM1 */
    LPC_SC->PCONP |= (1 << 6);

    /* 2. Configure P1.24 as PWM1.5 (PINSEL3[17:16] = 10) */
    LPC_PINCON->PINSEL3 &= ~(3 << 16);
    LPC_PINCON->PINSEL3 |=  (2 << 16);

    /* 3. Prescaler: PR = 24 -> 1 MHz PWM timer clock (at PCLK = 25 MHz) */
    LPC_PWM1->PR = 24;

    /* 4. Match Register 0: 1000 counts -> 1 kHz PWM base frequency */
    LPC_PWM1->MR0 = PWM_PERIOD;

    /* 5. Match Register 5: Default duty cycle 25% (250 counts) */
    LPC_PWM1->MR5 = FAN_DUTY_LOW;

    /* 6. Reset PWM Counter on MR0 match */
    LPC_PWM1->MCR = (1 << 1);

    /* 7. Latch enable for MR0 and MR5 */
    LPC_PWM1->LER = (1 << 0) | (1 << 5);

    /* 8. Enable PWM1.5 single-edge output */
    LPC_PWM1->PCR |= (1 << 13);

    /* 9. Enable PWM counter and PWM mode */
    LPC_PWM1->TCR = (1 << 0) | (1 << 3);
}

void PWM_SetFanSpeed(int duty)
{
    if (duty > PWM_PERIOD) duty = PWM_PERIOD;
    if (duty < 0) duty = 0;

    /* Single-edge active-high PWM: MR5 directly dictates high pulse width */
    LPC_PWM1->MR5 = duty;

    /* Latch update to MR5 */
    LPC_PWM1->LER |= (1 << 5);
}

/* ============================================================================
 * DELAY FUNCTIONS
 * ============================================================================
 */
void Delay_Simple(void)
{
    for (volatile int i = 0; i < 3000; i++);
}

void Delay_Ms(int ms)
{
    for (int i = 0; i < ms; i++) {
        for (volatile int j = 0; j < 10000; j++);
    }
}
