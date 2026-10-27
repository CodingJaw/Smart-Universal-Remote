# Waveshare ESP32-S3-Touch-LCD-2.8 V2

This branch adapts the CuriousTech Smart Universal Remote to the V2 display.

- The screen still uses the ST7789 and the existing TFT_eSPI setup.
- V2 touchscreen: CST3530 (I2C address `0x58`, 32-bit registers).
- Older V1 touchscreen: CST328 (kept under `#else` for reference).
- Touch pins: SDA GPIO1, SCL GPIO3, reset GPIO2, IRQ GPIO4.
- IMU uses the separate I2C pins GPIO11 / GPIO10.

## VS Code and PlatformIO

`URemote/URemote.cpp` replaces the `.ino` sketch so it is a standard
Arduino-framework C++ translation unit. The forward declarations that
the Arduino sketch preprocessor previously generated are now explicit.

Use the repository root as your VS Code PlatformIO project.

```sh
pio run -e waveshare28
pio device list
pio run -e waveshare28 -t upload --upload-port /dev/ttyACM0
pio device monitor -p /dev/ttyACM0 -b 115200
```

Substitute the detected upload port. If VS Code still reports stale C++
errors, use **PlatformIO: Rebuild IntelliSense Index**.

`platformio.ini` defines `WAVESHARE_LCD_28_V2` to select the new
`CST3530` driver only for the Waveshare 2.8-inch board.

### First power-on test

The initial screen is the clock/dashboard tile. Swipe up for remote
controls and left/right between pages. "Use EspTouch" is the original
Wi-Fi SmartConfig notification, not a touchscreen error.

This is an initial driver port; confirm I2C detection, taps, long-presses,
and all swipe directions on a physical Waveshare V2 before treating it
as hardware-validated.
