
// Copyright 2023 Rich Heslip
//
// Author: Rich Heslip 
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// I/O pin definitions for Raspberry Pi Pico eurorack module


#ifndef IO_H_
#define IO_H_

#define CPU_USE 28
//#define DEBUG_PIN 26
#define SPI0_MOSI 19
#define SPI0_MISO 16
#define SPI0_CS 17
#define SPI0_SCLK 18

/*
#define SPI1_MOSI 11
#define SPI1_MISO 8
#define SPI1_CS 9
#define SPI1_SCLK 10
*/
// SD card pins
#define RP_CLK_GPIO 7
#define RP_CMD_GPIO 6
#define RP_DAT0_GPIO 8

#define SDA 4
#define SCL 5

// I2S pins for DAC
#define BCLK 13
#define WS 14  // this will always be 1 pin above BCLK - can't change it
#define I2S_DATA 15

// OLED display pins
#define OLED_DC     2
#define OLED_CS     17
#define OLED_RESET  3

// TFT display pins
#define TFT_RESET 22
#define TFT_CS  17
#define TFT_RS   26

// SD card
#define SD_CS 9

#define ENC_A 20  // encoder
#define ENC_B 12
#define ENC_SW   21 


// analog inputs for voltage control - range approx 0-5v
// 
#define AIN0 	40
#define AIN1 	41  // Pimoroni Plus uses RP2350B - A/D on different inputs
#define AIN2 	42
#define AIN3 	29 // not available on standard Pico boards

// battery monitor
#define BAT_VOLTAGE AIN1

// MIDI serial port pins - not really MIDI but the serial port is exposed on the first two jacks
#define MIDIRX 1
#define MIDITX 0

#endif // IO_H_

