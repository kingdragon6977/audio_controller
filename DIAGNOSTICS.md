# Firmware diagnostics and safe bring-up standard

This project treats startup diagnostics as part of the firmware architecture, not as temporary debug prints.

## Boot breadcrumb order

1. Enter a known-safe board/GPIO state.
2. Start the debug UART.
3. Report MCU identity: DBGMCU ID, revision, device ID, flash size, UID.
4. Report the actual RCC clock tree and peripheral clock gates.
5. Report important GPIO pin modes, configuration and live levels.
6. Configure a peripheral.
7. Read back its hardware registers and verify the configuration.
8. Check the external electrical state before beginning transactions.
9. Only then activate or communicate with the external device.
10. Leave a clear PASS/FAIL breadcrumb at every important boundary.

## Safety rules

- Never configure a vulnerable external interface as push-pull unless the hardware design explicitly requires it.
- I2C SDA/SCL must be open-drain and released HIGH before generating START.
- Codec-driven I2S pins must never be driven by the MCU during codec-master operation.
- A failed preflight check stops the affected transaction rather than continuing blindly.
- Configuration output should come from actual MCU registers, not only from the values the source code intended to write.
- External hardware reset/enable sequencing must occur before normal device transactions.

## Required evidence for future MCU projects

Where supported by the MCU, startup diagnostics should include:

- device/revision/UID/flash size
- SYSCLK/HCLK/PCLK1/PCLK2
- PLL source/multiplier and bus prescalers
- RCC peripheral clock enables
- GPIO mode/configuration/live level for important pins
- I2C timing and status registers
- SPI/I2S configuration and status registers
- DMA configuration/status for active data paths
- external-device identity/probe result
- read-back verification of critical configuration

The goal is simple: **let the MCU leave a trail of where it has been and why it stopped, instead of making debugging a search for clues.**
