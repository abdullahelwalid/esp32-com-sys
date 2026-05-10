# RevTalk - Motorcycle Intercom System

## Overview
RevTalk is a real-time motorcycle intercom system designed for riders to communicate wirelessly using ESP32 microcontrollers. The system leverages **ESP-IDF**, **FreeRTOS**, and the **I2S protocol** to capture and transmit audio between motorcycles, providing a seamless communication experience even in high-noise environments.

## Features
- **Real-time communication** using Bluetooth or Wi-Fi
- **I2S microphone support** for high-quality audio input
- **Audio playback via DAC** for clear sound output
- **Low-latency transmission** using optimized FreeRTOS tasks
- **Noise reduction and filtering** for improved voice clarity
- **Expandable system** allowing multi-rider connectivity

## Tech Stack
- **Microcontroller:** ESP32
- **Framework:** ESP-IDF
- **Operating System:** FreeRTOS
- **Audio Interface:** I2S
- **Wireless Communication:** Bluetooth or Wi-Fi

## Project Structure
```
/revtalk
├── main/              # Application source code
│   └── revtalk.c      # Main intercom implementation
├── test.py            # Serial capture → output.wav
├── sdkconfig          # ESP-IDF configuration
├── CMakeLists.txt     # Project build configuration
└── README.md          # Project documentation
```

## ESP-IDF and `idf.py`

### Installation (Fish shell, ESP32 target, IDF v5.4)

```sh
mkdir -p ~/esp
cd ~/esp
git clone -b v5.4 --recursive https://github.com/espressif/esp-idf.git
```

```sh
cd ~/esp/esp-idf
./install.fish esp32
```

### Activating the environment

From `~/esp/esp-idf` (this is independent of the RevTalk project directory; run these whenever you open a new shell to build or flash):

```sh
cd ~/esp/esp-idf
source ~/.espressif/python_env/idf5.4_py3.14_env/bin/activate.fish
. ./export.fish
```

The exact Python env path may differ slightly (for example `idf5.4_py3.12_env`); adjust to match the directory `install.fish` created.

The `idf.py` helper lives under the ESP-IDF tree, for example:

`~/esp/esp-idf/tools/idf.py`

(On this machine it may resolve to something like `/Users/<you>/esp/esp-idf/tools/idf.py`.)

### Build and flash

Build from the RevTalk project root (with the environment above already activated):

```sh
idf.py build
```

Flash at 921600 baud to a specific serial port (change the port to match your board):

```sh
idf.py -p /dev/cu.usbserial-0001 -b 921600 flash
```

The `-b` value here is the **download** speed for flashing only. It does not need to match the application UART baud used for audio streaming.

### Application UART (audio stream)

Mic samples are sent over **UART0** at **921600** baud, configured in `main/revtalk.c` (`uart_config_t`). **`test.py` must use the same `BAUD`** so the host reads the stream correctly.

## Testing (serial capture and playback)

1. **Serial line speed (host)** — On macOS, set the TTY to match the firmware UART (921600):

   ```sh
   stty -f /dev/cu.usbserial-0001 921600
   ```

   Use the same device path as in `test.py` (`SERIAL_PORT`). If `pyserial` already opens the port at 921600, `stty` is still useful when other tools have left the port at a different speed.

2. **Record** — From the RevTalk repo (with the ESP running the firmware and the same port in `test.py`):

   ```sh
   python test.py
   ```

   This records **10 seconds** of microphone data and writes **`output.wav`** (8 kHz, mono, 16-bit PCM).

3. **Play** — With FFmpeg installed:

   ```sh
   ffplay output.wav
   ```

   This plays the 10 second recording.

## Setup & Compilation (short path)

If ESP-IDF is already installed and exported:

1. Clone this repository and `cd` into it.
2. Optional: `idf.py menuconfig`
3. `idf.py build` then flash as in the section above.

### Prerequisites
- **ESP32 development board**
- **ESP-IDF** (see [official installation guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/))
- **I2S microphone (e.g., INMP441)**

## Code Overview
### **Main file: `revtalk.c`**
The core logic is implemented in `revtalk.c`:
- **I2S initialization** for the microphone
- **FreeRTOS tasks** for capture and UART streaming
- **Optional onboard LED** driven from peak level for a quick “audio present” sanity check

## Hardware Setup
### **ESP32 pin configuration (see `main/revtalk.c`)**
| Component | ESP32 GPIO |
|-----------|------------|
| I2S WS (LRCLK) | 14 |
| I2S SCK (BCLK) | 12 |
| I2S SD (DIN)   | 13 |

Match your wiring to these defines; they supersede older README pin tables if they differ.

### **Power requirements**
- ESP32 requires **3.3V** supply
- Follow your breakout’s datasheet for mic and any amplifier (e.g. MAX98357A) supply limits

## Future Improvements
- Implement **mesh networking** for extended range
- Add **PTT (Push-to-Talk) functionality**
- Integrate **AI-based noise suppression**

## Contributing
Feel free to submit issues, feature requests, or pull requests to improve **RevTalk**.

## License
This project is licensed under the **MIT License**.
