#define PIANO_ROLL_Y 28  // origin of piano roll on screen
#define EDIT_CURSOR_Y 40

// draw sequencer pattern as a simple piano roll
void showpattern(uint8_t track) {
  float xstep=160.0/(float)(steps[track]); // how wide 1 step is on the screen
  SixteenStepNote *note;
  int16_t y;
  uint16_t velcolor;
  display.fillRect(0,17,160,23,BLACK); // erase old 
  for (int i=0; i<steps[track];++i ) {
    note=seq[track].getNote(i,(scene <<4 | track));
    if (note->pitch !=0) {
      y=(int) note->pitch-MIDDLE_C;  // all pitches are centered on middle C 
 //     velcolor=note->velocity <<9;  // map velocity to color
 //     velcolor=((note->velocity <<9) & 0xf000) | 0x0800;  // map velocity to color red
 //     velcolor|=((note->velocity <<5) & 0x0e00); // add some green
      velcolor=vcolors[map(note->velocity,0,127,0,sizeof(vcolors)/sizeof(uint16_t))]; // map velocity using color table
      y=PIANO_ROLL_Y-y/2; // flip so notes go up with pitch
      display.drawRect((int)(i*xstep), y,(int)xstep,2, velcolor);
    }
    else display.drawRect((int)(i*xstep), y,(int)xstep,2,BLACK);
  }
}

// draw the edit cursor at sequencer position x
void draweditcursor(int16_t x,uint16_t color) {
  float xstep=160.0/(float)(steps[track]); // how wide 1 step is on the screen
  display.drawRect(x*xstep,EDIT_CURSOR_Y,(int)xstep,2,color);
}

// erase the entire cursor line
void eraseeditcursor(int16_t x) {
  display.drawRect(0,EDIT_CURSOR_Y,160,2,BLACK);
}

enum editstates{INIT_EDITOR,SELECTNOTE,EDITNOTE}; // editor state machine states
static int16_t editstate=INIT_EDITOR;
static int16_t editcursorX;
static uint8_t editnote;

// note editor - a run to completion state machine very similar to the menu system
// it does loop while waiting for encoder button release on state changes
void editnotes(void) {

  int16_t enc;
  static uint8_t note;  
  static SixteenStepNote *noteptr;

  enc=Encoder1.getValue();

  switch (editstate) {
    case INIT_EDITOR:
      editcursorX=editnote=0;
      draweditcursor(editcursorX,GREEN);
      noteptr=seq[track].getNote(editcursorX,(scene <<4 | track)); // fetch note from this step so it displays correctly in showposition()
      editnote=note=noteptr->pitch;
      showpattern(track);  // show whats there
      editstate=SELECTNOTE;
      break;
    case SELECTNOTE:  // scroll l-r to select a note
      if (enc !=0) {
        eraseeditcursor(editcursorX);
        editcursorX+=enc; 
        int16_t s=steps[track]-1; // constrain() is a bit flakey with expressions in it
        editcursorX=constrain(editcursorX,0,s);
        draweditcursor(editcursorX,GREEN);
        noteptr=seq[track].getNote(editcursorX,(scene <<4 | track)); // fetch note from this step so it displays correctly in showposition()
        editnote=note=noteptr->pitch;
        showpattern(track);  // show whats there
      }
      if (!digitalRead(ENC_SW)) {
        editstate=EDITNOTE;
        draweditcursor(editcursorX,RED);
        while(!digitalRead(ENC_SW)) { // wait for encoder button release
          if (Encoder1.getButton() == ClickEncoder::Held) {
            edit_mode=false; // long press to exit editing
            eraseeditcursor(editcursorX);
            break;
          }
        }; 
      }
    break;
    case EDITNOTE:  // rotate encoder to change note - changed this so if you scroll off the bottom or the top there is more than one click (NOTE_DEADZONE) that removes the note
      if (enc !=0) {
        noteptr=seq[track].getNote(editcursorX,(scene <<4 | track)); // *** something very weird happening - note pointer getting trashed? had to recode this with editnote variable
        if (note !=0) {
          note+=enc;
          if (note > (HIGHEST_NOTE+NOTE_DEADZONE)) note=LOWEST_NOTE; // 
          if (note < (LOWEST_NOTE-NOTE_DEADZONE)) note=HIGHEST_NOTE; //  notes loop around
        }
        else {
          note = MIDDLE_C; // turns note back on
        }
        seq[track].removeNote(editcursorX,(scene <<4 | track)); // remove any note at this position
         //  if note is in valid range, save it
        if ((note >=LOWEST_NOTE) && (note <= HIGHEST_NOTE)) {          
          seq[track].setNote(editcursorX,(scene <<4 | track),note,DEFAULT_LEVEL); 
          editnote=note; // so it shows in showpattern()
        }
        else editnote=0; // out of range so turn it off for showpattern()
        showpattern(track);
      }
      if (!digitalRead(ENC_SW)) {
        editstate=SELECTNOTE;
        draweditcursor(editcursorX,GREEN);
        while(!digitalRead(ENC_SW));  // wait for encoder button release
        /*
        while(!digitalRead(ENC_SW)) {  // wait for encoder button release
          if (Encoder1.getButton() == ClickEncoder::Held) {
            edit_mode=false; // long press to exit editing
            eraseeditcursor(editcursorX);
           // break; // have to loop here or its a pressed button in menus
          }
        }; */
      }  
    break; 
  }  // end of switch
  return;
}