# Hardware Setup & Interfacing Guide

This document details the wiring and electronic schematic connections for the **LPC1768 Smart Cooling System**.

---

## 1. Pin Assignment Table

| Peripheral / Signal | LPC1768 Pin | Direction | Description |
| :--- | :--- | :--- | :--- |
| **LM35 Vout** | `P0.24` (`AD0.1`) | Input (Analog) | Analog voltage output ($10\text{ mV}/^\circ\text{C}$) |
| **PWM Fan Output** | `P1.24` (`PWM1.5`) | Output (Digital) | $1\text{ kHz}$ PWM signal driving Gate/Base of switch |
| **LCD RS** | `P0.10` | Output (Digital) | Register Select (`0` = Command, `1` = Data) |
| **LCD EN** | `P0.11` | Output (Digital) | Enable strobe pulse (Active High) |
| **LCD Data D0** | `P0.15` | Output (Digital) | 8-bit Data Bus Bit 0 |
| **LCD Data D1** | `P0.16` | Output (Digital) | 8-bit Data Bus Bit 1 |
| **LCD Data D2** | `P0.17` | Output (Digital) | 8-bit Data Bus Bit 2 |
| **LCD Data D3** | `P0.18` | Output (Digital) | 8-bit Data Bus Bit 3 |
| **LCD Data D4** | `P0.19` | Output (Digital) | 8-bit Data Bus Bit 4 |
| **LCD Data D5** | `P0.20` | Output (Digital) | 8-bit Data Bus Bit 5 |
| **LCD Data D6** | `P0.21` | Output (Digital) | 8-bit Data Bus Bit 6 |
| **LCD Data D7** | `P0.22` | Output (Digital) | 8-bit Data Bus Bit 7 |

---

## 2. LM35 Temperature Sensor Connection

* **Pin 1 ($V_{S}$)**: $+5\text{V}$ (or $+3.3\text{V}$)
* **Pin 2 ($V_{OUT}$)**: Connect to `P0.24` (AD0.1) on LPC1768
  * *Recommendation:* Place a $1\text{ k}\Omega$ to $2\text{ k}\Omega$ resistor in series between $V_{OUT}$ and `P0.24`, plus a $100\text{ nF}$ ceramic capacitor from `P0.24` to GND to filter high-frequency switching noise.
* **Pin 3 ($\text{GND}$)**: Common Ground

---

## 3. Fan Driver Circuit (Low-Side N-MOSFET Switching)

> ⚠️ **CAUTION**: Never connect a fan motor directly to an LPC1768 pin! The microcontroller pin can only handle $4\text{ mA}$, whereas a DC fan draws $100\text{ mA} - 1000\text{ mA}$. Connecting directly will permanently destroy the microcontroller.

### Schematic:
```text
           +12V / +5V (External Fan Supply)
                |
                +---------------+
                |               |
             +-----+            |
             | FAN |          [===]  Flyback Diode (1N4007 / 1N5819)
             +-----+          Cathode (+) at +12V, Anode (-) at Drain
                |               |
                +---------------+
                |
          D (Drain)
             |
LPC1768    |---|--+
P1.24 -----[ 220 Ohm ]--- G (Gate)   Logic-Level N-Channel MOSFET
                          |          (e.g., IRLZ44N, 2N7002, AO3400)
                        [10k] Pull-down to GND
                          |
                   S (Source)
                      |
                     GND (Common Ground with LPC1768)
```

### Protection Components:
1. **Flyback Diode (1N4007 / 1N5819)**: Placed reverse-biased across the fan terminals to absorb back-EMF inductive spikes ($V = -L \frac{di}{dt}$) when PWM shuts OFF.
2. **Gate Resistor ($220\,\Omega$)**: Limits peak inrush charging current into the MOSFET gate capacitance.
3. **Gate Pull-Down Resistor ($10\text{ k}\Omega$)**: Ensures the MOSFET stays completely OFF during microcontroller reset / booting.

---

## 4. HD44780 16x2 LCD Connection (8-bit Mode)

| LCD Pin | Pin Name | Connection |
| :--- | :--- | :--- |
| 1 | VSS | System GND |
| 2 | VDD | +5V DC |
| 3 | V0 | Wiper of 10k Potentiometer (Contrast control) |
| 4 | RS | LPC1768 `P0.10` |
| 5 | R/W | System GND (Write only mode) |
| 6 | E | LPC1768 `P0.11` |
| 7–14 | D0–D7 | LPC1768 `P0.15` through `P0.22` |
| 15 | Backlight A (+) | +5V (via 220 Ohm current-limiting resistor) |
| 16 | Backlight K (-) | System GND |
