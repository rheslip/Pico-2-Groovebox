# Pico 2 Groovebox
 Sample player/groovebox based on RP2350

**Hardware components used**

Pimoroni Pico 2+ with 8mb PSRAM (for sample storage)

Generic PCM5102A DAC module from AliExpress

160x128 ST7735 color TFT display

Encoder with switch

24 pad touch keyboard PCB - gerbers are in the hardware directory. $4 including shipping for 5 PCBs from JLCPCB.com

2x MPR121 touch sensor modules from AliExpress

Micro SD breakout board

**Optional:**

LiPo battery - I used 1500mah for approx 8 hours use

LiPo charger module from AliExpress

Power switch

3V to 5V stepup regulator from AliExpress

There is no schematic but it should be pretty obvious how to wire the modules to the RP2350 board if you look at IO.h in the source code.


**Overview**

Pico 2 Groovebox is a flexible sample player + groovebox with 16 tracks and 16 scenes. Samples can be oneshots or loops. You can play samples with the keypad, play loops, record clips and use drum trigger patterns. Each track has a sequencer which triggers a sample to play. Sequences can be from 1 to 128 steps but normally you would set tracks up as multiples of 16 steps (1 bar). 

It is helpful to think of tracks and scenes as a matrix with columns as tracks and rows as scenes. This setup is similar to Ableton and most grooveboxes. You can record a clip in every cell of this 16x16 matrix ie up to 144 clips, subject to memory limitation which is currently 512 total notes per track. 

The basic workflow is to select a track by holding the TRACK key and select the track using the number pads. Use the Track menus to load a .WAV file sample to a track from the SD card - this saves it in PSRAM for playback (SD is way too slow for direct playback). You can then record a clip (a sequence of sample triggers) by tapping the REC key and touching the the numbered keypads. Holding the REC key will erase the sequence. To change tracks hold the TRACK key and select another track using the number pads.

You can record up to 16 clips per track. Clips are organized by scenes ie rows of clips. Hold the SCENE key to select a scene using the number pads. Selecting a scene will launch all clips on that row of the clip matrix.

Tap the PLAY button to start or stop playback of all clips in the current scene. Hold the PLAY button to start playback in Song mode (described below).

When the sequencers are running you can play samples using the keypad to jam over the current scene, record sequences while the scene is playing, switch scenes and even record in song mode. 

Internally, clips are stored as MIDI sequences. I may consider adding MIDI I/O to the Pico 2 Groovebox so it could be used as a 16 channel MIDI recorder/sequencer.

**Track Screen and Menus**

The top line of the screen shows the currently selected Track:Scene, Song and Record indicators when enabled, and a Bar:Step indicator. On the far right is a simple battery level display.

Below that is a sequencer progress bar which indicates where the sequencer is. This is helpful when recording.

Below the progress bar is a small piano roll display showing the notes recorded on the track. If random velocities are enabled (see below) the color of the steps will range from Magenta (low velocity) to Red (highest velocities).

Below the piano roll is a parameter menu for each track for loading a sample, setting the track length in steps, setting the volume and pan, adjusting the tuning of the sample, adjusting the track shuffle timing, enabling or disabling sample slicing, and to adjust the pattern generator parameters.


**Setup Menu**

Hold the TRACK key and turn the encoder to scroll through tracks 1-16. The first menu past track 16 is the Song chain menu (below). The next menu is Setup which allows selection of BPM, master volume and the musical scale to use on the numbered keys.

**Scales**

There are currently ten musical scales to select from. Selecting a scale changes the layout of the numbered keys. Key 9 plays the sample at its nominal pitch. Playing keys above key 9 will raise the pitch of the sample according to the selected scale. e.g. if the selected scale is chromatic each numbered key above 9 increases the pitch by one semitone. Likewise, keys below 9 reduce the pitch according to the selected scale.

Note that sample pitch and sample tuning are accomplished by a resampling algorithm so higher pitches will play for a shorter time and lower pitches become longer in duration. ie no timestretching going on here!

**Pattern Generator**

The pattern generator is an alternate way of loading trigger patterns into the sequencer. There are currently fifty three 16 bit trigger patterns organized from fewest triggers with pattern 0 having no triggers to pattern 53 which triggers on every sequencer step. Trigger patterns include some common drum patterns and some Euclidean patterns. If the sequencer length is set to a length longer than 16, the pattern will be repeated till the sequencer is full. If the sequencer length is less than 16 the trigger pattern will be shortened to the sequencer length. This is a handy way to chop a pattern off - shorten the sequence, load a trigger pattern and then lengthen the sequence.

The Pattern Shift parameter shifts the trigger pattern right one bit at a time, effectly adding 16 more patterns to each of the basic trigger patterns.

The Pattern Offset parameter generates a series of random pitch offsets in the range selected in the menu. These offsets are added to the base note of the sample, quantized to the current scale and inserted into the sequencer. This results in a random melody in the current scale using the current pattern. 

The Level Offset parameter generates a series of random velocity offsets in the range selected in the menu. These offsets are added to velocity of the sample to give the sequence some random volume variations. The Level Offset parameter is scaled such that it maximum value (16) results in a random velocity range of 0 to 127.

Note that the pattern generator overwrites the sequencer clip with a new generated sequence when any of its parameters are changed. This means you can record more steps over a pattern but you can't add a pattern on top of a recorded clip.

**Clip/Scene Cut and Paste**

To copy a clip, press the Copy key while that clip is on the screen. Select a different clip (using Track and or Scene keys as above) and press the Paste key to copy the sequence to that clip.

To copy an entire scene, hold the Scene key and press the Copy key to copy the current scene. Select a new scene using the Scene key and the numbered keys and press Paste to paste into the new scene.

**Sample Slicer**

The Slices parameter in the track menu controls if slicing is off (slices=0) or the number of uniform sample slices to use up to a maximum of 16. Sample slices are mapped to the numbered keys - if the number of slices is less than 16 the slice pattern repeats over the numbered keys. Slices can be played on the keypad, recorded as a clip or sequenced with the pattern generator. If using the pattern generator, the Sample Offset is used to select which sample is played on active sequencer steps. ie if Sample Offset is 0, the pattern will only include the first slice, if Sample Offset is 1 it will randomly include slice 0 or slice 1 etc.

Sample Slicer + Pattern Generator = a lot of fun! 

**Song Chain**

As described above, the first menu when scrolling past track 16 is the Song Chain. The Song Chain is a list of scenes and the number of repeats for each  scene. By default all scene counts are 0 except for Scene 1 which will always have a minimum repeat count of 1. A song chain it set up by selecting the number of times you would like each scene to repeat. If the count is 0, that scene is skipped. Athe the end of the chain the song will restart at Scene 1. To play the song, hold the Play key until the "SONG" indicator on the top line of the display is on. Tor tuen song mode off, hold the Play key until the "SONG" indicator goes off.

Note: In song mode, a scene is completed/counted down when ALL clips arrive on the last step at the same time. e.g. if one of the tracks is set to a sequence length of 64 and the others are 16 steps, the other tracks will be repeated 4 times and when the 64 step sequence ends the scene counter will be counted down. If we have a track with a sequence length of 16 and another with a length of 15, it will take 16 iterations for the two tracks to arrive at the last step at the same time. If you add another track with 14 steps, its going to take a long time for them to all arrive at the last step at the same time! In general you should keep sequence lengths multiples of 16 or 1 bar.


**FAQ**

How do you compile the source? I used arduino 2.3.2 with Arduino Pico v 4.1. You will need Adafruit graphics library, the ST 7735 driver, and probably some other libs I've forgotten. The rest of the stuff is in the source tree. I started with the Adafruit FifteenStep MIDI recorder library but I had to modify it a lot so I renamed it SixteenStep and its included in the library directory.

Why not have sequencer length set up in bars? I debated this and actually had it set up with bars at one point. I opted for the flexibility of having sequences be any length (up to 128). Its useful for ambient stuff or jamming when you want things to sound similar but not repeat exactly. Set two drum tracks to different lengths for example and the rhythm will change as the beats change relative to each other on every loop.

Why the pattern generator? I had it set up as just a recorder and then quickly realized its hard to get things to record on step 1, which you want if you have multiple scenes so scene changes stay in sync. The pattern generator makes it easy to place triggers where you want them and relative to each other.

Is the code stable? Not completely. It crashes now and then but I've used it for hours on end with no glitches. Still working on those pesky bugs...

Why is loading files so slow? I'm using the SD library included with Arduino Pico which uses SPI. Supposedly you can get 3.5 mbytes/sec read speeds but I've tried everything and the best I can get is about 500kb/sec. If you have suggestions please contact me.

What file formats will it read? 16 and 24 bit, 44hkz, 22khz, stereo or mono .WAV files. It converts everything to 22khz mono internally. I suggest you organize your samples into directories with no more than 50 or so samples per directory to minimize loading time.

Why do you use 22khz mono internally? To minimize memory use. With 8mbytes RAM you can load about 190 seconds of 22khz mono samples vs only 47 seconds with 44khz stereo. Use the pan control for fake stereo - it actually sounds really good even at 22khz.

Why don't you do a PCB? It would probably have to be two boards to work mechanically. I'm not that great at PCB layout - maybe somebody else would take this on if I do a schematic in Kicad?

What are the F1 and F2 keys for? TBD. I'm thinking of adding some more features like track mute and solo
