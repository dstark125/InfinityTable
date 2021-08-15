
#include "config.h"

//- Note this ought to just map NUM_STRIPS, but this means the font module is portable elsewhere easily
#define CHARACTER_ROWS (5)
#define CHARACTER_COLS (5)

//- Structure shared between font.c and led control so it know how to draw
typedef struct Character_s
{
    char    character; //- Char this represents
    uint8_t numCols;   //- How wide this char is. This way not all chars are 5 columns
    uint8_t leds[CHARACTER_ROWS][CHARACTER_COLS]; //- Which LEDs are on in a 5x5 grid
}Character_t;

Character_t Font_GetCharacter(char character);
