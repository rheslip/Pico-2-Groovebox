


// Copyright 2020 Rich Heslip
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
// this is adapted from my XVA1 menu system. this one is bitmapped so its a mix of character and pixel addressing
// Feb 2022 - adapted again as a single encoder menu system - very similar to the Arduino Neu-rah menu system but only 2 levels
// menu items are displayed top to bottom of screen with a title bar
// encoder scrolls menu selector, click to select submenu
// encoder scrolls submenu selector, click to edit parameter
// last submenu item is treated as "back to top menu" so make sure its set up that way
// parameters can be:
// text strings which return ordinals
// integers in range -9999 to +9999, range and increment set in the submenu table
// floats in range -9.99 to +9.99 - floats are displayed but the parameter behind them is an int in the range -999 to +999 so your code has to convert the int to float
// the parameter field in the submenu initializer must point to an integer variable - when you edit the on screen value its this value you are changing
// the handler field in the submenu initializer must be either null or point to a function which is called when you edit the parameter

// these defs for 160x128 display with 6x8 font
#define SCREENWIDTH 160
#define SCREENHEIGHT 128
#define DISPLAY_X 26  // 26 char display
#define DISPLAY_Y 16   // 16 lines max
#define DISPLAY_CHAR_HEIGHT 8 // character height in pixels - for bitmap displays
#define DISPLAY_CHAR_WIDTH 6 // character width in pixels - for bitmap displays
#define DISPLAY_X_MENUPAD 2   // extra space between menu items in pixels
#define DISPLAY_Y_MENUPAD 2   // extra vertical space between menu items in pixels
#define DISPLAY_Y_OFFSET 4   // offset from top of screen in pixels - TFT is a bit wonky
#define TOPMENU_LINE 4    // line to start menus on
#define TOPMENU_Y (TOPMENU_LINE*(DISPLAY_CHAR_HEIGHT+DISPLAY_Y_MENUPAD)+DISPLAY_Y_OFFSET)   // pixel y position to display top menus
#define TOPMENU_X (1 * DISPLAY_CHAR_WIDTH)   // x pos to display top menus - first character reserved for selector character
#define TOPMENU_LINES 8 // number of menu text lines to display
#define SUBMENU_LINE 4 // line to start sub menus on
#define SUBMENU_Y (SUBMENU_LINE*(DISPLAY_CHAR_HEIGHT+DISPLAY_Y_MENUPAD)+DISPLAY_Y_OFFSET)   // line to display sub menus
#define SUBMENU_X (1 * DISPLAY_CHAR_WIDTH)   // x pos to display sub menus name field
#define SUBMENU_VALUE_X (18 * DISPLAY_CHAR_WIDTH)  // x pos to display submenu values
#define SUBMENU_LINES 8 // number of menu text lines to display
#define FILEMENU_LINES 8 // number of files to show 
#define FILEMENU_X (1 * DISPLAY_CHAR_WIDTH)   // x pos to display file menus - first character reserved for selector character
#define FILEMENU_Y (TOPMENU_LINE*(DISPLAY_CHAR_HEIGHT+DISPLAY_Y_MENUPAD)+DISPLAY_Y_OFFSET)   // pixel y position to display file menus

const char *menutitle;  // points to title of current menu/submenu
const char *maintitle="   Pico Groovebox   ";

const char *filesroot="/Samples";  // root of file tree

char filepath[200];  // working file path
#define ROOTLEN  8  // length of root filename
#define MAXFILES 500 // max number of files per directory we can read

int8_t topmenuindex=0;  // keeps track of which top menu item we are displaying
//int8_t fileindex=0;  // keeps track of which file we are displaying
int numfiles=0; // number of files in current directory

enum uimodes{TOPSELECT,SUBSELECT,PARAM_INPUT,FILEBROWSER,WAITFORBUTTONUP}; // UI state machine states
static int16_t uistate=TOPSELECT; // start out at top menu

enum paramtype{TYPE_NONE,TYPE_INTEGER,TYPE_FLOAT, TYPE_TEXT,TYPE_FILENAME}; // parameter display types

// holds file and directory info
struct fileinfo {
	char name[80];
	bool isdir;   // true if directory
  uint32_t size; // not using at the moment 
} files[MAXFILES];

// for sorting filenames in alpha order
int comp(const void *a,const void *b) {
return (strcmp((char *)a,(char *)b));
}
 
	
// submenus 
struct submenu {
  const char *name; // display name
  int16_t min;  // min value of parameter
  int16_t max;  // max value of parameter
  int16_t step; // step size. if 0, don't print ie spacer
  enum paramtype ptype; // how its displayed
  const char ** ptext;   // points to array of text for text display
  int16_t *parameter; // value to modify
  void (*handler)(void);  // function to call on value change
};

// top menus
struct menu {
   const char *name; // menu text
   struct submenu * submenus; // points to submenus for this menu
   int8_t submenuindex;   // stores the index of the submenu we are currently using
   int8_t numsubmenus; // number of submenus - not sure why this has to be int but it crashes otherwise. compiler bug?
};

// dummy variable for menu testing
int16_t dummy;

/*
// reset pitch that was modulated by a CV to a reasonable default value
// saves having to manually do it when you "unmodulate" a parameter
void resetCVpitch(void) {
	if (samp[topmenuindex].pitchCV==0) samp[topmenuindex].pitch=1.0;
}; // 
*/

// show sample name of voice 
// relies on topmenuindex having the same index as the current voice

void printsamplename(void) {
  int y= SUBMENU_Y+DISPLAY_Y_MENUPAD;  
	display.setCursor (SUBMENU_X, y ); // leave room for selector
  display.print("                    "); // erase old
  display.setCursor (SUBMENU_X, y ); // leave room for selector
  display.printf("%-.20s",sample[voice[topmenuindex].sample].sname);
  /*
	char temp[DISPLAY_X];  // chop the name to no more than 20 chars
		      //strncpy(temp,samp[sub[index].min].filename,DISPLAY_X-1); // hokey way of finding the sample's filename
  strncpy(temp,sample[voice[topmenuindex].sample].sname,DISPLAY_X-1);
  strcat(temp,"");
	display.print(temp);  //
  */ 	
}; 


void testfunc(void) {
  printf("test function %d\n",dummy);
}; // 

 

// ********** menu structs that build the menu system below *********


// text arrays used for submenu TYPE_TEXT fields
const char * scalenames[] = {"Chro","Maj", "Min","Hmin","MPen","mPen","Dor","Phry","Lyd","Mixo"};
const char * shiftdirection[] = {"<"," ",">"};
const char * onoff[] = {" Off","  On"};

struct submenu sample0params[] = {
  // name,min,max,step,type,*textfield,*parameter,*handler
  "",0,0,1,TYPE_FILENAME,0,&voice[0].sample,printsamplename,          // hokey - value of min is the sample number
  "Steps",1,MAX_STEPS,1,TYPE_INTEGER,0,&steps[0],setsteps,
  "Level",0,1000,10,TYPE_FLOAT,0,&tracklevel[0],setlevels,
  "Pan",-1000,1000,50,TYPE_FLOAT,0,&trackpan[0],setlevels,   
  "Tune",-12000,12000,50,TYPE_FLOAT,0,&voice[0].tune,0, 
  "Shuffle",0,15,1,TYPE_INTEGER,0,&shuffle[0],setshuffle,
//  "Shift",-1,1,1,TYPE_INTEGER,0,&shift,shiftclip,
  "Slices",0,16,1,TYPE_INTEGER,0,&voice[0].slices,0,
  "Pattern",0,NUMPATTERNS-1,1,TYPE_INTEGER,0,&pattern[0],setpattern,
  "Pat Shift",0,15,1,TYPE_INTEGER,0,&patshift[0],setpattern,   
  "Pat Offsets",0,15,1,TYPE_INTEGER,0,&patpitch[0],pitchrandomizer,  
  "Pat Accents",0,16,1,TYPE_INTEGER,0,&patvelocity[0],velocityrandomizer,  
  "AutoRandomize",0,1,1,TYPE_TEXT,onoff,&rerandomize[0],0,               
};

struct submenu sample1params[] = {
  // name,min,max,step,type,*textfield,*parameter,*handler
  "",1,0,1,TYPE_FILENAME,0,&voice[0].sample,printsamplename,          // hokey - value of min is the sample number
  "Steps",1,MAX_STEPS,1,TYPE_INTEGER,0,&steps[1],setsteps,
  "Level",0,1000,10,TYPE_FLOAT,0,&tracklevel[1],setlevels,
  "Pan",-1000,1000,50,TYPE_FLOAT,0,&trackpan[1],setlevels,   
  "Tune",-12000,12000,50,TYPE_FLOAT,0,&voice[1].tune,0, 
  "Shuffle",0,15,1,TYPE_INTEGER,0,&shuffle[1],setshuffle,
  "Slices",0,16,1,TYPE_INTEGER,0,&voice[1].slices,0, 
  "Pattern",0,NUMPATTERNS-1,1,TYPE_INTEGER,0,&pattern[1],setpattern,
  "Pat Shift",0,15,1,TYPE_INTEGER,0,&patshift[1],setpattern, 
  "Note Offsets",0,15,1,TYPE_INTEGER,0,&patpitch[1],pitchrandomizer,  
  "Level Offsets",0,16,1,TYPE_INTEGER,0,&patvelocity[1],velocityrandomizer,  
  "AutoRandomize",0,1,1,TYPE_TEXT,onoff,&rerandomize[1],0,                
};

struct submenu sample2params[] = {
  // name,min,max,step,type,*textfield,*parameter,*handler
  "",2,0,1,TYPE_FILENAME,0,&voice[0].sample,printsamplename,          // hokey - value of min is the sample number
  "Steps",1,MAX_STEPS,1,TYPE_INTEGER,0,&steps[2],setsteps,
  "Level",0,1000,10,TYPE_FLOAT,0,&tracklevel[2],setlevels,
  "Pan",-1000,1000,50,TYPE_FLOAT,0,&trackpan[2],setlevels,   
  "Tune",-12000,12000,50,TYPE_FLOAT,0,&voice[2].tune,0,
  "Shuffle",0,15,1,TYPE_INTEGER,0,&shuffle[2],setshuffle,
  "Slices",0,16,1,TYPE_INTEGER,0,&voice[2].slices,0, 
  "Pattern",0,NUMPATTERNS-1,1,TYPE_INTEGER,0,&pattern[2],setpattern,
  "Pat Shift",0,15,1,TYPE_INTEGER,0,&patshift[2],setpattern,          
  "Note Offsets",0,15,1,TYPE_INTEGER,0,&patpitch[2],pitchrandomizer,  
  "Level Offsets",0,16,1,TYPE_INTEGER,0,&patvelocity[2],velocityrandomizer, 
  "AutoRandomize",0,1,1,TYPE_TEXT,onoff,&rerandomize[2],0,       
};

struct submenu sample3params[] = {
  // name,min,max,step,type,*textfield,*parameter,*handler
  "",3,0,1,TYPE_FILENAME,0,&voice[0].sample,printsamplename,          // hokey - value of min is the sample number
  "Steps",1,MAX_STEPS,1,TYPE_INTEGER,0,&steps[3],setsteps,
  "Level",0,1000,10,TYPE_FLOAT,0,&tracklevel[3],setlevels,
  "Pan",-1000,1000,50,TYPE_FLOAT,0,&trackpan[3],setlevels,   
  "Tune",-12000,12000,50,TYPE_FLOAT,0,&voice[3].tune,0,
  "Shuffle",0,15,1,TYPE_INTEGER,0,&shuffle[3],setshuffle, 
  "Slices",0,16,1,TYPE_INTEGER,0,&voice[3].slices,0,
  "Pattern",0,NUMPATTERNS-1,1,TYPE_INTEGER,0,&pattern[3],setpattern,
  "Pat Shift",0,15,1,TYPE_INTEGER,0,&patshift[3],setpattern,        
  "Note Offsets",0,15,1,TYPE_INTEGER,0,&patpitch[3],pitchrandomizer,  
  "Level Offsets",0,16,1,TYPE_INTEGER,0,&patvelocity[3],velocityrandomizer, 
  "AutoRandomize",0,1,1,TYPE_TEXT,onoff,&rerandomize[3],0,       
};

struct submenu sample4params[] = {
  // name,min,max,step,type,*textfield,*parameter,*handler
  "",4,0,1,TYPE_FILENAME,0,&voice[0].sample,printsamplename,          // hokey - value of min is the sample number
  "Steps",1,MAX_STEPS,1,TYPE_INTEGER,0,&steps[4],setsteps,
  "Level",0,1000,10,TYPE_FLOAT,0,&tracklevel[4],setlevels,
  "Pan",-1000,1000,50,TYPE_FLOAT,0,&trackpan[4],setlevels,   
  "Tune",-12000,12000,50,TYPE_FLOAT,0,&voice[4].tune,0,
  "Shuffle",0,15,1,TYPE_INTEGER,0,&shuffle[4],setshuffle, 
  "Slices",0,16,1,TYPE_INTEGER,0,&voice[4].slices,0,
  "Pattern",0,NUMPATTERNS-1,1,TYPE_INTEGER,0,&pattern[4],setpattern,
  "Pat Shift",0,15,1,TYPE_INTEGER,0,&patshift[4],setpattern,      
  "Note Offsets",0,15,1,TYPE_INTEGER,0,&patpitch[4],pitchrandomizer,  
  "Level Offsets",0,16,1,TYPE_INTEGER,0,&patvelocity[4],velocityrandomizer,
  "AutoRandomize",0,1,1,TYPE_TEXT,onoff,&rerandomize[4],0,       
};

struct submenu sample5params[] = {
  // name,min,max,step,type,*textfield,*parameter,*handler
  "",5,0,1,TYPE_FILENAME,0,&voice[0].sample,printsamplename,          // hokey - value of min is the sample number
  "Steps",1,MAX_STEPS,1,TYPE_INTEGER,0,&steps[5],setsteps,
  "Level",0,1000,10,TYPE_FLOAT,0,&tracklevel[5],setlevels,
  "Pan",-1000,1000,50,TYPE_FLOAT,0,&trackpan[5],setlevels,   
  "Tune",-12000,12000,50,TYPE_FLOAT,0,&voice[5].tune,0,
  "Shuffle",0,15,1,TYPE_INTEGER,0,&shuffle[5],setshuffle,
  "Slices",0,16,1,TYPE_INTEGER,0,&voice[5].slices,0,
  "Pattern",0,NUMPATTERNS-1,1,TYPE_INTEGER,0,&pattern[5],setpattern,
  "Pat Shift",0,15,1,TYPE_INTEGER,0,&patshift[5],setpattern,     
  "Note Offsets",0,15,1,TYPE_INTEGER,0,&patpitch[5],pitchrandomizer,  
  "Level Offsets",0,16,1,TYPE_INTEGER,0,&patvelocity[5],velocityrandomizer,
  "AutoRandomize",0,1,1,TYPE_TEXT,onoff,&rerandomize[5],0,      
};

struct submenu sample6params[] = {
  // name,min,max,step,type,*textfield,*parameter,*handler
  "",6,0,1,TYPE_FILENAME,0,&voice[0].sample,printsamplename,          // hokey - value of min is the sample number
  "Steps",1,MAX_STEPS,1,TYPE_INTEGER,0,&steps[6],setsteps,
  "Level",0,1000,10,TYPE_FLOAT,0,&tracklevel[6],setlevels,
  "Pan",-1000,1000,50,TYPE_FLOAT,0,&trackpan[6],setlevels,   
  "Tune",-12000,12000,50,TYPE_FLOAT,0,&voice[6].tune,0,
  "Shuffle",0,15,1,TYPE_INTEGER,0,&shuffle[6],setshuffle,
  "Slices",0,16,1,TYPE_INTEGER,0,&voice[6].slices,0,
  "Pattern",0,NUMPATTERNS-1,1,TYPE_INTEGER,0,&pattern[6],setpattern,
  "Pat Shift",0,15,1,TYPE_INTEGER,0,&patshift[6],setpattern,     
  "Note Offsets",0,15,1,TYPE_INTEGER,0,&patpitch[6],pitchrandomizer,  
  "Level Offsets",0,16,1,TYPE_INTEGER,0,&patvelocity[6],velocityrandomizer,
  "AutoRandomize",0,1,1,TYPE_TEXT,onoff,&rerandomize[6],0,       
};

struct submenu sample7params[] = {
  // name,min,max,step,type,*textfield,*parameter,*handler
  "",7,0,1,TYPE_FILENAME,0,&voice[0].sample,printsamplename,          // hokey - value of min is the sample number
  "Steps",1,MAX_STEPS,1,TYPE_INTEGER,0,&steps[7],setsteps,
  "Level",0,1000,10,TYPE_FLOAT,0,&tracklevel[7],setlevels,
  "Pan",-1000,1000,50,TYPE_FLOAT,0,&trackpan[7],setlevels,   
  "Tune",-12000,12000,50,TYPE_FLOAT,0,&voice[7].tune,0,
  "Shuffle",0,15,1,TYPE_INTEGER,0,&shuffle[7],setshuffle,
  "Slices",0,16,1,TYPE_INTEGER,0,&voice[7].slices,0,
  "Pattern",0,NUMPATTERNS-1,1,TYPE_INTEGER,0,&pattern[7],setpattern,
  "Pat Shift",0,15,1,TYPE_INTEGER,0,&patshift[7],setpattern,      
  "Note Offsets",0,15,1,TYPE_INTEGER,0,&patpitch[7],pitchrandomizer,  
  "Level Offsets",0,16,1,TYPE_INTEGER,0,&patvelocity[7],velocityrandomizer,
  "AutoRandomize",0,1,1,TYPE_TEXT,onoff,&rerandomize[7],0,       
};

struct submenu sample8params[] = {
  // name,min,max,step,type,*textfield,*parameter,*handler
  "",8,0,1,TYPE_FILENAME,0,&voice[0].sample,printsamplename,          // hokey - value of min is the sample number
  "Steps",1,MAX_STEPS,1,TYPE_INTEGER,0,&steps[8],setsteps,
  "Level",0,1000,10,TYPE_FLOAT,0,&tracklevel[8],setlevels,
  "Pan",-1000,1000,50,TYPE_FLOAT,0,&trackpan[8],setlevels,   
  "Tune",-12000,12000,50,TYPE_FLOAT,0,&voice[8].tune,0,
  "Shuffle",0,15,1,TYPE_INTEGER,0,&shuffle[8],setshuffle,
  "Slices",0,16,1,TYPE_INTEGER,0,&voice[8].slices,0,
  "Pattern",0,NUMPATTERNS-1,1,TYPE_INTEGER,0,&pattern[8],setpattern,
  "Pat Shift",0,15,1,TYPE_INTEGER,0,&patshift[8],setpattern,              
  "Note Offsets",0,15,1,TYPE_INTEGER,0,&patpitch[8],pitchrandomizer,  
  "Level Offsets",0,16,1,TYPE_INTEGER,0,&patvelocity[8],velocityrandomizer,
  "AutoRandomize",0,1,1,TYPE_TEXT,onoff,&rerandomize[8],0,       
};

struct submenu sample9params[] = {
  // name,min,max,step,type,*textfield,*parameter,*handler
  "",9,0,1,TYPE_FILENAME,0,&voice[0].sample,printsamplename,          // hokey - value of min is the sample number
  "Steps",1,MAX_STEPS,1,TYPE_INTEGER,0,&steps[9],setsteps,
  "Level",0,1000,10,TYPE_FLOAT,0,&tracklevel[9],setlevels,
  "Pan",-1000,1000,50,TYPE_FLOAT,0,&trackpan[9],setlevels,   
  "Tune",-12000,12000,50,TYPE_FLOAT,0,&voice[9].tune,0,
  "Shuffle",0,15,1,TYPE_INTEGER,0,&shuffle[9],setshuffle,
  "Slices",0,16,1,TYPE_INTEGER,0,&voice[9].slices,0,
  "Pattern",0,NUMPATTERNS-1,1,TYPE_INTEGER,0,&pattern[9],setpattern,
  "Pat Shift",0,15,1,TYPE_INTEGER,0,&patshift[9],setpattern,    
  "Note Offsets",0,15,1,TYPE_INTEGER,0,&patpitch[9],pitchrandomizer,  
  "Level Offsets",0,16,1,TYPE_INTEGER,0,&patvelocity[9],velocityrandomizer,
  "AutoRandomize",0,1,1,TYPE_TEXT,onoff,&rerandomize[9],0,       
};

struct submenu sample10params[] = {
  // name,min,max,step,type,*textfield,*parameter,*handler
  "",10,0,1,TYPE_FILENAME,0,&voice[0].sample,printsamplename,          // hokey - value of min is the sample number
  "Steps",1,MAX_STEPS,1,TYPE_INTEGER,0,&steps[10],setsteps,
  "Level",0,1000,10,TYPE_FLOAT,0,&tracklevel[10],setlevels,
  "Pan",-1000,1000,50,TYPE_FLOAT,0,&trackpan[10],setlevels,   
  "Tune",-12000,12000,50,TYPE_FLOAT,0,&voice[10].tune,0,
  "Shuffle",0,15,1,TYPE_INTEGER,0,&shuffle[10],setshuffle,
  "Slices",0,16,1,TYPE_INTEGER,0,&voice[10].slices,0,
  "Pattern",0,NUMPATTERNS-1,1,TYPE_INTEGER,0,&pattern[10],setpattern,
  "Pat Shift",0,15,1,TYPE_INTEGER,0,&patshift[10],setpattern,        
  "Note Offsets",0,15,1,TYPE_INTEGER,0,&patpitch[10],pitchrandomizer,  
  "Level Offsets",0,16,1,TYPE_INTEGER,0,&patvelocity[10],velocityrandomizer, 
  "AutoRandomize",0,1,1,TYPE_TEXT,onoff,&rerandomize[10],0,      
};

struct submenu sample11params[] = {
  // name,min,max,step,type,*textfield,*parameter,*handler
  "",11,0,1,TYPE_FILENAME,0,&voice[0].sample,printsamplename,          // hokey - value of min is the sample number
  "Steps",1,MAX_STEPS,1,TYPE_INTEGER,0,&steps[11],setsteps,
  "Level",0,1000,10,TYPE_FLOAT,0,&tracklevel[11],setlevels,
  "Pan",-1000,1000,50,TYPE_FLOAT,0,&trackpan[11],setlevels,   
  "Tune",-12000,12000,50,TYPE_FLOAT,0,&voice[11].tune,0,
  "Shuffle",0,15,1,TYPE_INTEGER,0,&shuffle[11],setshuffle,
  "Slices",0,16,1,TYPE_INTEGER,0,&voice[11].slices,0,
  "Pattern",0,NUMPATTERNS-1,1,TYPE_INTEGER,0,&pattern[11],setpattern,
  "Pat Shift",0,15,1,TYPE_INTEGER,0,&patshift[11],setpattern,    
  "Note Offsets",0,15,1,TYPE_INTEGER,0,&patpitch[11],pitchrandomizer,  
  "Level Offsets",0,16,1,TYPE_INTEGER,0,&patvelocity[11],velocityrandomizer,
  "AutoRandomize",0,1,1,TYPE_TEXT,onoff,&rerandomize[11],0,       
};

struct submenu sample12params[] = {
  // name,min,max,step,type,*textfield,*parameter,*handler
  "",12,0,1,TYPE_FILENAME,0,&voice[0].sample,printsamplename,          // hokey - value of min is the sample number
  "Steps",1,MAX_STEPS,1,TYPE_INTEGER,0,&steps[12],setsteps,
  "Level",0,1000,10,TYPE_FLOAT,0,&tracklevel[12],setlevels,
  "Pan",-1000,1000,50,TYPE_FLOAT,0,&trackpan[12],setlevels, 
  "Tune",-12000,12000,50,TYPE_FLOAT,0,&voice[12].tune,0,  
  "Shuffle",0,15,1,TYPE_INTEGER,0,&shuffle[12],setshuffle,
  "Slices",0,16,1,TYPE_INTEGER,0,&voice[12].slices,0,
  "Pattern",0,NUMPATTERNS-1,1,TYPE_INTEGER,0,&pattern[12],setpattern,
  "Pat Shift",0,15,1,TYPE_INTEGER,0,&patshift[12],setpattern, 
  "Note Offsets",0,15,1,TYPE_INTEGER,0,&patpitch[12],pitchrandomizer,  
  "Level Offsets",0,16,1,TYPE_INTEGER,0,&patvelocity[12],velocityrandomizer, 
  "AutoRandomize",0,1,1,TYPE_TEXT,onoff,&rerandomize[12],0,      
};

struct submenu sample13params[] = {
  // name,min,max,step,type,*textfield,*parameter,*handler
  "",13,0,1,TYPE_FILENAME,0,&voice[0].sample,printsamplename,          // hokey - value of min is the sample number
  "Steps",1,MAX_STEPS,1,TYPE_INTEGER,0,&steps[13],setsteps,
  "Level",0,1000,10,TYPE_FLOAT,0,&tracklevel[13],setlevels,
  "Pan",-1000,1000,50,TYPE_FLOAT,0,&trackpan[13],setlevels, 
  "Tune",-12000,12000,50,TYPE_FLOAT,0,&voice[13].tune,0,  
  "Shuffle",0,15,1,TYPE_INTEGER,0,&shuffle[13],setshuffle,
  "Slices",0,16,1,TYPE_INTEGER,0,&voice[13].slices,0,
  "Pattern",0,NUMPATTERNS-1,1,TYPE_INTEGER,0,&pattern[13],setpattern,
  "Pat Shift",0,15,1,TYPE_INTEGER,0,&patshift[13],setpattern, 
  "Note Offsets",0,15,1,TYPE_INTEGER,0,&patpitch[13],pitchrandomizer,  
  "Level Offsets",0,16,1,TYPE_INTEGER,0,&patvelocity[13],velocityrandomizer,
  "AutoRandomize",0,1,1,TYPE_TEXT,onoff,&rerandomize[13],0,       
};

struct submenu sample14params[] = {
  // name,min,max,step,type,*textfield,*parameter,*handler
  "",14,0,1,TYPE_FILENAME,0,&voice[0].sample,printsamplename,          // hokey - value of min is the sample number
  "Steps",1,MAX_STEPS,1,TYPE_INTEGER,0,&steps[14],setsteps,
  "Level",0,1000,10,TYPE_FLOAT,0,&tracklevel[14],setlevels,
  "Pan",-1000,1000,50,TYPE_FLOAT,0,&trackpan[14],setlevels, 
  "Tune",-12000,12000,50,TYPE_FLOAT,0,&voice[14].tune,0,
  "Shuffle",0,15,1,TYPE_INTEGER,0,&shuffle[14],setshuffle,
  "Slices",0,16,1,TYPE_INTEGER,0,&voice[14].slices,0,
  "Pattern",0,NUMPATTERNS-1,1,TYPE_INTEGER,0,&pattern[14],setpattern,
  "Pat Shift",0,15,1,TYPE_INTEGER,0,&patshift[14],setpattern, 
  "Note Offsets",0,15,1,TYPE_INTEGER,0,&patpitch[14],pitchrandomizer,  
  "Level Offsets",0,16,1,TYPE_INTEGER,0,&patvelocity[14],velocityrandomizer, 
  "AutoRandomize",0,1,1,TYPE_TEXT,onoff,&rerandomize[14],0,      
};

struct submenu sample15params[] = {
  // name,min,max,step,type,*textfield,*parameter,*handler
  "",15,0,1,TYPE_FILENAME,0,&voice[0].sample,printsamplename,          // hokey - value of min is the sample number
  "Steps",1,MAX_STEPS,1,TYPE_INTEGER,0,&steps[15],setsteps,
  "Level",0,1000,10,TYPE_FLOAT,0,&tracklevel[15],setlevels,
  "Pan",-1000,1000,50,TYPE_FLOAT,0,&trackpan[15],setlevels, 
  "Tune",-12000,12000,50,TYPE_FLOAT,0,&voice[15].tune,0,  
  "Shuffle",0,15,1,TYPE_INTEGER,0,&shuffle[15],setshuffle,
  "Slices",0,16,1,TYPE_INTEGER,0,&voice[15].slices,0,
  "Pattern",0,NUMPATTERNS-1,1,TYPE_INTEGER,0,&pattern[15],setpattern,
  "Pat Shift",0,15,1,TYPE_INTEGER,0,&patshift[15],setpattern, 
  "Note Offsets",0,15,1,TYPE_INTEGER,0,&patpitch[15],pitchrandomizer,  
  "Level Offsets",0,16,1,TYPE_INTEGER,0,&patvelocity[15],velocityrandomizer,\
  "AutoRandomize",0,1,1,TYPE_TEXT,onoff,&rerandomize[15],0,       
};

struct submenu scenechain[] = {
  // name,min,max,step,type,*textfield,*parameter,*handler
  "SCENE 1",1,16,1,TYPE_INTEGER,0,&scenecount[0],0,
  "SCENE 2",0,16,1,TYPE_INTEGER,0,&scenecount[1],0,
  "SCENE 3",0,16,1,TYPE_INTEGER,0,&scenecount[2],0,
  "SCENE 4",0,16,1,TYPE_INTEGER,0,&scenecount[3],0,
  "SCENE 5",0,16,1,TYPE_INTEGER,0,&scenecount[4],0,
  "SCENE 6",0,16,1,TYPE_INTEGER,0,&scenecount[5],0,
  "SCENE 7",0,16,1,TYPE_INTEGER,0,&scenecount[6],0,
  "SCENE 8",0,16,1,TYPE_INTEGER,0,&scenecount[7],0,
  "SCENE 9",0,16,1,TYPE_INTEGER,0,&scenecount[8],0,
  "SCENE 10",0,16,1,TYPE_INTEGER,0,&scenecount[9],0,
  "SCENE 11",0,16,1,TYPE_INTEGER,0,&scenecount[10],0,
  "SCENE 12",0,16,1,TYPE_INTEGER,0,&scenecount[11],0,
  "SCENE 13",0,16,1,TYPE_INTEGER,0,&scenecount[12],0,
  "SCENE 14",0,16,1,TYPE_INTEGER,0,&scenecount[13],0,
  "SCENE 15",0,16,1,TYPE_INTEGER,0,&scenecount[14],0,
  "SCENE 16",0,16,1,TYPE_INTEGER,0,&scenecount[15],0,
};

struct submenu setupparams[] = {
  // name,min,max,step,type,*textfield,*parameter,*handler
  "BPM",40,200,1,TYPE_INTEGER,0,&bpm,settempo,
  "Volume",20,127,1,TYPE_INTEGER,0,&master_volume,0,
//  "Steps/Bar",1,MAX_STEPS,1,TYPE_INTEGER,0,&stepsperbar,0,
  "Scale",0,9,1,TYPE_TEXT,scalenames,&current_scale,0,
};


// top level menu structure - each top level menu contains one submenu
struct menu mainmenu[] = {
  // name,submenu *,initial submenu index,number of submenus
  "",sample0params,0,sizeof(sample0params)/sizeof(submenu),
  "",sample1params,0,sizeof(sample1params)/sizeof(submenu),
  "",sample2params,0,sizeof(sample2params)/sizeof(submenu),
  "",sample3params,0,sizeof(sample3params)/sizeof(submenu),
  "",sample4params,0,sizeof(sample4params)/sizeof(submenu),
  "",sample5params,0,sizeof(sample5params)/sizeof(submenu),
  "",sample6params,0,sizeof(sample6params)/sizeof(submenu),
  "",sample7params,0,sizeof(sample7params)/sizeof(submenu), 
  "",sample8params,0,sizeof(sample8params)/sizeof(submenu),
  "",sample9params,0,sizeof(sample9params)/sizeof(submenu),
  "",sample10params,0,sizeof(sample10params)/sizeof(submenu),
  "",sample11params,0,sizeof(sample11params)/sizeof(submenu),
  "",sample12params,0,sizeof(sample12params)/sizeof(submenu),
  "",sample13params,0,sizeof(sample13params)/sizeof(submenu),
  "",sample14params,0,sizeof(sample14params)/sizeof(submenu),
  "",sample15params,0,sizeof(sample15params)/sizeof(submenu), 
  "Song Chain    Repeats",scenechain,0,sizeof(scenechain)/sizeof(submenu),
  "Setup ",setupparams,0,sizeof(setupparams)/sizeof(submenu),   
 };

#define NUM_MAIN_MENUS sizeof(mainmenu)/ sizeof(menu)
menu * topmenu=mainmenu;  // points at current menu

// highlight the currently selected menu item
void drawselector( int8_t index) {
  int line = index % TOPMENU_LINES;
  display.setCursor (0, TOPMENU_Y+DISPLAY_Y_MENUPAD+line*(DISPLAY_CHAR_HEIGHT+DISPLAY_Y_MENUPAD));
  display.print(">"); 
//  display.display();
}

// highlight the currently selected menu item as being edited
void draweditselector( int8_t index) {
  int line = index % TOPMENU_LINES;
  display.setCursor (0, TOPMENU_Y+DISPLAY_Y_MENUPAD+line*(DISPLAY_CHAR_HEIGHT+DISPLAY_Y_MENUPAD) );
  display.setTextColor(RED,BLACK);
  display.print("*"); 
  display.setTextColor(WHITE,BLACK);
//  display.display();
}

// dehighlight the currently selected menu item
void undrawselector( int8_t index) {
  int line = index % TOPMENU_LINES;
  display.setCursor (0, TOPMENU_Y+DISPLAY_Y_MENUPAD+line*(DISPLAY_CHAR_HEIGHT+DISPLAY_Y_MENUPAD) );
  display.print(" "); 
//  display.display();
}

// display the top menu
// index - currently selected top menu
void drawtopmenu( int8_t index) {
    display.fillScreen(BLACK);
//    display.fillRect(0,TOPMENU_Y,SCREENWIDTH,SCREENHEIGHT-TOPMENU_Y,BLACK); // erase old 
    display.setCursor(TOPMENU_X,TOPMENU_Y);
    display.printf("%s",menutitle);
    int i = (index/TOPMENU_LINES)*TOPMENU_LINES; // which group of menu items to display
    int last = i+NUM_MAIN_MENUS % TOPMENU_LINES; // show only up to the last menu item
    if ((i + TOPMENU_LINES) <= NUM_MAIN_MENUS) last = i+TOPMENU_LINES; // handles case like 2nd of 3 menu pages
 //   int y=TOPMENU_Y+DISPLAY_Y_MENUPAD+DISPLAY_Y_OFFSET;
    int y=DISPLAY_Y_OFFSET;

    for (i; i< last ; ++i) {
      display.setCursor ( TOPMENU_X, y ); 
      display.print(topmenu[i].name);
    
	  if (i < NUM_VOICES) {			// first N items are always samples - show the sample filename
      display.printf("%-.22s",sample[voice[topmenuindex].sample].sname);
		 // strncpy(temp,sample[voice[i].sample].sname,DISPLAY_X-3); // 3 columns are used: selector, sample#, space
	  }
    
      y+=DISPLAY_CHAR_HEIGHT+DISPLAY_Y_MENUPAD;
    }
//    display.display();
} 

// display a sub menu item and its value
// index is the index into the current top menu's submenu array

void drawsubmenu( int8_t index) {
    submenu * sub;
    sub=topmenu[topmenuindex].submenus; //get pointer to the submenu array
    // print the name text
    int y= SUBMENU_Y+DISPLAY_Y_MENUPAD+(index % SUBMENU_LINES)*(DISPLAY_CHAR_HEIGHT+DISPLAY_Y_MENUPAD); // Y position of this menu index
    display.setCursor (SUBMENU_X,y) ; // set cursor to parameter name field
    display.print(sub[index].name); 
    
    // print the value
    display.setCursor (SUBMENU_VALUE_X, y ); // set cursor to parameter value field
    display.print("      "); // erase old value
    display.setCursor (SUBMENU_VALUE_X, y ); // set cursor to parameter value field
    if (sub[index].step !=0) { // don't print dummy parameter 
      int16_t val=*sub[index].parameter;  // fetch the parameter value
      char temp[5];
      switch (sub[index].ptype) {
        case TYPE_INTEGER:   // print the value as an unsigned integer    
          sprintf(temp,"%4d",val); // lcd.print doesn't seem to print uint8 properly
          display.print(temp);  
          display.print(" ");  // blank out any garbage
          break;
        case TYPE_FLOAT:   // print the int value as a float  
          sprintf(temp,"%1.2f",(float)val/1000); // menu should have int value between -1000 to +1000 so float is -1 to +1
          display.print(temp);  
          display.print(" ");  // blank out any garbage
          break;
        case TYPE_TEXT:  // use the value to look up a string
          if (val > sub[index].max) val=sub[index].max; // sanity check
          if (val < 0) val=0; // min index is 0 for text fields
          display.print(sub[index].ptext[val]); // parameter value indexes into the string array
          display.print(" ");  // blank out any garbage
          break;
		    case TYPE_FILENAME:  // print filename of sample using index in min
		      display.setCursor (SUBMENU_X, y ); // leave room for selector
          display.printf("%-22s",sample[voice[topmenuindex].sample].sname);
        break;
        default:
        case TYPE_NONE:  // blank out the field
          display.print("     ");
          break;
      } 
    }
//    display.display(); 
}

// display sub menus of the current topmenu

void drawsubmenus() {
    int8_t index,len;
    index= topmenu[topmenuindex].submenuindex; // submenu field index
    len= topmenu[topmenuindex].numsubmenus; // number of submenu items
    submenu * sub=topmenu[topmenuindex].submenus; //get pointer to the current submenu array
//    display.fillRect(0,TOPMENU_Y,SCREENWIDTH,SCREENHEIGHT-TOPMENU_Y,BLACK); // erase old 
    display.fillScreen(BLACK);
    display.setCursor(0,DISPLAY_Y_OFFSET);
    display.printf("%s",topmenu[topmenuindex].name); // show the menu we came from at top of screen
    int i = (index/SUBMENU_LINES)*SUBMENU_LINES; // which group of menu items to display
    int last = i+len % SUBMENU_LINES; // show only up to the last menu item
    if ((i + SUBMENU_LINES) <= len) last = i+SUBMENU_LINES; // handles case like 2nd of 3 menu pages
    int y=SUBMENU_Y+DISPLAY_Y_MENUPAD;

    for (i; i< last ; ++i) {
      //display.setCursor ( SUBMENU_X, y ); 
      //display.print(sub[i].name);
      //y+=DISPLAY_CHAR_HEIGHT+DISPLAY_Y_MENUPAD;
      drawsubmenu(i);
    }
 //   display.display();
} 

/* get file size */
int16_t get_filesize(char * path) {
  File f, entry;
  f=SD.open(path); // open the path
  entry =  f.openNextFile(); // open the file - seems to be necessary
  return f.size(); // size in bytes
}


/* function to get the content of a given folder */
int16_t get_dir_content(char * path) {
  File dir; 
  int16_t nfiles;
  dir = SD.open(path); // open the path
  nfiles=0;
  while (true) {
    File entry =  dir.openNextFile();
    if (! entry) break;
 //   Serial.print(entry.name());
    if (entry.isDirectory()) {
 //     Serial.println("/");
	    strcpy(files[nfiles].name,entry.name());
      files[nfiles].name[0]=toupper(files[nfiles].name[0]); // so they sort properly
		  files[nfiles].isdir=true;
      files[nfiles].size=0;
		  ++ nfiles;
    //  printDirectory(entry, numTabs + 1); //recursive call from SD example

    } else {
 //     Serial.print("\t\t");
//      Serial.println(entry.size(), DEC); // files have sizes, directories do not
	    strcpy(files[nfiles].name,entry.name());
      files[nfiles].name[0]=toupper(files[nfiles].name[0]); // so they sort properly
		  files[nfiles].isdir=false;
      files[nfiles].size=entry.size();
		  ++ nfiles;
    }
    entry.close();
    if (nfiles >= (MAXFILES-1)) break; // hit the limit of our directory structure
  }
	qsort (files, nfiles, sizeof(fileinfo), comp);  // put them in alpha order
	strcpy(files[nfiles].name,".."); // last entry is to go up
	files[nfiles].isdir=0; // special case - don't treat it as a directory
	++nfiles;
	return nfiles;  
}

// display a list of files
// index - currently selected file
void drawfilelist( int16_t index) {

  //display.clearDisplay();
  display.fillScreen(BLACK);
//  display.setCursor(0,0);
//  display.printf("S%d /%s",topmenuindex,directory); // show sample # and current directory on top line
  int16_t i = (index/FILEMENU_LINES)*FILEMENU_LINES; // which group of menu items to display
  int last = i+numfiles % TOPMENU_LINES; // show only up to the last menu item
  if ((i + FILEMENU_LINES) <= numfiles) last = i+FILEMENU_LINES; // handles case like 2nd of 3 menu pages
  int y=FILEMENU_Y+DISPLAY_Y_MENUPAD;

  for (i; i< last ; ++i) {
    display.setCursor ( FILEMENU_X, y ); 
	  if (files[i].isdir) display.print("/"); // show its a directory  	
    display.printf("%-.24s",files[i].name);	
    y+=DISPLAY_CHAR_HEIGHT+DISPLAY_Y_MENUPAD;
  }
   // display.display();
} 

// popdir - go up one level in the directory tree
// returns 1 if at top level
bool popdir(void) {
  int16_t p;
  p=strlen(filepath);
  if (p <= ROOTLEN) return true;
  --p; // index of last character in directory string
  while (filepath[p] != '/') --p;
  filepath[p]=0; // null terminate directory string
  return 0;
}


// menu handler
// a run to completion state machine - it never blocks except while waiting for encoder button release
// allows the rest of the application to keep playing audio while parameters are adjusted
// modded July 2024 for the Pico groovebox - doesn't display top menus, scrolls thru submenus when button1 is pressed with encoder rotation

void domenus(void) {
  int16_t enc;
  int8_t index; 
  char temp[80]; // for file paths
  static int16_t fileindex=0;  // index of selected file/directory   
  static int16_t lastdir=0;  // index of last directory we looked at
  static int16_t lastfile=0;  // index of last file we looked at  
  bool exitflag;


//  ClickEncoder::Button button; 
  
  enc=Encoder1.getValue();
//  button= Encoder2.getButton();

  // process the menu encoder 
//  enc=P4Encoder.getValue();


  switch (uistate) {
    case TOPSELECT:  // scrolling thru top menu
      if ((enc !=0) && (currtouched & TRACK_BUTTON)) { // scroll thru submenus
        int topmenupage = (topmenuindex) / TOPMENU_LINES;  
        topmenuindex+=enc;
        if (topmenuindex <0) topmenuindex=0;  // we don't wrap menus around, just stop at the ends
        if (topmenuindex >=(NUM_MAIN_MENUS -1) ) topmenuindex=NUM_MAIN_MENUS-1; 
        topmenu[topmenuindex].submenuindex=0;  // start from the first item
        drawsubmenus();
        drawselector(topmenu[topmenuindex].submenuindex);  
        if (topmenuindex < NTRACKS) showpattern(topmenuindex); // show the piano roll if this is a track menu
      }
      if (!(currtouched & TRACK_BUTTON)) {
        uistate=SUBSELECT; // do submenu when button is released
        topmenu[topmenuindex].submenuindex=0;  // start from the first item
        drawsubmenus();
        drawselector(topmenu[topmenuindex].submenuindex);
        if (topmenuindex < NTRACKS) showpattern(topmenuindex); // show the piano roll if this is a track menu
      }  
      break;
    case SUBSELECT:  // scroll thru submenus
      if (enc !=0 ) { // move selector
        int submenupage = topmenu[topmenuindex].submenuindex / SUBMENU_LINES;  
        undrawselector(topmenu[topmenuindex].submenuindex);
        topmenu[topmenuindex].submenuindex+=enc;
        if (topmenu[topmenuindex].submenuindex <0) topmenu[topmenuindex].submenuindex=0;  // we don't wrap menus around, just stop at the ends
        if (topmenu[topmenuindex].submenuindex >=(topmenu[topmenuindex].numsubmenus -1) ) topmenu[topmenuindex].submenuindex=topmenu[topmenuindex].numsubmenus -1; 
        if ((topmenu[topmenuindex].submenuindex / SUBMENU_LINES) != submenupage) {
          drawsubmenus();  // redraw if we scrolled beyond the menu page
          if (topmenuindex < NTRACKS) showpattern(topmenuindex); // show the piano roll if this is a track menu
        }
        drawselector(topmenu[topmenuindex].submenuindex);   
      } 
      if (!digitalRead(ENC_SW)) { // submenu item has been selected so either go back to top or go to change parameter state
      //if ((Encoder1.getButton() == ClickEncoder::Clicked)) { // submenu item has been selected so either go back to top or go to change parameter state
	      submenu * sub;
		    sub=topmenu[topmenuindex].submenus; //get pointer to the submenu array
        if (sub[topmenu[topmenuindex].submenuindex].ptype ==TYPE_FILENAME ) { // if filebrowser menu type so we go into file browser
          
          //Serial.printf("path=%s\n",filepath);
          numfiles=get_dir_content(filepath);
			    fileindex=0;
          if (files[0].isdir) drawfilelist(fileindex=lastdir);
			    else drawfilelist(fileindex=lastfile);
          drawselector(fileindex); 
          uistate=FILEBROWSER;
          while(!digitalRead(ENC_SW));// dosequencers(); // keep sequencers running till button released
        }
        else {
          undrawselector(topmenu[topmenuindex].submenuindex);
          draweditselector(topmenu[topmenuindex].submenuindex); // show we are editing
          uistate=PARAM_INPUT;  // change the submenu parameter
          while(!digitalRead(ENC_SW)); // dosequencers(); // keep sequencers running till button released
        }
      }  
      
      if (currtouched & TRACK_BUTTON) uistate=TOPSELECT; // press button to select track again   

      break;
    case PARAM_INPUT:  // changing value of a parameter
      if (enc !=0 ) { // change value
//      Serial.printf("%d\n",enc);
        index= topmenu[topmenuindex].submenuindex; // submenu field index
        submenu * sub=topmenu[topmenuindex].submenus; //get pointer to the current submenu array
        int16_t temp=*sub[index].parameter + enc*sub[index].step; // menu code uses ints - convert to floats when needed
        if (temp < (int16_t)sub[index].min) temp=sub[index].min;
        if (temp > (int16_t)sub[index].max) temp=sub[index].max;
        *sub[index].parameter=temp;
        if (sub[index].handler != 0) (*sub[index].handler)();  // call the handler function
        drawsubmenu(index);
      }
      if (!digitalRead(ENC_SW)) { // stop changing parameter
      //if ((Encoder1.getButton() == ClickEncoder::Clicked)) { // stop changing parameter
        undrawselector(topmenu[topmenuindex].submenuindex);
        drawselector(topmenu[topmenuindex].submenuindex); // show we are selecting again
        uistate=SUBSELECT;
        while(!digitalRead(ENC_SW)); // dosequencers(); // keep sequencers running till button released
      }   
      break;
	  case FILEBROWSER:  // browse files - file structure is ./<filesroot>/<directory>/<file> ie all files must be in a directory and no more than 1 directory deep
      if (enc !=0 ) { // move selector
        int filespage = (fileindex) / FILEMENU_LINES;  
        undrawselector(fileindex);
        fileindex+=enc;
        if (fileindex <0) fileindex=0;  // we don't wrap menus around, just stop at the ends
        if (fileindex >=(numfiles -1) ) fileindex=numfiles-1; 
        if ((fileindex / FILEMENU_LINES) != filespage) {
          drawfilelist(fileindex);  // redraw if we scrolled beyond the menu page
        }
        drawselector(fileindex);    
      }
      if (!digitalRead(ENC_SW)) { // file item has been selected 
		    if (files[fileindex].isdir) {  // show nested directory	
          strcat(filepath,"/"); 
          strcat(filepath,files[fileindex].name); // save the current directory
          numfiles=get_dir_content(filepath);
			    fileindex=lastfile;
          drawfilelist(fileindex);
          drawselector(fileindex);
			    while(!digitalRead(ENC_SW));// loop here till encoder released, otherwise we go right back into select
		    }
		    else if (fileindex == (numfiles -1)) { // last file is always ".." so go up to directories
          popdir();  // go up one level
          numfiles=get_dir_content(filepath);
			    lastfile=lastdir=0;        // new directory so start at beginning
          drawfilelist(fileindex=lastdir);
          drawselector(lastdir);
			    while(!digitalRead(ENC_SW));// 
		    }
		    else {  // we have selected a file so load it
			    exitflag=0;
          while (!digitalRead(ENC_SW)) {   // long press to exit - way to get out of a very long file list
            if (Encoder1.getButton() == ClickEncoder::Held) {
					    exitflag=1;
					    break;
				    } 
          }      
			    if (exitflag) {  // don't load file, go back to root dir
				    lastfile=0;
				    strcpy(filepath,filesroot);
			    }
			    else {    
                 
				    lastfile=fileindex;  // remember where we were
				    char temp2[200];
				    strcpy(temp2,filepath);  // build the full file path
				    strcat(temp2,"/");
            strcat(temp2,files[fileindex].name);

            int32_t fsize=loadwav(temp2,0);  // null pointer just returns .wav data size in words
            //dosequencers(); // SD is really slow so try to keep things going
#ifdef DEBUG
	          Serial.printf("loading %s %d words\n",temp2,fsize);
#endif
            if (sample[track].samplearray !=0) free(sample[track].samplearray); // deallocate psram
            sample[track].samplesize=0; // this should stop sample playing
            if (fsize > 0) {
              uint8_t * p;
              if ((p=(uint8_t *)pmalloc(((fsize*2/PMALLOC_CHUNK)+1)*PMALLOC_CHUNK)) && (((uint32_t)p+fsize*2) < (PSRAM_ADDR+PSRAM_SIZE))) { // allocate memory in PMALLOC_CHUNK units to keep fragmentation to a minimum 
                sample[track].samplearray=(int16_t *)p;
                sample[track].samplesize=loadwav(temp2,p); // **** no error checking yet but this should always work
                memcpy((void *)sample[track].sname,files[fileindex].name,25); // copy first 25 chars of filename over
                sample[track].sname[24]=0;  // make sure its null terminated
#ifdef DEBUG
                Serial.printf("loaded %s %d words at addr %x\n",temp2,fsize,p);
#endif
              }
              else {
                Serial.printf("pmalloc failed\n");
                strcpy(sample[track].sname,"**Memory error**");
                sample[track].samplearray=0;
                sample[track].samplesize=0;
                sample[track].sname[0]=0;
              }
            }
            else strcpy(sample[track].sname,"**File load error**");
          }
			    topmenu[topmenuindex].submenuindex=0;  // restore submenu from the first item
			    drawsubmenus();
			    drawselector(topmenu[topmenuindex].submenuindex); 
          if (topmenuindex < NTRACKS) showpattern(topmenuindex); // show the piano roll if this is a track menu 
			    uistate=SUBSELECT;	
			    while(!digitalRead(ENC_SW)); // 
          
		  }
    } // end of case FILEBROWSER
  } // end of case statement
}


