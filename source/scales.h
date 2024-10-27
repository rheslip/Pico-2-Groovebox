// some of this code borrowed from https://github.com/alicedb2/tinyQuan

// Main scales with 1st note at LSB
#define CHROMATIC 0xfff
#define MAJOR 0xab5  // 0b101010110101
#define MINOR 0x5ad  // 0b10110101101
#define HARMONIC_MINOR 0x9ad
#define MAJOR_PENTATONIC 0x295
#define MINOR_PENTATONIC 0x4a9
#define DORIAN 0x6ad
#define PHRYGIAN 0x5ab
#define LYDIAN 0xad5
#define MIXOLYDIAN 0x6b5

uint16_t scales[] ={CHROMATIC,MAJOR,MINOR,HARMONIC_MINOR,MAJOR_PENTATONIC,MINOR_PENTATONIC,DORIAN,PHRYGIAN,LYDIAN,MIXOLYDIAN};
//int16_t current_scale[NTRACKS]={1,1,1,1}; // index of scale in use for each track
int16_t current_scale=1; // index of global scale in use 
int16_t rootnote=60; // MIDI root note 

uint16_t rotate12left(uint16_t n, uint16_t d) {
  return 0xfff & ((n << (d % 12)) | (n >> (12 - (d % 12))));
}

uint16_t rotate12right(uint16_t n, uint16_t d) {
  return 0xfff & ((n >> (d % 12)) | (n << (12 - (d % 12))));
}

// quantize MIDI notes 0-128 to scale with given MIDI root note 0-128
uint8_t quantize(uint8_t note, uint16_t scale,uint8_t root){
  uint8_t n = note%12; // reduce to one octave
  uint8_t key = root%12;
  scale=rotate12left(scale,key); // adjust scale mask into the right key
  for (int i=0;i<3;++i) {  // quantize up, max 2 notes
    if (bitRead(scale,n)) return note;
    scale=rotate12right(scale,1);
    ++note;
  }
  return note; // failed to quantize - should not happen for most scales
}
