// Copyright 2019 Rich Heslip
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
// 
// sample player/looper inspired by Jan Ostman's ESP8266 drum machine http://janostman.wordpress.com
// will play a 22khz sample file converted to a header file in the appropriate format
// Feb 3/19 - initial version on ESP8266
// Jan 2023 - ported code to Pi Pico so I can use it on a 16mb flash version
// July 24 - this version modded to run on my new groovebox HW - added recording, tracks, scenes, song mode, sample slicing etc
// Sept 2024 - ported to Pimoroni Pico 2 + board with 8mb PSRAM and SD card - can load samples from SD now. 160x128 Color TFT display. added a per track pattern generator

#include <Arduino.h> 

//#include <WiFi.h>
//#include <i2s.h>
//#include <i2s_reg.h>
//#include <pgmspace.h>
//#include "driver/i2s.h"
//#include "freertos/queue.h"
#include <Wire.h>
#include "SdFat.h"
#include "sdios.h"

#include <SPI.h>
//#include <SD.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
//#include <Adafruit_SSD1306.h>
//#include <SH1106.h>
//#include "MIDI.h"
#include "io.h"
#include <I2S.h>
//#include "RPi_Pico_TimerInterrupt.h" // fails on RP2350 after a while
#include <hardware/timer.h>
#include <hardware/irq.h>
#include "Clickencoder.h"
#include "Adafruit_MPR121.h"
#include "SixteenStep.h"
#include "pico/multicore.h"
#include "drumpatterns.h"

#define DEBUG // to get serial debug info

#ifndef _BV
#define _BV(bit) (1 << (bit)) 
#endif

// Try to select the best SD card configuration.
#if defined(HAS_TEENSY_SDIO)
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif defined(RP_CLK_GPIO) && defined(RP_CMD_GPIO) && defined(RP_DAT0_GPIO)
// See the Rp2040SdioSetup example for RP2040/RP2350 boards.
#define SD_CONFIG SdioConfig(RP_CLK_GPIO, RP_CMD_GPIO, RP_DAT0_GPIO)
#elif ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK)
#else  // HAS_TEENSY_SDIO
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SPI_CLOCK)
#endif  // HAS_TEENSY_SDIO


// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#define SD_FAT_TYPE 3

#if SD_FAT_TYPE == 0
SdFat sd;
File file;
#elif SD_FAT_TYPE == 1
SdFat32 sd;
File32 file;
#elif SD_FAT_TYPE == 2
SdExFat sd;
ExFile file;
#elif SD_FAT_TYPE == 3
SdFs sd;
FsFile file;
#else  // SD_FAT_TYPE
#error Invalid SD_FAT_TYPE
#endif  // SD_FAT_TYPE

// Try max SPI clock for an SD. Reduce SPI_CLOCK if errors occur.
//#define SPI_CLOCK SD_SCK_MHZ(50)
// #define SPI_CLOCK SD_SCK_MHZ(25)

#define MONITOR_CPU1  // define to enable 2nd core monitoring
//#define HW_DEBUG  // for debug pin usage

//Adafruit_SH1106 display(OLED_DC, OLED_RESET, OLED_CS);
Adafruit_ST7735 display = Adafruit_ST7735(TFT_CS, TFT_RS, TFT_RESET);
#define WHITE ST77XX_WHITE
#define BLACK ST77XX_BLACK
#define RED ST77XX_RED
#define RED2 0xF804
#define LIGHTGREEN 0x03e0
#define GREEN ST77XX_GREEN
#define BLUE ST77XX_BLUE 
#define CYAN ST77XX_CYAN
#define MAGENTA ST77XX_MAGENTA
#define YELLOW ST77XX_YELLOW
#define ORANGE ST77XX_ORANGE


#define PSRAM_ADDR 0x11000000
#define PSRAM_SIZE 0x00800000  // 8 Mbytes

//#define SAMPLERATE 11025 
#define SAMPLERATE 22050
//#define SAMPLERATE 44100
#define PMALLOC_CHUNK 131072 // hopefully minimizes memory fragmentation 

#define MIDDLE_C 60 // MIDI note for middle C
#define HIGHEST_NOTE 76 // upper note for editor
#define LOWEST_NOTE 43 // lower note for editor
#define NOTE_DEADZONE 4 // range of notes above highest and below lowest that will result in note being turned off in editor
#define DEFAULT_LEVEL 64  // default MIDI velocity

#define NTRACKS 16  // 
#define NSCENES  16 // works best with the keypad
#define NUM_VOICES NTRACKS
#define MAX_STEPS FS_MAX_STEPS // max number of notes per sequencer
#define SEQUENCER_MEMORY 2048  // memory per sequencer (bytes)
//#define SEQUENCER_MEMORY sizeof(SixteenStepNote)*MAX_STEPS // FifteenStep can record polyphonic but not using that 
#define STEPS_PER_BAR 16
#define TEMPO    120 // default tempo
#define CLIPS_COMPLETE ((1<<NSCENES)-1) // bitmap of clips that are on last step

// Keeps track of the last pins touched
// so we know when buttons are released
uint32_t lasttouched = 0;
uint32_t currtouched = 0;
#define RECBUTTON_HOLD_TIME 500 // time in ms for rec button hold to erase
#define TRANSPORT_BUTTON_HOLD_TIME 500 // time in ms for play button hold to start song mode

// remap pads from MPR121 readings which are not in order to desired keypad position
// top row - track,scene, start/stop, record
// next 4 rows x 4 columns keypad 0-15
// right column copy, paste
// ** note to self - uncomment the pad number code in loop() below and press PAD0 (Number pad 1) and enter its bit number as the definition for PAD0. etc etc
#define NPADS 24 // total number of pads on keypad
/*
#define PAD0 4   // pad 0 (ie number pad 1) as detected by MPR121
#define PAD1 9
#define PAD2 14
#define PAD3 19
#define PAD4 2
#define PAD5 7
#define PAD6 12
#define PAD7 17
#define PAD8 1
#define PAD9 6
#define PAD10 11
#define PAD11 16
#define PAD12 0
#define PAD13 5
#define PAD14 10
#define PAD15 15 // last number pad
*/
#define PAD0 0   // pad 0 (ie number pad 1) as detected by MPR121
#define PAD1 5
#define PAD2 10
#define PAD3 15
#define PAD4 1
#define PAD5 6
#define PAD6 11
#define PAD7 16
#define PAD8 2
#define PAD9 7
#define PAD10 12
#define PAD11 17
#define PAD12 4
#define PAD13 9
#define PAD14 14
#define PAD15 19 // last number pad
#define PAD16 3  // track
#define PAD17 8  // scene
#define PAD18 13 // play/stop
#define PAD19 18 // record
#define PAD20 20 // copy
#define PAD21 21 // paste
#define PAD22 22
#define PAD23 23 // shift

// maps MPR121 number to desired keypad
// note to self - this is the PAD numbers above in order of #define value
//uint8_t padmap[NPADS] = {12,8,4,0,16,13,9,5 ,1,17,14,10,6,2,18,15, 11,7,19,3,23,22,21,20}; 
//uint8_t padmap[NPADS] = {12,8,4,16,0,13,9,5 ,17,1,14,10,6,18,2,15, 11,7,19,3,20,21,22,23}; // for number pads with 1 top left
uint8_t padmap[NPADS] = {0,4,8,16, 12,1,5,9 ,17,13,2,6, 10,18,14,3, 7,11,19,15, 20,21,22,23};   // for number pads with 1 bottom left

// button masks
// button is the bit in the combined MPR121 keypad outputs
#define TRACK_BUTTON _BV(PAD16) // press to select track
#define SCENE_BUTTON _BV(PAD17) // press to select scene
#define TRANSPORT_BUTTON _BV(PAD18) // play/stop, double click for song mode
#define RECORD_BUTTON _BV(PAD19) // record, hold to delete
#define COPY_BUTTON _BV(PAD20)   // copy clip
#define PASTE_BUTTON _BV(PAD21)  // paste clip
#define EDIT_BUTTON _BV(PAD22)
#define SHIFT_BUTTON _BV(PAD23)
#define KEY1        _BV(PAD0) // number pads
#define KEY2        _BV(PAD1)
#define KEY3        _BV(PAD2)
#define KEY4        _BV(PAD3)
#define KEY5        _BV(PAD4) // number pads
#define KEY6        _BV(PAD5)
#define KEY7        _BV(PAD6)
#define KEY8        _BV(PAD7)
#define KEY9        _BV(PAD8) // number pads
#define KEY10        _BV(PAD9)
#define KEY11        _BV(PAD10)
#define KEY12        _BV(PAD11)
#define KEY13        _BV(PAD12) // number pads
#define KEY14        _BV(PAD13)
#define KEY15        _BV(PAD14)
#define KEY16        _BV(PAD15)

// mask to detect any number keypress
#define KEYSMASK (KEY1|KEY2|KEY3|KEY4|KEY5|KEY6|KEY7|KEY8|KEY9|KEY10|KEY11|KEY12|KEY13|KEY14|KEY15|KEY16)

// globals
int16_t bpm = TEMPO;
int16_t shuffle[NTRACKS]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,}; // swing/shuffle amount for each track 0-15
int16_t pattern[NTRACKS]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,}; // pattern from drumpatterns.h
int16_t patshift[NTRACKS]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,}; // pattern shift
int16_t patpitch[NTRACKS]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,}; // pattern note offsets
int16_t patvelocity[NTRACKS]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,}; // pattern velocity offsets
int16_t tracklevel[NTRACKS] = {500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500}; // track volume 
int16_t trackpan[NTRACKS] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,};  // track pan
uint32_t seqmillis; // millis() time to pass to sequencers

int8_t pitchoffsets[NTRACKS][MAX_STEPS]; // random values for pitch pattern

int8_t velocityoffsets[NTRACKS][MAX_STEPS]; // random values for velocity offset

// sequencer modes
bool record_mode = false;
bool play_mode = true;
bool song_mode = false;
bool edit_mode=false;
bool Copybutton, Pastebutton; // button down flags
bool shiftkey; // shift key state
uint16_t clip_complete,clipflags; // bitmap of sequencers that have just completed last step
//int16_t steps[NTRACKS] = { 16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16}; // steps for each track
int16_t steps[NTRACKS] = { 16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16}; // steps for each track
int16_t scenecount[NSCENES] = {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};  // how many times to repeat scene in song mode - values changed in song menu
int16_t rerandomize[NTRACKS] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};  // when true re-randomize notes and velocities at end of scene
uint8_t track=0;  // current track in UI
int16_t scene=0; // the current scene 
int16_t scenecounter=0; // counts down scene repeats 

int16_t sequencer=0; // the sequencer currently running
int16_t current_step=0;
int16_t last_step=0; // so we can tell when the sequencer advanced
int16_t last_scene=0; // so we can tell when the song scene sequencer advanced
int16_t stepsperbar=STEPS_PER_BAR; // used by the UI to show bar:step
int16_t current_scale=1; // musical scale in use
int16_t master_volume = 64;
int16_t shift=0; // shift flag used by menu system 0= shift back, 1 = shift right
int16_t clipbuffercnt =0; // current size of clip buffer (in notes)
SixteenStepNote clipbuffer[MAX_STEPS]; // buffer used for clip copy/paste
SixteenStepNote scenebuffer[NTRACKS][MAX_STEPS]; // buffer used for scene copy/paste

// sequencer per track allows tracks to have different lengths and it allows clearing each track individually
// more overhead but its a lot more flexible
SixteenStep seq[NTRACKS] = { 
  SixteenStep(SEQUENCER_MEMORY), // MIDI recorder/sequencer
  SixteenStep(SEQUENCER_MEMORY),
  SixteenStep(SEQUENCER_MEMORY),
  SixteenStep(SEQUENCER_MEMORY),
  SixteenStep(SEQUENCER_MEMORY),
  SixteenStep(SEQUENCER_MEMORY),
  SixteenStep(SEQUENCER_MEMORY),
  SixteenStep(SEQUENCER_MEMORY),
  SixteenStep(SEQUENCER_MEMORY), // MIDI recorder/sequencer
  SixteenStep(SEQUENCER_MEMORY),
  SixteenStep(SEQUENCER_MEMORY),
  SixteenStep(SEQUENCER_MEMORY),
  SixteenStep(SEQUENCER_MEMORY),
  SixteenStep(SEQUENCER_MEMORY),
  SixteenStep(SEQUENCER_MEMORY),
  SixteenStep(SEQUENCER_MEMORY),
};

I2S DAC(OUTPUT);  // using PCM1103 stereo DAC

Adafruit_MPR121 padA = Adafruit_MPR121(); // touch keyboard used for note entry
Adafruit_MPR121 padB = Adafruit_MPR121(); // touch keyboard used for note entry


// chromatic pitch table - maps a midi note to a 12 bit pitch step - see voices below for how pitch step works
uint32_t pitchtable[128]= {
128,136,144,152,161,171,181,192,203,215,228,242,
256,271,287,304,323,342,362,384,406,431,456,483,
512,542,575,609,645,683,724,767,813,861,912,967,
1024,1085,1149,1218,1290,1367,1448,1534,1625,1722,1825,1933,
2048,2170,2299,2435,2580,2734,2896,3069,3251,3444,3649,3866,
4096,4340,4598,4871,5161,5468,5793,6137,6502,6889,7298,7732,
8192,8679,9195,9742,10321,10935,11585,12274,13004,13777,14596,15464,
16384,17358,18390,19484,20643,21870,23170,24548,26008,27554,29193,30929,
32768,34716,36781,38968,41285,43740,46341,49097,52016,55109,58386,61858,
65536,69433,73562,77936,82570,87480,92682,98193,104032,110218,116772,123715,
131072,138866,147123,155872,165140,174960,185364,196386
};

// table maps pads to MIDI note numbers - pad 9 is middle C. 
// rows are scales: CHROMATIC,MAJOR,MINOR,HARMONIC_MINOR,MAJOR_PENTATONIC,MINOR_PENTATONIC,DORIAN,PHRYGIAN,LYDIAN,MIXOLYDIAN
uint8_t padtoMIDI[10][16] = {
52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67, // chromatic
47,48,50,52,53,55,57,59,60,62,64,65,67,69,71,72, // major
48,50,51,55,53,55,56,58,60,62,63,65,67,68,70,72, // minor 0b10110101101
47,48,50,51,53,55,56,59,60,62,63,65,67,68,71,72, // harmonic minor
40,43,45,48,50,52,55,57,60,62,64,67,69,72,74,76, // major penta
41,43,46,48,51,53,55,58,60,63,65,67,70,72,75,77,  // minor penta
46,48,50,51,53,55,57,58,60,62,63,65,67,69,70,72,  // dorian
46,48,49,51,53,55,56,58,60,61,63,65,67,68,70,72, // phrygian
47,48,50,52,54,55,57,59,60,62,64,66,67,69,71,72, // lydian
46,48,50,52,53,55,57,58,60,62,64,65,67,69,70,72, // mixolydian
};


// maps random pitch offsets so they can be quantized to scale
// ie as we increase pitch offsets we want to add more notes above and below the root note
uint8_t pitchremap[NPADS]= {8,7,9,6,10,5,11,4,12,3,13,2,14,1,15,0};

// maps velocity levels to colors
uint16_t vcolors[8] = {MAGENTA,CYAN,BLUE,LIGHTGREEN,GREEN,ORANGE,YELLOW,RED};


/* no MIDI for now
// MIDI stuff
uint8_t MIDI_Channel=10;  // default MIDI channel for percussion
struct SerialMIDISettings : public midi::DefaultSettings
{
  static const long BaudRate = 31250;
};


// must use HardwareSerial for extra UARTs
HardwareSerial MIDISerial(2);

// instantiate the serial MIDI library
MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, MIDISerial, MIDI, SerialMIDISettings);
*/


// I'm using the same structure for psram samples loaded from SD as the original code with flash based samples
// there are some unused elements in this structure which were used by older code but I'm leaving them here for now
// perhaps a bit convoluted but this way the code doesn't change significantly and in future both flash and SD could be used for sample storage
struct sample_t {
  int16_t * samplearray; // pointer to sample array
  uint32_t samplesize; // size of the sample array
  uint32_t sampleindex; // current sample array index when playing. index at last sample= not playing - NOT USED
  uint8_t MIDINOTE;  // MIDI note on that plays this sample - NOT USED
  uint8_t play_volume; // play volume 0-127 - NOT USED
  char sname[25];        // sample name
} sample[NUM_VOICES];

#define NUM_SAMPLES (sizeof(sample)/sizeof(sample_t)) // for PSRAM this will always be the same as NUM_VOICES

// initialize samples 
void init_samples(void) {
  for (int i=0; i< NUM_VOICES; ++i) {
    sample[i].samplearray=0; // start with a null pointer
    sample[i].samplesize=0;
    strcpy(sample[i].sname,"Click to load from SD");  // no sample loaded
  }
}

// voice structure holds info for the sample in use on each track
// variable pitch is done by stepping thru the sample at diferent rates as determined by sampleincrement which is a 20:12 fixed point number
// the 2nd core adds sampleincrement to sampleindex, then interpolates the sample values when a new sample is needed
// note that both CPU cores use the voice data structure but sampleindex is modified by 2nd core only so we don't get read-modify-write issues between cores
// note that there is some other stuff in the sample structures included above. Its used by other sketches but not using it here
struct voice_t {
  int16_t sample;   // index of sample in use - note menusystem requires signed ints
  int16_t levelL;   // 0-128 - ideally this should be pan control vs L-R levels
  int16_t levelR;     // 0-128
  uint32_t sampleindex; // 20:12 fixed point index into the sample array
  uint32_t sampleincrement; // 20:12 fixed point sample step for pitch changes
  uint32_t samplesize; // number of samples
  int16_t tune;  // fine tuning of sample pitch  
  uint8_t note; // current MIDI note 
  uint8_t velocity; // midi velocity
  int16_t slices; // number of slices
} voice[NUM_VOICES]; 

// initialize voices 
void init_voices(void) {
  for (int i=0; i< NUM_VOICES; ++i) { 
    voice[i].sample=i*(NUM_SAMPLES/NUM_VOICES); // init to use samples spread out over the collection - less knob turning

    voice[i].levelR=(map(tracklevel[i],0,1000,0,128)*map(trackpan[i],-1000,1000,0,128))/128;
    voice[i].levelL=(map(tracklevel[i],0,1000,0,128)*map(trackpan[i],-1000,1000,128,0))/128;
    voice[i].sampleincrement=pitchtable[MIDDLE_C]; // 20:12 fixed point sample step for pitch changes
    voice[i].tune=0;  // no pitch adjustment
    voice[i].note= MIDDLE_C; // current MIDI note
    voice[i].velocity=DEFAULT_LEVEL;
    voice[i].slices=0; // no slices
    // silence all voices by setting sampleindex to last sample
    voice[i].sampleindex=sample[voice[i].sample].samplesize<<12; // sampleindex is a 20:12 fixed point number
    voice[i].samplesize=sample[voice[i].sample].samplesize; // 
  } 
}

// encoder 
ClickEncoder Encoder1(ENC_A,ENC_B,ENC_SW,4); // divide by 4 works best with my encoder

// interrupt timer defs
#define ENC_TIMER_MICROS 250 // 4khz for encoder
#define SEQ_INTERVAL ((1000/ENC_TIMER_MICROS)*5)  // update sequencers every 5 ms - 300bpm is 20 steps per second=50ms
// Init RPI_PICO_Timer
//RPI_PICO_Timer ITimer(0);

// RP2040 timer code from https://github.com/raspberrypi/pico-examples/blob/master/timer/timer_lowlevel/timer_lowlevel.c
// Use alarm 0
#define ALARM_NUM 0
#define ALARM_IRQ timer_hardware_alarm_get_irq_num(timer_hw, ALARM_NUM)

static void alarm_in_us(uint32_t delay_us) {
  hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);
  irq_set_exclusive_handler(ALARM_IRQ, alarm_irq);
  irq_set_enabled(ALARM_IRQ, true);
  alarm_in_us_arm(delay_us);
}

static void alarm_in_us_arm(uint32_t delay_us) {
  uint64_t target = timer_hw->timerawl + delay_us;
  timer_hw->alarm[ALARM_NUM] = (uint32_t) target;
}

static void alarm_irq(void) {
  static int8_t seqcnt;
  Encoder1.service();    // check the encoder inputs
  if (--seqcnt <=0) {  // update sequencers every seqcnt interrupts
    dosequencers(); // update all the sequencers at the same time so they stay in sync
    seqcnt=SEQ_INTERVAL;
  }
  hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM); // clear IRQ flag
  alarm_in_us_arm(ENC_TIMER_MICROS);  // reschedule interrupt
}

#include "loadwav.h" // to avoid forward references
#include "seq_editor.h" // to avoid forward references
#include "menusystem.h" // to avoid forward references


/* no MIDI for now
// serial MIDI handler

void HandleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
  if (channel==MIDI_Channel) {
      for (int i=0; i< NUM_SAMPLES;++i) {  // 
        if (sample[i].MIDINOTE == note) {
          sample[i].play_volume=velocity;  // use MIDI volume
          sample[i].sampleindex=0;  // if note matches sample MIDI note number, start it playing 
        }
      }
  }
}
*/

// turn off all voices 
void allnotesoff(void) {
  for (int track=0; track< NUM_VOICES; ++track) rp2040.fifo.push((0x80 | track) <<24);  // tell other core to turn off this voice  
}

// rotate trigger pattern
uint16_t rightRotate(int shift, uint16_t value, uint8_t pattern_length) {
  uint16_t mask = ((1 << pattern_length) - 1);
  value &= mask;
  return ((value >> shift) | (value << (pattern_length - shift))) & mask;
}

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
//                         SEQUENCER CALLBACKS                               //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

// called when the sequencer needs the time
// we provide all sequencers the same time on every call to run()
// otherwise time skew builds up and they go out of sync
uint32_t seqtime(void) {
  return seqmillis;
}



// called when the step position changes. both the current
// position and last are passed to the callback
// note that every sequencer uses this same callback so we use the current track number
// *** note this runs in the interrupt when we call dosequencers()
void step_pos(int current, int last) {

  if (current == steps[sequencer]-1) { 
    clip_complete |= _BV(sequencer); // set flag if on last step
  }
  else clip_complete &= ~(_BV(sequencer)); // clip_complete is used to determine when all clips are on last step - for song mode

  if (sequencer == track) current_step=current; // this triggers display update in loop() for the current track  
}

// the callback that will be called by the sequencer when it needs to play notes
// note that every sequencer uses this same callback 
// high nybble of channel = scene, low nybble = track
// each track's sequencer holds all the notes (clips) for all scenes on that track
// the sequencers play recorded notes from all scenes but we only sound notes from the current scene
// originally I used a sequencer for every clip but its a lot of overhead
// *** note this runs in the interrupt when we call dosequencers()
void step_play(byte channel, byte command, byte arg1, byte arg2) {
  byte track=channel & 0xf;   // recorded track
  byte s=(channel & 0xf0)>>4; // recorded scene 

  if (s == scene) {
    switch (command) {
      case 0x9:  // note on
        voice[track].note=arg1; // save note for this voice
        voice[track].velocity=arg2;
        rp2040.fifo.push(((0x90 | track)<<24) | (arg2 <<16));  // tell other core to play this voice  
        break;
      case 0x8: // note off - do nothing
        break;
    }
  }
}

// stop all sequencers
void stop_sequencers(void) {
  for (int8_t i=0; i< NTRACKS;++i)  seq[i].stop();
}

// start all sequencers
void start_sequencers(void) {
  for (int8_t i=0; i< NTRACKS;++i)  seq[i].start();
}

// menu callback functions
// set number of steps in current track from current bar setting
void setsteps(void) {
  seq[track].setSteps(steps[track]); // fixed at 16 beats per bar
//  play_mode=false;  // stop playing - starting up will restart all sequencers so everything stays in sync
//  stop_sequencers();
  start_sequencers(); // restart the sequencers to keep tracks in sync
  showpattern(track); 
}

// menu callback - set L & R voice levels from level and pan
// better to do this from a menu callback than in the sample playing loop
void setlevels() {
  voice[track].levelR=(map(tracklevel[track],0,1000,0,128)*map(trackpan[track],-1000,1000,0,128))/128;
  voice[track].levelL=(map(tracklevel[track],0,1000,0,128)*map(trackpan[track],-1000,1000,128,0))/128;
}

// menu callback -set all tracks to the same tempo
void settempo(void) {
  for (int8_t i=0; i< NTRACKS;++i)  seq[i].setTempo(bpm);
}

// menu callback - set shuffle amount for current track
void setshuffle(void){
  seq[track].setShuffle(shuffle[track]); 
}



// menu callback - set pattern for current track
// works like paste - keep repeating till scene steps are full
void setpattern(void){
  uint16_t pat,mask;
  int16_t s;
  seq[track].removeNotes(scene <<4 | track); // clear scene contents
  pat=rightRotate(patshift[track],drumpatterns[pattern[track]],STEPS_PER_BAR); // rotate the pattern per the menu setting
//  Serial.printf("%4x\n",pat);
  s=0;
  mask=0x8000;
  for (int n=0; n < steps[track]; ++n) {
    if ((pat & mask) !=0) { 
      if (voice[track].slices !=0) seq[track].setNote(n,scene <<4 | track,MIDDLE_C+pitchoffsets[track][n],DEFAULT_LEVEL+velocityoffsets[track][n]); // if pattern bit is set insert slice number
      else seq[track].setNote(n,scene <<4 | track,padtoMIDI[current_scale][pitchremap[pitchoffsets[track][n]]],DEFAULT_LEVEL+velocityoffsets[track][n]); // else insert note from current scale
      
  //    int8_t velocityoffset=constrain(random(-(patvelocity[track]*4),patvelocity[track]*4),0,127);  // generate a random velocity offset
  //    if (voice[track].slices !=0) seq[track].setNote(n,scene <<4 | track,MIDDLE_C+random(0,patpitch[track]+1),DEFAULT_LEVEL+velocityoffset); 
      // else insert note from current scale
  //    else seq[track].setNote(n,scene <<4 | track,padtoMIDI[current_scale][pitchremap[random(0,patpitch[track]+1)]],DEFAULT_LEVEL+velocityoffset); 
    }
    mask=mask>>1;
    ++s;
    if (s == STEPS_PER_BAR) {
      s=0; // end of pattern, repeat
      mask=0x8000;
    }
  }
  showpattern(track); 
}

// update display to show sequencer position
void showposition (int16_t position) {
  display.setCursor(0,DISPLAY_Y_OFFSET);
  display.printf("T%d:%d ",track+1,scene+1); 
  display.setCursor(39,DISPLAY_Y_OFFSET);

  if (song_mode) {
    display.setTextColor(ORANGE,BLACK); // foreground, background 
    display.printf("SONG"); 
    display.setTextColor(WHITE,BLACK); // foreground, background 
  }    
  else if (edit_mode) {   // we can't be in edit mode and song mode at the same time
    display.setTextColor(GREEN,BLACK); // foreground, background 
    display.printf("EDIT %d",editcursorX+1);
    if ((editnote <= HIGHEST_NOTE) && (editnote >= LOWEST_NOTE)) {
      display.setTextColor(GREEN,BLACK);
      display.printf(" %d  ",editnote);
    }
    else display.printf("      ");  // erases old shit if its there
    display.setTextColor(WHITE,BLACK); // foreground, background 
  }    
  else display.printf("        "); // erases old shit if its there

  display.setCursor(107,DISPLAY_Y_OFFSET);
  if (record_mode) {
    display.setTextColor(RED2,BLACK); // foreground, background 
    display.printf("REC"); 
    display.setTextColor(WHITE,BLACK); // foreground, background 
  }
  else display.printf("   "); // erases old shit if its there
 //     display.setCursor(95,0);
 //     display.print("   ");
  display.setCursor(128,DISPLAY_Y_OFFSET);
  display.printf("%d:%d ", position/STEPS_PER_BAR+1,position%STEPS_PER_BAR+1);  // bar:step display 
  uint32_t pos;
  pos= (position+1)*160/(steps[track]); // sequencer position bar
  display.drawRect(0, 15,pos,2, YELLOW);
  display.drawRect(pos, 15,160-pos,2, BLACK);
//  display.display();
}

// create random pitch offsets to add to pattern
void pitchrandomizer(void) {  
  for (uint16_t i=0;i<MAX_STEPS;++i) pitchoffsets[track][i]=random(0,patpitch[track]+1);
  setpattern();
}

// create random velocity offsets to add to pattern
// offsets add and subtract from default velocity level of 64
void velocityrandomizer(void) {
  for (uint16_t i=0;i<MAX_STEPS;++i) velocityoffsets[track][i]=constrain(random(-(patvelocity[track]*4),patvelocity[track]*4),0,128);
  setpattern();
}


// process sequencers - must be called frequently to keep the sequencers running.
// we also process song mode here
// Oct 14/24 **** changed to run under interrupts for accurate timing
// the sequencer library was also modded to disable interrupts during note writes
void dosequencers(void) {
  seqmillis=millis(); // freeze current time while we run the sequencers so they stay in sync
  for (sequencer=0; sequencer< NTRACKS; ++sequencer) seq[sequencer].run();

  if ((clip_complete == CLIPS_COMPLETE) && song_mode) {  // all clips complete
    --scenecounter; // count down scene repeats
    clip_complete=0;
    //Serial.printf("scenecounter %d \n",scenecounter);
    if (scenecounter <=0) { 
      ++scene;
      if (scene >= NSCENES) scene=0; // back to start of song
      while (scenecount[scene]==0) { // skip over unused scenes
        ++scene;
        if (scene >= NSCENES) scene=0; // back to start of song
      }
//      if (scene == 0) allnotesoff(); // back to beginning of song so silence all notes 
      //Serial.printf("advance to scene %d \n",scene);
//      allnotesoff(); // silence all notes for new scene **** causes a crash now that its running in an interrupt
      scenecounter=scenecount[scene];
    }
  }

}

// save all the notes in track, scene to the clip buffer
void copyclip(int16_t track, int16_t scene) {
  SixteenStepNote * p;
  for (int n=0; n < steps[track]; ++n) {
    p=seq[track].getNote(n,scene <<4 | track); // retrieve notes by channel
    memcpy(&clipbuffer[n],p,sizeof(SixteenStepNote));
  }
  clipbuffercnt=steps[track]; // keep size because paste destination may not be the same
}

// paste the notes in the clip buffer to track, scene
// if src clip smaller than dest clip, repeat paste till dest clip full
void pasteclip(int16_t track, int16_t scene) {
  SixteenStepNote * p;
  int16_t s;
  s=0;
  for (int n=0; n < steps[track]; ++n) {
    seq[track].setNote(n,scene <<4 | track,clipbuffer[s].pitch,clipbuffer[s].velocity); 
    ++s;
    if (s >= clipbuffercnt) s=0; // end of source clip, repeat
  }
  showpattern(track); 
}

// save all clips in scene to the scene buffer
void copyscene( int16_t scene) {
  SixteenStepNote * p;
  for (int track=0;track< NTRACKS;++track) {
    for (int n=0; n < steps[track]; ++n) {
      p=seq[track].getNote(n,scene <<4 | track); // retrieve notes by channel
     // Serial.printf("step %2d ch %02X note %02X\n", n, p->channel,p->pitch);
      memcpy(&scenebuffer[track][n],p,sizeof(SixteenStepNote));
    }
  }
}

// paste the clips in the scene buffer to scene
void pastescene(int16_t scene) {
  SixteenStepNote * p;
  for (int track=0;track< NTRACKS;++track) {
    for (int n=0; n < steps[track]; ++n) {
      seq[track].setNote(n,scene <<4 | track,scenebuffer[track][n].pitch,scenebuffer[track][n].velocity); 
    }
  }
  showpattern(track); 
}

// **** not working - doesn't move last note to first correctly
// first note propagates forward properly
// seems to be something about the way notes are allocated in setnote() during recording
// I would have thought all the empty notes would be sorted to the end of memory but they are at the beginning
// seems like a bug to me - you will allocate a new note before you find the old one to write over
// Fifteenstep was written to be polyphonic ie multiple notes at same step and I'm using it monophonic so maybe thats the underlying issue

/*
// shift all notes in clip forward one step
void shiftclipfwd(int16_t track, int16_t scene) {
  FifteenStepNote * src, * dest, lastnote;
  if (steps[track]==1) return; // nothing to do
  src=seq[track].getNote(steps[track]-1,scene <<4 | track); // get last note
//  Serial.printf("src pointer channel %d pitch %d velocity %d\n",src->channel,src->pitch,src->velocity );
  memcpy(&lastnote,src,sizeof(FifteenStepNote)); // save it
    Serial.printf("lastnote channel %d pitch %d velocity %d\n",lastnote.channel,lastnote.pitch,lastnote.velocity);
  for (int n=steps[track]-1; n > 0; --n) { // start at end and copy n-1 to n
    src=seq[track].getNote(n-1,scene <<4 | track); // get source note pointer
    seq[track].setNote(n,src->channel,src->pitch,src->velocity); 
 //   dest=seq[track].getNote(n,scene <<4 | track); // get destination pointer
 //   memcpy(dest,src,sizeof(FifteenStepNote)); // ** use memcpy here. src may point to DEFAULT_NOTE ie empty step
  }
//  dest=seq[track].getNote(0,scene <<4 | track); // get pointer to first note
//  memcpy(dest,&lastnote,sizeof(FifteenStepNote));  // copy last note to first position
  seq[track].setNote(0,lastnote.channel,lastnote.pitch,lastnote.velocity);
 // src=seq[track].getNote(0,scene <<4 | track);
 // Serial.printf("firstnote channel %d pitch %d velocity %d\n",src->channel,src->pitch,src->velocity );
  seq[track].dumpNotes();
}

// **** broken - causes a crash. once I get above working fix this
// shift all notes in clip back one step
void shiftclipback(int16_t track, int16_t scene) {
  FifteenStepNote * src, *dest, firstnote;
  if (steps[track]==1) return; // nothing to do
  src=seq[track].getNote(0,scene <<4 | track); // get first note
  memcpy(&firstnote,src,sizeof(FifteenStepNote)); // save it
  for (int n=0; n < steps[track]-1; ++n) { // start at beginning and copy n+1 to n
    src=seq[track].getNote(n+1,scene <<4 | track); // get source note pointer
    dest=seq[track].getNote(n,scene <<4 | track); // get destination pointer
    memcpy(dest,src,sizeof(FifteenStepNote)); // ** use memcpy here. src may point to DEFAULT_NOTE ie empty step
   // seq[track].setNote(n,scene <<4 | track,src->pitch,src->velocity); 
  }
  dest=seq[track].getNote(steps[track]-1,scene <<4 | track); // get pointer to last note
  memcpy(dest,&firstnote,sizeof(FifteenStepNote)); // copy first note to last position
  // seq[track].setNote(steps[track]-1,scene <<4 | track,firstnote.pitch,firstnote.velocity);  // copy last note to first position
}


// menu callback function to shift clip
void shiftclip(void) {
  if (shift == 1) shiftclipfwd(track,scene);
  else if (shift == -1)shiftclipback(track,scene);
}
*/

// main core setup
void setup() {
//  WiFi.forceSleepBegin();             


 Serial.begin(115200);
 //while (!Serial);  // wait for serial port

 Serial.print("Number of Samples ");
 Serial.println(NUM_SAMPLES);      

  SPI.setRX(SPI0_MISO);  // assign RP2040 SPI0 pins
  SPI.setCS(SPI0_CS);
  SPI.setSCK(SPI0_SCLK);
  SPI.setTX(SPI0_MOSI);

/* using SDIO for SD card now
  SPI1.setRX(SPI1_MISO);
  SPI1.setCS(SPI1_CS);
  SPI1.setSCK(SPI1_SCLK);
  SPI1.setTX(SPI1_MOSI );
*/

  Wire.setSDA(SDA);
  Wire.setSCL(SCL);


// set up I/O pins
 
#ifdef MONITOR_CPU1 // for monitoring 2nd core CPU usage
  pinMode(CPU_USE,OUTPUT); // hi = CPU busy
#endif 

#ifdef HW_DEBUG // for monitoring other stuff
  pinMode(DEBUG_PIN,OUTPUT); // hi = CPU busy
#endif 

//  pinMode(SD_CS, OUTPUT);
//  digitalWrite(SD_CS,1);

    // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
//  display.begin(SH1106_SWITCHCAPVCC);  // initialize display
  // Use this initializer if using a 1.8" TFT screen:
  display.initR(INITR_BLACKTAB);      // Init ST7735S chip, black tab
  display.setRotation(3);

  //  display.clearDisplay();
 display.fillScreen(BLACK);
// text display tests
  display.setTextSize(1);

	display.setTextColor(WHITE,BLACK); // foreground, background  
  display.setCursor(10,20);
  display.printf("  Pico Groovebox\n\n");
//  display.printf("   %d Samples Loaded",NUM_SAMPLES); 
//  display.display();
  delay(2000);

  spi_init(spi0, 30 * 1000000); // crank up the display SPI clock after the display is running

  // bail if the mpr121 init fails
  if (! padA.begin(0x5A)) {
      display.println("MPR121 A Not found");
 //     while(1);
  }
  // bail if the mpr121 init fails
  if (! padB.begin(0x5C)) {
      display.println("MPR121 B Not found");
 //     while(1);
  }

  padA.setThresholds(13,8); // make pads a bit more sensitive - sensitivity dropped a lot with labels stuck on them
  padB.setThresholds(13,8);
  padA.setThreshold(PAD16, 6, 4); // track button is problematic
  padA.setThreshold(PAD10, 8, 5); // "3" button is problematic as well

// set up Pico I2S for PT8211 stereo DAC
	DAC.setBCLK(BCLK);
	DAC.setDATA(I2S_DATA);
	DAC.setBitsPerSample(16);
	DAC.setBuffers(1, 128, 0); // DMA buffer - 32 bit L/R words
	//DAC.setLSBJFormat();  // needed for PT8211 which has funny timing
	DAC.begin(SAMPLERATE);

  display.println("Initializing SD card...");
 // if (!(SD.begin(SPI1_CS, SPI1))) {
  if (!sd.begin(SD_CONFIG)) {
    display.println("SD initialization failed!");
    while (1);
  }
  spi_init(spi1, 50 * 1000000); // crank up the SD SPI clock after its running
  display.println("SD initialization done.");

  delay(1000);
  display.fillScreen(BLACK);
// initialize the sample and voice data structures
  init_samples();
  init_voices();

/* no MIDI for now	
//  Set up serial MIDI port
  MIDISerial.begin(31250, SERIAL_8N1, MIDIRX,MIDITX ); // midi port

  // set up serial MIDI library callbacks
  MIDI.setHandleNoteOn(HandleNoteOn);  // 

  // Initiate serial MIDI communications, listen to all channels
  MIDI.begin(MIDI_CHANNEL_OMNI);
*/

  // init the menu system
	menutitle=maintitle;
  uistate=TOPSELECT;
  strcpy(filepath,filesroot); // set up root path
   
    // start sequencers and set callbacks
  for (int i=0; i< NTRACKS; ++i) {  
    seq[i].begin();  // this uses the library default settings with lots of memory
    seq[i].setTempo(TEMPO);
    seq[i].setSteps(steps[i]);
    seq[i].setMidiHandler(step_play);
    seq[i].setStepHandler(step_pos);
    seq[i].setTimeHandler(seqtime);
  }

  for (int t=0; t< NTRACKS; ++t) {  // make sure we start off with no random offsets
    for (int i=0; i< MAX_STEPS;++i) {
      velocityoffsets[t][i]=pitchoffsets[t][i]=0; 
    }
  }
  // start up the timer interrupt
  alarm_in_us(ENC_TIMER_MICROS);
}


// main core handles UI
void loop() {
  float pitch, retune;
  static uint32_t recbutton_timer,transportbutton_timer;
  static bool trackerased,startsongmode;


  if (edit_mode) editnotes();  // note editor needs encoder so its mutually exclusive from menus
  else  domenus();

  // first 16 menu pages are track/voice settings which have extra elements on screen
  if (topmenuindex < NTRACKS) {
    track = topmenuindex; // map menu index to track/voice number
    if (last_step != current_step) { // sequencer has advanced so update display
      showposition(current_step);  // moved this out of the sequencer callback to reduce display overhead
      last_step=current_step;
    }
    if (last_scene != scene) { // scene sequencer has advanced so update piano roll display
      showpattern(track);  // 
      last_scene=scene;
    }
  }

// re-randomize at end of scene if user has selected to in menus
  if (clipflags!=clip_complete) {  
    uint16_t clips=clip_complete;  // bitmap of clips that just completed
    for (int16_t t=0; t<NTRACKS; ++t) {
      if ((clips & 1) && rerandomize[t] ) {  // find clips that just ended
        for (int16_t s=0;s<steps[t];++s) {
          pitchoffsets[t][s]=random(0,patpitch[t]+1);
          velocityoffsets[t][s]=constrain(random(-(patvelocity[t]*4),patvelocity[t]*4),0,128);
        }
        uint16_t pat,mask;  // this is just a copy of setpattern() code but it doesn't use the "track" global
        int16_t step;
        seq[t].removeNotes(scene <<4 | t); // clear scene contents
        pat=rightRotate(patshift[t],drumpatterns[pattern[t]],STEPS_PER_BAR); // rotate the pattern per the menu setting
        step=0;
        mask=0x8000;
        for (int n=0; n < steps[t]; ++n) {
          if ((pat & mask) !=0) { 
            if (voice[t].slices !=0) seq[t].setNote(n,scene <<4 | t,MIDDLE_C+pitchoffsets[t][n],DEFAULT_LEVEL+velocityoffsets[t][n]); // if pattern bit is set insert slice number
            else seq[t].setNote(n,scene <<4 | t,padtoMIDI[current_scale][pitchremap[pitchoffsets[t][n]]],DEFAULT_LEVEL+velocityoffsets[t][n]); // else insert note from current scale
          }
          mask=mask>>1;
          ++step;
          if (step == STEPS_PER_BAR) {
            step=0; // end of pattern, repeat
            mask=0x8000;
          }
        }
        if (t == track)showpattern(track); 
      }
      clips=clips>>1;
    }
    clipflags=clip_complete; // so we wait for next change
  }

// handle touch pad used for note entry, track and scene selection

  currtouched = padA.touched() | (padB.touched()<<(NPADS/2)); // combine both pads

// process the edit key
  if ((currtouched & EDIT_BUTTON)  && !(lasttouched & EDIT_BUTTON)) {
    edit_mode=!edit_mode; // toggle note editor on/off
    if (edit_mode) {
      song_mode=false; // can't edit in song mode
      editstate=INIT_EDITOR;  // reset editor state machine
    }
    else {
      eraseeditcursor(editcursorX);
      Encoder1.getButton(); // resets encoder button state which may not have been read
    }
  }

// save shift key state
  if (currtouched & SHIFT_BUTTON) shiftkey=true;
  else shiftkey=false;

// process play/stop/song mode button
  if ((currtouched & TRANSPORT_BUTTON) && !(lasttouched & TRANSPORT_BUTTON)) {  // transport button pressed, start timing
    transportbutton_timer=millis();
    startsongmode=false;
  }
  uint32_t tholdtime=millis()-transportbutton_timer;
  if ((currtouched & TRANSPORT_BUTTON) && (tholdtime > TRANSPORT_BUTTON_HOLD_TIME) && !startsongmode) { //transport button held
      song_mode=!song_mode;
      if (song_mode) {
   //     play_mode=false; // stop playing  
        stop_sequencers(); // reset all sequencers to step 0
        start_sequencers();
        noInterrupts(); // scenes change in the ISR so stop it running
        scene=0;  // start at first scene
        scenecounter=scenecount[scene];
        interrupts();
      }
      showposition(0); // update the screen in case sequencers are stopped
      startsongmode=true;      // so we only do this once per key hold
  }
  if (!(currtouched & TRANSPORT_BUTTON) && (lasttouched & TRANSPORT_BUTTON)) {  // transport button released   
    if ( tholdtime < TRANSPORT_BUTTON_HOLD_TIME) {
      play_mode=!play_mode;
      if (play_mode) start_sequencers();
      else stop_sequencers();
    }
  }

// process record button
  if ((currtouched & RECORD_BUTTON) && !(lasttouched & RECORD_BUTTON)) {  // record button pressed, start timing
    recbutton_timer=millis();
    trackerased=false;
  }
  uint32_t recholdtime=millis()-recbutton_timer;
  if ((currtouched & RECORD_BUTTON) && (recholdtime > RECBUTTON_HOLD_TIME) && !trackerased) { //record button held
    seq[track].removeNotes(scene <<4 | track); //  hold record to clear scene
//    allnotesoff(); // silence all notes that may have been playing
    trackerased=true; // if we keep erasing it causes audio noise
    if (topmenuindex < NTRACKS) showpattern(topmenuindex); // show the piano roll if this is a track menu
 //   Serial.printf("track erased\n");
  }
  if (!(currtouched & RECORD_BUTTON) && (lasttouched & RECORD_BUTTON)) {  // record button released   
    if ( recholdtime < RECBUTTON_HOLD_TIME) {
        record_mode=!record_mode;
    }
    showposition(0);  // update the screen in case sequencers are stopped
  }


// process copy button
  if ((currtouched & COPY_BUTTON) && !(lasttouched & COPY_BUTTON)) {  // copy button pressed
    if (currtouched & SCENE_BUTTON) copyscene(scene);
    else copyclip(track,scene);  
  }

// process paste button
  if ((currtouched & PASTE_BUTTON) && !(lasttouched & PASTE_BUTTON)) {  // paste button pressed
    if (currtouched & SCENE_BUTTON) pastescene(scene);
    else pasteclip(track,scene);  
  }

// debug
/*
  if ((currtouched & EXTRA_BUTTON) && !(lasttouched & EXTRA_BUTTON)) {  
    seq[track].dumpNotes();
  }
*/

// process number pads - note entry, track and scene select etc
  for (uint8_t i=0; i<NPADS; i++) { // have to scan all the pads because they are not in order
    if (padmap[i] < 16) { // process just the number pads
      if ((currtouched & _BV(i)) && !(lasttouched & _BV(i)) ) { // if pad was pressed
       // Serial.printf("numpad %d curtouched %04x %02x\n",padmap[i],currtouched, padA.touched());
        if ((currtouched & SCENE_BUTTON) && (!(currtouched & COPY_BUTTON)) && (!(currtouched & PASTE_BUTTON))) {
         // Serial.printf("scene \n");
          if (!edit_mode) scene=padmap[i];  // scene button + pad = change scene
          showpattern(track); // update piano roll for this scene
        }
        // else if ((currtouched & TRACK_BUTTON) && (!edit_mode)) { // track button + pad = change track
        else if ((padA.touched() & TRACK_BUTTON) && (!edit_mode)) { // track button + pad = change track
       // Serial.printf("track %d\n",padmap[i]);
          track=topmenuindex=padmap[i];  // *** kludgy - force menu to that track
          uistate=SUBSELECT; // do submenu when button is released
          topmenu[topmenuindex].submenuindex=0;  // start from the first item
          drawsubmenus();
          drawselector(topmenu[topmenuindex].submenuindex);
          showpattern(track); // update piano roll for this track
        }
        
        else { // use the keypad to enter notes
          if (voice[track].slices == 0) { // normal pitched playback
            voice[track].note=padtoMIDI[current_scale][padmap[i]];
            voice[track].velocity=DEFAULT_LEVEL;
          }
          else {  // slice playback mode
            voice[track].note=padmap[i]+MIDDLE_C;
          }
      // if recording, save note on
          if(record_mode) {
            seq[track].setNote(scene <<4 | track, voice[track].note, DEFAULT_LEVEL); // record note with fixed velocity
          //seq[track].dumpNotes();
          }  
          rp2040.fifo.push(((0x90 | track)<<24) | (DEFAULT_LEVEL <<16));  // tell other core to play this voice  
          showpattern(track);      
        }
      }
 /*  removed note offs - they just chew up memory
     if (!(currtouched & _BV(i)) && (lasttouched & _BV(i)) ) { // if pad just released
           // if recording, save note off
        if ((record_mode) && (voice[track].slices == 0)) seq[track].setNote(scene << 4 | track, padtoMIDI[current_scale][padmap[i]], 0x0); // pitch mode note off
        else seq[track].setNote(scene <<4 | track, padmap[i]+MIDDLE_C, 0); // slice mode note off
       
      } */
    }
  }

  lasttouched = currtouched; // reset pad state


// draw a simple battery level bar on the right side of the display - I'm using a Lipoly battery
  float batteryvoltage=(float) analogRead(BAT_VOLTAGE); // note Pimironi Plus board analog ports are not the same as the Pico
 //   Serial.println(analogRead(27));
  batteryvoltage=batteryvoltage*5.96/1024; // has external voltage divider

  batteryvoltage=constrain(batteryvoltage,3.0, 4.2);
  int16_t batterygauge=int((batteryvoltage-3.0)*8); // usable battery range is 4.2v down to 3.0v for safety, 10 pixel gauge
  if (topmenuindex < NTRACKS) display.drawRect(155, 10-batterygauge,DISPLAY_Y_OFFSET,batterygauge, WHITE); // display for track menus only - will display on next call to display.display
 // Serial.printf("battery %f gauge %d\n", batteryvoltage, batterygauge);

} // end of main loop()

// second core setup
// second core is dedicated to sample processing
void setup1() {
delay (1000); // wait for main core to start up peripherals
}

// look up samples, interpolate and send to DAC
void loop1(){
  int32_t newsample,samplesumL,samplesumR;
  uint32_t index;
  int16_t samp0,samp1,delta,track,tracksample;
  uint32_t command;
  float pitch, retune;
  uint8_t velocity;

// July 2024 changed to interprocessor command FIFO. old scheme of both processors modifying sampleindex is not multicore safe
// this scheme sends note on messages from core1 to core2 via the fifo
// we don't care about note offs - just let the sample play thru
  while (rp2040.fifo.available()) { // get MIDI command, channel# = voice#
    command=rp2040.fifo.pop(); //
    track=(command>>24) & 0xf; 
    command= (command>>24) & 0xf0;  
    velocity=(command >>16) & 0x7f;
    switch (command) {
      case 0x90: // note on
        if (voice[track].slices != 0) { // slice mode playback added 8/15/24
          uint32_t slicesize=(uint32_t)sample[voice[track].sample].samplesize/(uint32_t)(voice[track].slices); // calculate slice size
          uint8_t slicenumber=(uint8_t)(voice[track].note-MIDDLE_C) % (uint8_t)(voice[track].slices); // modulo so we don't index off the end of the sample       
          voice[track].sampleindex=(slicesize*slicenumber)<<12; // calculate start of slice
          voice[track].samplesize=slicesize*(slicenumber+1); // calculate end of slice
          pitch=(float)pitchtable[MIDDLE_C];      
        }
        else { // normal pitched playback of sample
          voice[track].samplesize=sample[voice[track].sample].samplesize; // reset samplesize since we might have just come from slice mode
          pitch=(float)pitchtable[voice[track].note];
          voice[track].sampleindex=0; // start of sample
        }
        retune=(float)voice[track].tune/1000; // tune is integer because of menu system, 1000= 1.000
        retune=powf(2,retune/12); // calculate pitch retuning
        voice[track].sampleincrement=(uint32_t)(pitch*retune); 
        break;
      case 0x80: // note off
      // silence voice by setting sampleindex to last sample
        voice[track].sampleindex=sample[voice[track].sample].samplesize<<12; // sampleindex is a 20:12 fixed point number
        voice[track].samplesize=sample[voice[track].sample].samplesize; //  
        break;
    }       
  }

 // oct 22 2023 resampling code
// to change pitch we step through the sample by .5 rate for half pitch up to 2 for double pitch
// sample.sampleindex is a fixed point 20:12 integer:fraction number
// we step through the sample array by sampleincrement - sampleincrement is also 20:12 fixed point
// 20 bit integer limits the max sample size to 2**20 or about 1 million samples, about 45 seconds @22khz mono
  // oct 24/2023 - scan through voices instead of sample array
  // faster because there are only 16 voices vs typically 45 or more samples
// this is time critical code - keep this loop optimized!

  samplesumL=samplesumR=0;
  for (int i=0; i< NUM_VOICES;++i) {  // look for samples that are playing, scale their volume, and add them up
    tracksample=voice[i].sample; // precompute for a little more speed below
    index=voice[i].sampleindex>>12; // get the integer part of the sample increment
    if (index <= voice[i].samplesize) { // if sample is playing, do linear interpolation   
      samp0=sample[tracksample].samplearray[index]; // get the first sample to interpolate
      samp1=sample[tracksample].samplearray[index+1];// get the second sample
      delta=samp1-samp0;
      newsample=(int32_t)samp0+((int32_t)delta*((int32_t)voice[i].sampleindex & 0x0fff))/4096; // interpolate between the two samples
      samplesumL+=(newsample*voice[i].levelL*voice[i].velocity)/16384; // use MIDI velocity levels 0-127 - have to scale down by 128*128 to avoid overflow
      samplesumR+=(newsample*voice[i].levelR*voice[i].velocity)/16384; // using voice level, not the sample level
      voice[i].sampleindex+=voice[i].sampleincrement; // add step increment
    }
  }

 // samplesum=samplesum>>7;  // adjust for volume multiply above
//  samplesumL=samplesumL>>7;  // adjust for volume multiply above 
//  samplesumR=samplesumR>>7;  // adjust for level*velocity multiply above 
  // adjust the master volume separately - gotta avoid overflow!
  samplesumL=samplesumL*master_volume>>7;  // adjust for master volume 
  samplesumR=samplesumR*master_volume>>7;  // adjust for master volume 
  if  (samplesumL>32767) samplesumL=32767; // clip if sample sum is too large
  if  (samplesumL<-32767) samplesumL=-32767;
  if  (samplesumR>32767) samplesumR=32767; // clip if sample sum is too large
  if  (samplesumR<-32767) samplesumR=-32767;


#ifdef MONITOR_CPU1  
  digitalWrite(CPU_USE,0); // low - CPU not busy
#endif
 // write samples to DMA buffer - this is a blocking call so it stalls when buffer is full
	DAC.write(int16_t(samplesumL)); // left
	DAC.write(int16_t(samplesumR)); // right

#ifdef MONITOR_CPU1
  digitalWrite(CPU_USE,1); // hi = CPU busy
#endif
}


