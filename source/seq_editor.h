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
  
  static SixteenStepNote *note;

  enc=Encoder1.getValue();

  switch (editstate) {
    case INIT_EDITOR:
      editcursorX=editnote=0;
      draweditcursor(editcursorX,GREEN);
      editstate=SELECTNOTE;
      break;
    case SELECTNOTE:  // scroll l-r to select a note
      if (enc !=0) {
        eraseeditcursor(editcursorX);
        editcursorX+=enc; 
        int16_t s=steps[track]-1;
        editcursorX=constrain(editcursorX,0,s);
        draweditcursor(editcursorX,GREEN);
        note=seq[track].getNote(editcursorX,(scene <<4 | track)); // fetch note from this step so it displays correctly in showposition()
        editnote=note->pitch;
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
    case EDITNOTE:  // rotate encoder to change note
      if (enc !=0) {
        note=seq[track].getNote(editcursorX,(scene <<4 | track)); // *** something very weird happening - note pointer getting trashed? had to recode this with editnote variable
        editnote=note->pitch;
        if (editnote !=0) {
          editnote+=enc;
          if (editnote > HIGHEST_NOTE) editnote=0; // note > range becomes silent
          if (editnote < LOWEST_NOTE) editnote=0; // note < range becomes silent 
        }
        else {
          editnote = MIDDLE_C; // turns note back on
        }
        seq[track].removeNote(editcursorX,(scene <<4 | track)); //  
        if (editnote >0) seq[track].setNote(editcursorX,(scene <<4 | track),editnote,DEFAULT_LEVEL); 
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