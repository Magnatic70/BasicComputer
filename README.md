# BasicComputer
A text-only basic computer with VGA-output using a ESP32/ESP8266 and a FPGA

## What it does
It is a simply computer with a BASIC-interpreter that is close to the one of the Sinclair ZX Spectrum. The computer is text-based only, but does support 8 colors for both foreground and background.

The computer has a 80x60 character display with a 640x480 resolution. You can use a PS2-keyboard connected to the FPGA-board or a HTML-based keyboard using a browser as input.

There are BASIC-commands for http-based communication (both incoming and outgoing), reading and writing of programs to SPIFFS, reading and writing of data to SPIFFS.

## Components, tools and software required
* ESP32: I use a D1 Mini ESP32 (https://www.aliexpress.com/item/32816065152.html)
* (Alternative) ESP8266: D1 Mini ESP8266 (https://www.aliexpress.com/item/32651747570.html)
* FPGA-board:  I use a A-C4E6E10 with an Cyclone IV EP4CE6E22C8 (https://www.aliexpress.com/item/4001125259366.html) but other FPGA-boards may also work. The FPGA-program needs 953 logical elements, 258 registers, 11 pins, 4688 bits of on-chip memory and 1 on-chip pll. The source in this project has all pins assigned, but they most likely won't work for other boards.
* Wires (M-F) to connect the ESP32 and FPGA-board
* Arduino-IDE to compile and upload to the ESP32
* Quartus Prime 19.1 Lite Edition to compile and upload to the FPGA
* (Optional) 3D-printer to print the case for the computer

Using the ESP8266 instead of the ESP32 will result in a lot less memory available for BASIC-programs. The ESP32 has 113792 bytes available for BASIC where the ESP8266 has just 16384 bytes available.

## How to make it
### ESP

### FPGA
### 3D-print
### Connect ESP and FPGA
### Place ESP and FPGA in 3D-printed case

