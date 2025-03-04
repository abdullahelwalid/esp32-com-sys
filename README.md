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
│── main/          # Application source code
│   ├── revtalk.c  # Main intercom implementation
│── sdkconfig      # ESP-IDF configuration
│── CMakeLists.txt # Project build configuration
│── README.md      # Project documentation
```

## Setup & Compilation
### Prerequisites
- **ESP32 development board**
- **ESP-IDF installed** ([Installation Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/))
- **I2S microphone (e.g., INMP441)**
- **DAC (e.g., MAX98357A) for audio output**

### Installation & Flashing
1. Clone the repository:
   ```sh
   git clone https://github.com/yourusername/revtalk.git
   cd revtalk
   ```
2. Set up the ESP-IDF environment:
   ```sh
   . $HOME/esp/esp-idf/export.sh
   ```
3. Configure the project:
   ```sh
   idf.py menuconfig
   ```
4. Compile and flash:
   ```sh
   idf.py build flash monitor
   ```

## Code Overview
### **Main File: `revtalk.c`**
The core logic of the intercom system is implemented in `revtalk.c`:
- **I2S Initialization:** Configures the microphone and DAC
- **FreeRTOS Tasks:** Handles audio processing and communication
- **Noise Filtering:** Applies basic DSP techniques to enhance clarity

### **Audio Processing (`audio.c`)**
- Captures microphone input via **I2S**
- Filters background noise and normalizes volume
- Streams processed audio to the communication module

### **Communication (`comms.c`)**
- Handles Bluetooth or Wi-Fi transmission
- Implements low-latency packet-based streaming
- Supports multiple riders in a group network

## Hardware Setup
### **ESP32 Pin Configuration**
| Component      | ESP32 Pin |
|--------------|----------|
| I2S WS | GPIO25 |
| I2S SCK | GPIO32 |
| I2S SD | GPIO33 |


### **Power Requirements**
- ESP32 requires **3.3V power supply**
- Amplifier/DAC (MAX98357A) operates on **3.3V or 5V**

## Future Improvements
- Implement **mesh networking** for extended range
- Add **PTT (Push-to-Talk) functionality**
- Integrate **AI-based noise suppression**

## Contributing
Feel free to submit issues, feature requests, or pull requests to improve **RevTalk**.

## License
This project is licensed under the **MIT License**.
