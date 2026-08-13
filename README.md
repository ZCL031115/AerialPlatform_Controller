# AerialPlatform_Controller

STM32F103-based hardware safety controller for an aerial work platform. The
repository contains separate ECU (vehicle-side) and PCU (ground-side) firmware
projects migrated to the STM32 HAL library.

## Projects

- `ECU/`: vehicle-side controller, LoRa command handling, relay control, RC522
  card authentication, BMP280 pressure/height monitoring, and diagnostics.
- `PCU/`: ground-side controller, emergency-stop input, LoRa bridge, and
  diagnostics.

Both projects include STM32CubeMX `.ioc` files, Makefiles, STM32 HAL drivers,
and Windows host-side unit tests.

## Serial Interfaces

| Interface | Baud rate | Purpose |
| --- | ---: | --- |
| USART1 | 115200, 8N1 | Diagnostic output |
| USART2 | 115200, 8N1 | PC/external controller bridge |
| USART3 | 9600, 8N1 | MCU to LoRa module |

The two LoRa modules must also be configured for a local UART rate of 9600 baud
and matching radio channel, address, and air-data settings.

## Safety Commands

- `0x87`: emergency stop. The PCU forwards it over LoRa; the ECU enters the
  emergency-stop state, de-energizes the relay, and forwards the byte to its
  USART2 test interface.
- `0x26`: restore. The PCU accepts and forwards it only while the local
  emergency-stop input is inactive.

The PCU starts in a latched safe state. A valid restore command is required
before normal operation.

## Build

With GNU Arm Embedded Toolchain and Make available:

```powershell
make -j16 -f Makefile GCC_PATH=<arm-none-eabi-toolchain>/bin
```

Run the host tests on Windows with Visual Studio C/C++ Build Tools installed:

```powershell
cmd /c Tests\host\run_tests.cmd
```

Run the commands separately from the `ECU` and `PCU` directories.

