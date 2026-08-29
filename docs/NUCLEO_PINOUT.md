# Nucleo-64 Pinout Reference (F411RE / F446RE)

The Nucleo-64 form factor is identical between F411RE and F446RE.
This documents the connector layout, pin numbers, and functions.

## Connector Overview

The board has TWO physical rows on each side, but they are electrically the same nets:

- **CN7** (Morpho, left side) — CN6 + CN8 (Arduino headers, left) tap the SAME nets
- **CN10** (Morpho, right side) — CN5 + CN9 (Arduino headers, right) tap the SAME nets

So an Arduino header pin (e.g. D13) is directly wired to a specific Morpho pin.
The Morpho headers are just the "full access" version of the Arduino headers plus
all the extra MCU pins the Arduino form factor doesn't expose.

```
        ┌────────────────────────────────────────┐
        │  CN7 (Morpho L)      CN10 (Morpho R)    │
        │  ▪▪ 2 rows           2 rows ▪▪          │
        │                                          │
        │  CN8/CN6             CN5/CN9             │
        │  (Arduino L)         (Arduino R)         │
        │                                          │
        │           [ST-LINK / USB]                │
        └────────────────────────────────────────┘
```

## Arduino Headers (Right side — CN5 top, CN9 lower)

| Arduino | MCU Pin | Function (default AF) | Notes |
|---------|---------|----------------------|-------|
| D15 | PB8 | I2C1_SCL | |
| D14 | PB9 | I2C1_SDA | |
| AREF | - | Analog reference | |
| GND | - | Ground | |
| D13 | PA5 | SPI1_SCK / **User LED (green)** | LED_BUILTIN |
| D12 | PA6 | SPI1_MISO | |
| D11 | PA7 | SPI1_MOSI / TIM1_CH1N | |
| D10 | PB6 | TIM4_CH1 / I2C1_SCL | |
| D9  | PC7 | TIM3_CH2 | |
| D8  | PA9 | USART1_TX / TIM1_CH2 | |

| Arduino | MCU Pin | Function (default AF) | Notes |
|---------|---------|----------------------|-------|
| D7  | PA8 | TIM1_CH1 | |
| D6  | PB10 | TIM2_CH3 | |
| D5  | PB4 | TIM3_CH1 | |
| D4  | PB5 | TIM3_CH2 | |
| D3  | PB3 | TIM2_CH2 | |
| D2  | PA10 | USART1_RX / TIM1_CH3 | |
| D1  | PA2 | USART2_TX (ST-Link VCP) | Serial console |
| D0  | PA3 | USART2_RX (ST-Link VCP) | Serial console |

## Arduino Headers (Left side — CN6 top, CN8 lower)

### CN6 — Power

| Pin | Function | Notes |
|-----|----------|-------|
| NC | - | |
| IOREF | 3.3V ref | |
| RESET | NRST | |
| +3V3 | 3.3V out | |
| +5V | 5V out/in | |
| GND | Ground | |
| GND | Ground | |
| VIN | 7-12V input | External power (E5V jumper) |

### CN8 — Analog

| Arduino | MCU Pin | Function | Notes |
|---------|---------|----------|-------|
| A0 | PA0 | ADC1_IN0 / TIM2_CH1 | |
| A1 | PA1 | ADC1_IN1 / TIM2_CH2 | |
| A2 | PA4 | ADC1_IN4 / SPI1_NSS | |
| A3 | PB0 | ADC1_IN8 / TIM3_CH3 | |
| A4 | PC1 | ADC1_IN11 | |
| A5 | PC0 | ADC1_IN10 | |

## Morpho Headers — Full Pin Access

### CN7 (Left Morpho) — Pin numbers 1-38

Odd pins = left column, even pins = right column.

| # | Pin | Key Functions (AF) | # | Pin | Key Functions (AF) |
|---|-----|-------------------|---|-----|-------------------|
| 1 | PC10 | SPI3_SCK, USART3_TX, UART4_TX | 2 | PC11 | SPI3_MISO, USART3_RX, UART4_RX |
| 3 | PC12 | SPI3_MOSI, UART5_TX | 4 | PD2 | UART5_RX, TIM3_ETR |
| 5 | VDD | Power | 6 | E5V | 5V from ST-Link |
| 7 | BOOT0 | Boot mode select | 8 | GND | Ground |
| 9 | NC | — | 10 | NC | — |
| 11 | NC | — | 12 | IOREF | 3.3V I/O ref |
| 13 | PA13 | **SWDIO** (debug — avoid) | 14 | RESET | NRST |
| 15 | PA14 | **SWCLK** (debug — avoid) | 16 | +3V3 | Power |
| 17 | PA15 | TIM2_CH1, SPI1_NSS, I2S3_WS | 18 | +5V | Power |
| 19 | GND | Ground | 20 | GND | Ground |
| 21 | PB7 | I2C1_SDA, USART1_RX, TIM4_CH2 | 22 | GND | Ground |
| 23 | PC13 | GPIO (User button on some) | 24 | VIN | 7-12V input |
| 25 | PC14 | OSC32_IN (RTC xtal) | 26 | NC | — |
| 27 | PC15 | OSC32_OUT (RTC xtal) | 28 | PA0 | ADC1_IN0, TIM2_CH1, TIM5_CH1, UART4_TX |
| 29 | PH0 | OSC_IN (HSE xtal) | 30 | PA1 | ADC1_IN1, TIM2_CH2, TIM5_CH2, UART4_RX |
| 31 | PH1 | OSC_OUT (HSE xtal) | 32 | PA4 | ADC1_IN4, SPI1_NSS, DAC_OUT1, USART2_CK |
| 33 | VBAT | Battery backup | 34 | PB0 | ADC1_IN8, TIM1_CH2N, TIM3_CH3 |
| 35 | PC2 | ADC1_IN12, SPI2_MISO | 36 | PC1 | ADC1_IN11 |
| 37 | PC3 | ADC1_IN13, SPI2_MOSI, I2S2_SD | 38 | PC0 | ADC1_IN10 |

### CN10 (Right Morpho) — Pin numbers 1-38

Odd pins = left column, even pins = right column.

| # | Pin | Key Functions (AF) | # | Pin | Key Functions (AF) |
|---|-----|-------------------|---|-----|-------------------|
| 1 | PC9 | TIM3_CH4, TIM8_CH4, I2C3_SDA, SDIO_D1 | 2 | PC8 | TIM3_CH3, TIM8_CH3, SDIO_D0 |
| 3 | PB8 | I2C1_SCL, TIM4_CH3, TIM10_CH1, CAN1_RX | 4 | PC6 | TIM3_CH1, TIM8_CH1, USART6_TX |
| 5 | PB9 | I2C1_SDA, TIM4_CH4, TIM11_CH1, CAN1_TX | 6 | PC5 | ADC1_IN15, USART3_RX |
| 7 | AVDD | Analog power | 8 | U5V | 5V from USB |
| 9 | GND | Ground | 10 | NC | — |
| 11 | PA5 | **LED green (D13)**, SPI1_SCK, TIM2_CH1, DAC_OUT2 | 12 | PA12 | USART1_RTS, TIM1_ETR, CAN1_TX, USB_DP |
| 13 | PA6 | SPI1_MISO, TIM3_CH1, TIM1_BKIN, TIM13_CH1 | 14 | PA11 | USART1_CTS, TIM1_CH4, CAN1_RX, USB_DM |
| 15 | PA7 | SPI1_MOSI, TIM1_CH1N, TIM3_CH2, TIM14_CH1, ADC1_IN7 | 16 | PB12 | **TIM1_BKIN**, SPI2_NSS, I2C2_SMBA, CAN2_RX |
| 17 | PB6 | I2C1_SCL, TIM4_CH1, USART1_TX, CAN2_TX | 18 | PB11 | TIM2_CH4, I2C2_SDA, USART3_RX |
| 19 | PC7 | TIM3_CH2, TIM8_CH2, USART6_RX, I2S3_MCK | 20 | GND | Ground |
| 21 | PA9 | USART1_TX, TIM1_CH2, I2C3_SMBA, USB_VBUS | 22 | PB2 | GPIO (BOOT1) |
| 23 | PA8 | **TIM1_CH1**, I2C3_SCL, USART1_CK, MCO1 | 24 | PB1 | ADC1_IN9, TIM1_CH3N, TIM3_CH4, TIM8_CH3N |
| 25 | PB10 | TIM2_CH3, I2C2_SCL, SPI2_SCK, USART3_TX | 26 | PB15 | SPI2_MOSI, TIM1_CH3N, TIM8_CH3N, I2S2_SD |
| 27 | PB4 | TIM3_CH1, SPI1_MISO, SPI3_MISO, I2C3_SDA | 28 | PB14 | TIM1_CH2N, SPI2_MISO, TIM8_CH2N, USART3_RTS |
| 29 | PB5 | TIM3_CH2, SPI1_MOSI, SPI3_MOSI, CAN2_RX | 30 | PB13 | **TIM1_CH1N**, SPI2_SCK, TIM8_CH1N, USART3_CTS |
| 31 | PB3 | TIM2_CH2, SPI1_SCK, SPI3_SCK, I2C2_SDA, **SWO** | 32 | AGND | Analog ground |
| 33 | PA10 | USART1_RX, TIM1_CH3, USB_ID | 34 | PC4 | ADC1_IN14 |
| 35 | PA2 | **USART2_TX (VCP)**, TIM2_CH3, TIM5_CH3, TIM9_CH1 | 36 | NC | — |
| 37 | PA3 | **USART2_RX (VCP)**, TIM2_CH4, TIM5_CH4, TIM9_CH2 | 38 | NC | — |

> **Bold** = special/reserved: debug (SWD/SWO), onboard LED, serial console (VCP), or key project functions (TIM1 PWM/BKIN).

> For the inter-board wiring (which of these pins travel down to the driver
> board and on which JST connector), see `JST_CONNECTORS.md`.

## Project Pin Assignments — Where They Land

| Function | MCU Pin | Morpho Location | Arduino Alias |
|----------|---------|-----------------|---------------|
| PWM_A | PA8 | CN10 pin 23 | D7 |
| PWM_B | PB13 | CN10 pin 30 | - |
| FAULT_IN (BKIN) | PB12 | CN10 pin 16 | - |
| FREQ_FB | PA0 | CN7 pin 28 | A0 |
| ADC_OCP | PA1 | CN7 pin 30 | A1 |
| ADC_VBUS | PA4 | CN7 pin 32 | A2 |
| ADC_NTC | PC0 | CN7 pin 38 | A5 |
| AC_SENSE | PC1 | CN7 pin 36 | A4 |
| ENC_A | PB4 | CN10 pin 27 | D5 |
| ENC_B | PB5 | CN10 pin 29 | D4 |
| ENC_BTN | PC13 | CN7 pin 23 | - |
| TFT_SCK | PA5 | CN10 pin 11 | D13 |
| TFT_MOSI | PA7 | CN10 pin 15 | D11 |
| TFT_CS | PB6 | CN10 pin 17 | D10 |
| TFT_DC | PC7 | CN10 pin 19 | D9 |
| TFT_RST | PA9 | CN10 pin 21 | D8 |
| STATUS_LED | PB0 | CN7 pin 34 | A3 |
| CONTACTOR | PB14 | CN10 pin 28 | - |

## Pins to AVOID

| Pin | Why |
|-----|-----|
| PA13, PA14 | SWD debug (SWDIO/SWCLK) — used by ST-Link |
| PA5 | Also drives onboard green LED — OK for TFT SCK but LED will flicker |
| PB3, PA15, PB4 | JTAG remnants — usable but need remap from default |
| PC14, PC15 | 32kHz crystal (if RTC used) |
| PH0, PH1 | Main oscillator (if HSE used) |
| BOOT0 | Boot mode select — do not pull high at reset |
| PA2, PA3 | USART2 → ST-Link virtual COM port (serial console) |

## Peripheral Quick-Reference (F446RE)

Handy for future projects — which pins give you what.

### Advanced Timers (complementary PWM + dead-time + break)
| Timer | Channels | Complementary (N) | Break | Common Pins |
|-------|----------|-------------------|-------|-------------|
| TIM1 | CH1: PA8, CH2: PA9, CH3: PA10, CH4: PA11 | CH1N: PB13, CH2N: PB14, CH3N: PB15 | BKIN: PB12 or PA6 | (used in this project) |
| TIM8 | CH1: PC6, CH2: PC7, CH3: PC8, CH4: PC9 | CH1N: PB13, CH2N: PB14, CH3N: PB15 | BKIN: PA6 | Spare advanced timer |

### General-Purpose Timers
| Timer | Notes | Common Pins |
|-------|-------|-------------|
| TIM2 | 32-bit, input capture, encoder | PA0, PA1, PA5, PA15, PB3, PB10, PB11 |
| TIM3 | 16-bit, encoder mode | PA6, PA7, PB0, PB1, PB4, PB5, PC6-9 |
| TIM4 | 16-bit | PB6, PB7, PB8, PB9 |
| TIM5 | 32-bit | PA0, PA1, PA2, PA3 |

### Communication
| Bus | Instance | Pins |
|-----|----------|------|
| SPI1 | SCK: PA5, MISO: PA6, MOSI: PA7, NSS: PA4 | (D13/D12/D11/D10) |
| SPI2 | SCK: PB10/PB13, MISO: PB14/PC2, MOSI: PB15/PC3 | |
| SPI3 | SCK: PB3/PC10, MISO: PB4/PC11, MOSI: PB5/PC12 | |
| I2C1 | SCL: PB6/PB8, SDA: PB7/PB9 | |
| I2C2 | SCL: PB10, SDA: PB11 | |
| I2C3 | SCL: PA8, SDA: PC9 | |
| USART1 | TX: PA9/PB6, RX: PA10/PB7 | |
| USART2 | TX: PA2, RX: PA3 | **VCP — ST-Link serial console** |
| USART3 | TX: PB10/PC10, RX: PB11/PC11 | |

### ADC (ADC1 channels)
| Channel | Pin | Channel | Pin |
|---------|-----|---------|-----|
| IN0 | PA0 | IN8 | PB0 |
| IN1 | PA1 | IN9 | PB1 |
| IN4 | PA4 | IN10 | PC0 |
| IN7 | PA7 | IN11 | PC1 |
| — | — | IN12 | PC2 |
| — | — | IN13 | PC3 |
| — | — | IN14 | PC4 |
| — | — | IN15 | PC5 |

### DAC (F446RE has 2 channels — F411 does not)
| Channel | Pin |
|---------|-----|
| DAC_OUT1 | PA4 |
| DAC_OUT2 | PA5 |

> Note: This is the physical/electrical layout common to F411RE and F446RE.
> Peripheral quick-reference is F446RE-specific (TIM8, DAC, extra USARTs).
> The default alternate-function (AF) mappings shown are the most common;
> most pins have multiple AF options selectable in firmware via the AF mux.
> Verify against the F446RE datasheet (Table 11, Alternate Functions) for
> non-default AF assignments.
