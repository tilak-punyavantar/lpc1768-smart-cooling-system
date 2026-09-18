#  Smart Cooling System (NXP LPC1768)

An intelligent, closed-loop thermal management firmware developed for the **NXP LPC1768 (ARM Cortex-M3)** in bare-metal Embedded C. The system performs real-time temperature acquisition via a **12-bit SAR ADC**, executes a **hysteresis-stabilized control algorithm**, adjusts cooling fan speed using **hardware PWM**, and outputs live telemetry to an **HD44780 16×2 character LCD**.

---

##  Key Features

* **High-Accuracy Sensing**: Samples analog temperature from an LM35 sensor via 12-bit ADC (`AD0.1`, `P0.24`) with an 8-point moving average filter to suppress high-frequency noise.
* **Hysteresis-Based Control**: Implements a 3-tier finite state machine with a $\pm 0.5^\circ\text{C}$ deadband, eliminating thermal chattering, acoustic whining, and mechanical fan fatigue.
* **Hardware PWM Actuation**: Drives a low-side N-MOSFET switch using LPC1768's dedicated PWM peripheral (`PWM1.5`, `P1.24`) at $1\text{ kHz}$ across 25%, 55%, and 90% duty cycles.
* **Optimized LCD Driver**: Direct 8-bit GPIO bit-banged driver for HD44780 displays using fixed-point arithmetic, completely eliminating soft-FPU runtime overhead on Cortex-M3.

---

##  System Architecture & Control Logic

```
 +----------------+      Analog      +-------------------------+
 |  LM35 Sensor   | ---------------> | LPC1768 12-bit ADC      |
 | (10 mV/°C)     |      (P0.24)     | (AD0.1 @ 5 MHz)         |
 +----------------+                  +-------------------------+
                                                  |
                                      Moving Average Filter
                                                  |
                                                  v
                                     +-------------------------+
                                     | Finite State Machine    |
                                     | Thermal Controller      |
                                     +-------------------------+
                                       /                     \
                      PWM1.5 (P1.24)  /                       \  GPIO (P0.10-P0.22)
                                     v                         v
                       +-------------------+             +-------------------+
                       | Logic N-MOSFET    |             | HD44780 16x2 LCD  |
                       | + Flyback Diode   |             | Real-time Telemetry|
                       +-------------------+             +-------------------+
                                 |
                                 v
                       +-------------------+
                       | Brushless DC Fan  |
                       +-------------------+
```

### Hysteresis State Machine (Chatter Prevention)

```
        T >= 25.5°C                        T >= 30.5°C
  +--------------------+             +--------------------+             +--------------------+
  |    LEVEL 1 (L1)    | ----------> |    LEVEL 2 (L2)    | ----------> |    LEVEL 3 (L3)    |
  |   25% Duty Cycle   | <---------- |   55% Duty Cycle   | <---------- |   90% Duty Cycle   |
  +--------------------+  T < 24.5°C +--------------------+  T < 29.5°C +--------------------+
```

---

##  Mathematical Derivations

### 1. ADC Voltage & Temperature Conversion
* **ADC Resolution**: 12-bit SAR ADC $\rightarrow 2^{12} - 1 = 4095$ quantization levels.
* **Reference Voltage ($V_{REF}$)**: $3.3\text{ V}$
$$\text{Voltage } (V) = \frac{\text{ADC\_RAW} \times 3.3\text{ V}}{4095}$$
* **LM35 Sensitivity**: $10\text{ mV}/^\circ\text{C} = 0.010\text{ V}/^\circ\text{C}$
$$\text{Temperature } (^\circ\text{C}) = \frac{\text{Voltage}}{0.010\text{ V}/^\circ\text{C}} = \text{Voltage} \times 100.0$$

### 2. PWM Frequency & Duty Cycle Configuration
* **Peripheral Clock ($PCLK_{\text{PWM1}}$)**: $25\text{ MHz}$ (with $CCLK = 100\text{ MHz}$, $PCLK\_DIV = 4$)
* **Prescaler ($PR$)**: $24 \implies f_{\text{timer}} = \frac{25\text{ MHz}}{24 + 1} = 1\text{ MHz}$
* **Period ($MR0$)**: $1000 \implies f_{\text{PWM}} = \frac{1\text{ MHz}}{1000} = 1\text{ kHz}$
* **Duty Cycle**:
$$\text{Duty Cycle (\%)} = \frac{MR5}{MR0} \times 100\%$$
  * **Low Speed (L1)**: $MR5 = 250 \implies 25\%$
  * **Medium Speed (L2)**: $MR5 = 550 \implies 55\%$
  * **High Speed (L3)**: $MR5 = 900 \implies 90\%$

---

##  Pin Mapping

| Peripheral | LPC1768 Pin | Function / Register | Description |
| :--- | :--- | :--- | :--- |
| **LM35 Analog In** | `P0.24` | `AD0.1` | 12-bit ADC Input (Pull-up disabled) |
| **Fan PWM Out** | `P1.24` | `PWM1.5` | 1 kHz Single-Edge PWM |
| **LCD RS** | `P0.10` | GPIO Output | Command / Data Select |
| **LCD EN** | `P0.11` | GPIO Output | Enable Strobe Pulse |
| **LCD Data Bus** | `P0.15` - `P0.22` | GPIO Output | 8-bit Data Bus (D0 - D7) |

> Complete electrical schematic and flyback protection details are in [docs/HARDWARE_SETUP.md](docs/HARDWARE_SETUP.md).

---

##  Repository Structure

```
.
├── .gitignore               # Ignores Keil MDK, VS Code, and build artifacts
├── LICENSE                  # MIT License
├── README.md                # Project documentation & engineering specs
├── docs/
│   └── HARDWARE_SETUP.md    # Circuit schematic, MOSFET driver, & pinouts
└── src/
    └── main.c               # Firmware source code (LPC1768 Embedded C)
```

---

##  How to Build and Flash

### Using Keil µVision (MDK-ARM):
1. Open Keil µVision and create a new project targeting **NXP LPC1768**.
2. When prompted, select **CMSIS Core** and **Startup** files.
3. Add `src/main.c` to your Source Group.
4. Under **Options for Target $\rightarrow$ Target**: Ensure System Frequency is set to $100.0\text{ MHz}$ and select **Use MicroLIB**.
5. Click **Build (F7)** to generate the `.hex` / `.axf` binary.
6. Flash using **ULINK2 / J-Link / CMSIS-DAP** debugger or via **Flash Magic** (UART ISP on COM port).

---

##  License
This project is open-source under the [MIT License](LICENSE).
