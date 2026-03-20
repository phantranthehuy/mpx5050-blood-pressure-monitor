# 🩺 Digital Oscillometric Blood Pressure Monitor

> An academic research project focused on designing and developing a digital blood pressure monitor using the **MPX5050** pressure sensor and the **Oscillometric Method**.

## 📖 Table of Contents
- [About the Project](#about-the-project)
- [Key Features](#key-features)
- [Team Members](#team-members)
- [System Architecture](#system-architecture)
- [Technology Stack](#technology-stack)
- [Repository Structure](#repository-structure)
- [Getting Started](#getting-started)
- [License](#license)

## 🔬 About the Project
This repository contains the hardware schematics, embedded C firmware, and signal processing algorithms for an automated blood pressure monitor. 

The system operates based on the **Oscillometric Method**. It extracts tiny pressure oscillations (AC component) superimposed on the static cuff pressure (DC component) during the deflation phase. By analyzing the envelope of these oscillations, the system calculates the Mean Arterial Pressure (MAP), Systolic Pressure (SYS), Diastolic Pressure (DIA), and Heart Rate (BPM).

## ✨ Key Features
- **Automated Cuff Control:** Smooth inflation and linear deflation using a DC pump and a solenoid valve.
- **High-Precision Sensing:** Utilizes the MPX5050DP sensor with a custom Analog Front-End (AFE) for signal amplification and band-pass filtering (0.5Hz - 5Hz).
- **DSP Algorithms:** Implements digital filtering and envelope detection algorithms to accurately identify blood pressure parameters.
- **User Interface:** Real-time pressure monitoring and final result display via an LCD/OLED screen.

## 👥 Team Members
| Name | Role | Responsibilities |
| :--- | :--- | :--- |
| **Phan Tran The Huy** | **Project Manager & Hardware Lead** | Overall project management, Altium/KiCad schematic & PCB design, and hardware integration. |
| **Tran Nhat Ly** | **Mechanical & UI/UX Engineer** | 3D enclosure design, mechanical assembly, component sourcing, and UI programming. |
| **Nguyen Huy Hoang** | **DSP Algorithm & Research Lead** | Signal processing research (Kalman/FIR filters), data analysis, and technical documentation. |
| **Cao Hoang Tung** | **Embedded Software Engineer** | STM32 C programming, Pump/Valve driver implementation, and translating MATLAB/Python algorithms into embedded C. |

## 📐 System Architecture

The core of the system relies on capturing and separating the pressure signals:

## 🛠 Technology Stack
- **Microcontroller:** STM32 Series
- **Firmware Language:** C (STM32 HAL Framework)
- **Development Environment:** VS Code with STM32CubeIDE Extension
- **Hardware Design:** Altium Designer / KiCad
- **Simulation Tools:** LTSpice (Analog filtering), Python/MATLAB (Algorithm verification)

## 📂 Repository Structure
```text
📦 mpx5050-blood-pressure-monitor
 ┣ 📂 Algorithms      # Python/MATLAB scripts for algorithm simulation & signal filtering
 ┣ 📂 Docs            # Datasheets, academic papers, and technical reports
 ┣ 📂 Firmware        # Embedded C source code, CubeMX (.ioc) config, and drivers
 ┣ 📂 Hardware        # Schematics, PCB layouts, and Bill of Materials (BOM)
 ┣ 📂 Mechanical      # 3D models and CAD files for the device enclosure
 ┣ 📜 .gitignore      # Ignored files for STM32, VS Code, and EDA tools
 ┣ 📜 LICENSE         # MIT License
 ┗ 📜 README.md       # Project documentation
```

## 🚀 Getting Started

### Prerequisites
- **VS Code** installed with the `C/C++` and `STM32 VS Code Extension`.
- **STM32CubeMX** (for modifying microcontroller pinouts/clocks).
- **Git** for version control.

### Installation & Build
1. **Clone the repository:**
   ```bash
   git clone https://github.com/your-username/mpx5050-blood-pressure-monitor.git
   ```
2. **Open the Firmware:**
   Open the `/Firmware` directory in VS Code.
3. **Compile:**
   Press `Ctrl + Shift + B` (or use the STM32 extension build button) to compile the C code and generate the `.elf` / `.hex` files.
4. **Flash to MCU:**
   Connect your ST-Link debugger and click `Run > Start Debugging` (F5) to flash the firmware onto the STM32.

## ⚖️ License
This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details. Designed for academic and research purposes.
