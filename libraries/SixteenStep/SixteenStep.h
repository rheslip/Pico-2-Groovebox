// ---------------------------------------------------------------------------
//
// SixteenStep.h
// A generic MIDI sequencer library for Arduino.
//
// Author: Todd Treece <todd@uniontownlabs.org>
// Copyright: (c) 2015 Adafruit Industries
// License: GNU GPLv3
// lots of mods Julky aug 2024 R Heslip
// decided to rename it so it doesn't get overwritten by the original library
// ---------------------------------------------------------------------------
#ifndef _SixteenStep_h
#define _SixteenStep_h

#include "Arduino.h"

#define FS_DEFAULT_TEMPO 120
#define FS_DEFAULT_STEPS 16
#define FS_DEFAULT_MEMORY 2048 // need lots of notes for the Pico Groovebox - 16 scenes in each sequencer
//#define FS_DEFAULT_MEMORY 64
#define FS_MIN_TEMPO 10
#define FS_MAX_TEMPO 250
#define FS_MAX_STEPS 128 // step is 8 bits so max 255
//#define FS_MAX_STEPS 16

// MIDIcallback
//
// This defines the MIDI callback function format that is required by the
// sequencer.
//
// Most of the time these arguments will represent the following:
//
// channel: midi channel
// command: note on or off (0x9 or 0x8)
// arg1: pitch value
// arg1: velocity value
//
// It's possible that there will be other types of MIDI messages sent
// to this callback in the future, so please check the command sent if
// you are doing something other than passing on the MIDI messages to
// a MIDI library.
//
typedef void (*MIDIcallback) (byte channel, byte command, byte arg1, byte arg2);

// StepCallback
//
// This defines the format of the step callback function that will be used
// by the sequencer. This callback will be called with the current
// step position and last step position whenever the step changes.
// Please check SixteenStep.cpp for more info about setting the callback
// that will be used.
//
typedef void (*StepCallback) (int current, int last);

// TimeCallback
//
// This defines the format of the time callback function that will be used
// by the sequencer. This callback will be called to get the current time
//
typedef uint32_t (*Timecallback) (void);


// SixteenStepNote
//
// This defines the note type that is used when storing sequence note
// values. The notes will be set to DEFAULT_NOTE until they are modified
// by the user.
typedef struct
{
  byte channel;
  byte pitch;
  byte velocity;
  byte step;
} SixteenStepNote;

// default values for sequence array members
const SixteenStepNote DEFAULT_NOTE = {0x0, 0x0, 0x0, 0x0};

class SixteenStep
{
  public:
    SixteenStep();
    SixteenStep(int memory);
    void  begin();
    void  begin(int tempo);
    void  begin(int tempo, int steps);
    void  begin(int tempo, int steps, int polyphony);
    void  run();
    void  pause();
    void  start();
    void  stop();
    void  panic();
    void  setTempo(int tempo);
    void  setSteps(int steps);
    void  increaseTempo();
    void  decreaseTempo();
    void  increaseShuffle();
    void  decreaseShuffle();
	void  setShuffle(int divisions);
    void  setMidiHandler(MIDIcallback cb);
    void  setStepHandler(StepCallback cb);
	void  setTimeHandler(Timecallback cb);
    void  setNote(byte channel, byte pitch, byte velocity);
	void  setNote( int position, byte channel, byte pitch, byte velocity);
	void  removeNotes(byte channel);
	void  dumpNotes(void);
	SixteenStepNote* getNote(int position, byte channel);
  private:
    MIDIcallback      _midi_cb;
    StepCallback      _step_cb;
	Timecallback      _time_cb;
    SixteenStepNote*  _sequence;
    bool              _running;
	bool			  _mutenotes;
    int               _sequence_size;
    int               _tempo;
    int              _steps;
    int              _position;
    unsigned long     _clock;
    unsigned long     _sixteenth;
    unsigned long     _shuffle;
    unsigned long     _next_beat;
    unsigned long     _next_clock;
    unsigned long     _shuffleDivision();
    int               _quantizedPosition();
    int               _greater(int first, int second);
    void              _init(int memory);
    void              _heapSort();
    void              _siftDown(int root, int bottom);
    void              _resetSequence();
    void              _loopPosition();
    void              _tick();
    void              _step();
    void              _triggerNotes();
};

#endif
