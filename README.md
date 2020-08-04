# BasicComputer
A text-only basic computer with VGA-output using a ESP32/ESP8266 and a FPGA

## What it does
It is a simply computer with a BASIC-interpreter that is close to the one of the Sinclair ZX Spectrum. The computer is text-based only, but does support 8 colors for both foreground and background.

The computer has a 80x60 character display with a 640x480 resolution. You can use a PS2-keyboard connected to the FPGA-board or a HTML-based keyboard using a browser as input.

There are BASIC-commands for http-based communication (both incoming and outgoing), reading and writing of programs to SPIFFS, reading and writing of data to SPIFFS.

## Components, tools and software required
* ESP32: I use a D1 Mini ESP32 (https://www.aliexpress.com/item/32816065152.html)
* (Alternative) ESP8266: D1 Mini ESP8266 (https://www.aliexpress.com/item/32651747570.html)
* FPGA-board:  I use a A-C4E6E10 with an Cyclone IV EP4CE6E22C8 (https://www.aliexpress.com/item/4001125259366.html) but other FPGA-boards may also work. The FPGA-program needs 953 logical elements, 258 registers, 11 pins, 4688 bits of on-chip memory and 1 on-chip pll. The source in this repository has all pins assigned, but they most likely won't work for other boards.
* USB-blaster or other JTAG-programmer for uploading to the FPGA
* Five wires (M-F) to connect the ESP32 and FPGA-board
* Arduino-IDE to compile and upload to the ESP32
* Quartus Prime 19.1 Lite Edition with support for the Cyclone IV-family to compile and upload to the FPGA
* (Optional) 3D-printer to print the case for the computer
* (Optional) Two small screws to join the two parts of the case and four small screws to join the FPGA-board to the bottom of the case.

Using the ESP8266 instead of the ESP32 will result in a lot less memory available for BASIC-programs. The ESP32 has 113792 bytes available for BASIC where the ESP8266 has just 16384 bytes available.

## How to make it
### ESP
#### Compilation
1. Download all files from https://github.com/Magnatic70/magnatic-esp, except magnatic-esp.ino and add these files to the ones in this projects esp-source folder.
1. Use the Arduino-IDE to load, compile and upload BASCOMP002.ino to the ESP.

The ESP is now ready for use.

### FPGA
1. Load asciiVGAVideoCard.qpf in Quartus Prime and Start Compilation
1. File --> Convert programming files --> Open conversion Setup Data -->  sof2jic4cycloneiv.cof --> Open --> Generate
1. Programmer --> Add file --> output_files/asciiVGACard.jic --> Open --> Check the Program/Configure checkbox in the second line --> Start

The FPGA-board is now ready for use.

### (Optional) 3D-print
1. Print the bottom.stl with a layer height of 0.2mm, no supports needed, infill 10%
1. Print the top.stl with a layer height of 0.2mm, SUPPORTS NEEDED!, infill 10%
1. Remove supports

### Connect ESP and FPGA
For a D1 Mini ESP32
1. Connect FPGA-pin GND to ESP-pin GND
1. Connect FPGA-pin 3.3V to ESP-pin 3.3V
1. Connect FPGA-pin 30 to ESP-pin IO17 (TX UART2)
1. Connect FPGA-pin 28 to ESP-pin IO16 (RX UART2)
1. Connect FPGA-pin 32 to ESP-pin IO21

For a D1 Mini ESP8266
1. Connect FPGA-pin GND to ESP-pin GND
1. Connect FPGA-pin 3.3V to ESP-pin 3.3V
1. Connect FPGA-pin 30 to ESP-pin TX
1. Connect FPGA-pin 28 to ESP-pin RX
1. Connect FPGA-pin 32 to ESP-pin D1

### Place ESP and FPGA in 3D-printed case

