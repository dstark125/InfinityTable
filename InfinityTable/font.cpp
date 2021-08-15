
#include "stdint.h"
#include "font.h"

//- https://www.fonts4free.net/5x5-dots-font.html
Character_t g_character_map[] =
{
    { ' ', 3,{
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0}}
    },
    { '0', 5,{
        {0,1,1,1,0},
        {1,0,0,1,1},
        {1,0,1,0,1},
        {1,1,0,0,1},
        {0,1,1,1,0}}
    },
    { '1', 3,{
        {0,1,0,0,0},
        {1,1,0,0,0},
        {0,1,0,0,0},
        {0,1,0,0,0},
        {1,1,1,0,0}}
    },
    { '2', 5,{
        {1,1,1,1,0},
        {0,0,0,0,1},
        {0,1,1,1,0},
        {1,0,0,0,0},
        {1,1,1,1,1}}
    },
    { '3', 5,{
        {1,1,1,1,0},
        {0,0,0,0,1},
        {0,0,1,1,0},
        {0,0,0,0,1},
        {1,1,1,1,0}}
    },
    { '4', 5,{
        {1,0,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,1},
        {0,0,0,0,1},
        {0,0,0,0,1}}
    },
    { '5', 5,{
        {1,1,1,1,1},
        {1,0,0,0,0},
        {1,1,1,1,0},
        {0,0,0,0,1},
        {1,1,1,1,0}}
    },
    { '6', 5,{
        {0,1,1,1,1},
        {1,0,0,0,0},
        {1,1,1,1,0},
        {1,0,0,0,1},
        {0,1,1,1,0}}
    },
    { '7', 5,{
        {1,1,1,1,1},
        {0,0,0,0,1},
        {0,0,0,1,0},
        {0,0,0,1,0},
        {0,0,0,1,0}}
    },
    { '8', 5,{
        {0,1,1,1,0},
        {1,0,0,0,1},
        {0,1,1,1,0},
        {1,0,0,0,1},
        {0,1,1,1,0}}
    },
    {'9', 5,{
        {0,1,1,1,0},
        {1,0,0,0,1},
        {0,1,1,1,1},
        {0,0,0,0,1},
        {0,0,1,1,0}}
    },
    { 'A', 5,{
        {0,1,1,1,0},
        {1,0,0,0,1},
        {1,1,1,1,1},
        {1,0,0,0,1},
        {1,0,0,0,1}}
    },
    { 'B', 5,{
        {1,1,1,1,0},
        {1,0,0,0,1},
        {1,1,1,1,0},
        {1,0,0,0,1},
        {1,1,1,1,0}}
    },
    { 'C', 5,{
        {0,1,1,1,1},
        {1,0,0,0,0},
        {1,0,0,0,0},
        {1,0,0,0,0},
        {0,1,1,1,1}}
    },
    { 'D', 5,{
        {1,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,1,1,1,0}}
    },
    { 'E', 5,{
        {1,1,1,1,1},
        {1,0,0,0,0},
        {1,1,1,1,1},
        {1,0,0,0,0},
        {1,1,1,1,1}}
    },
    { 'F', 5,{
        {1,1,1,1,1},
        {1,0,0,0,0},
        {1,1,1,0,0},
        {1,0,0,0,0},
        {1,0,0,0,0}}
    },
    { 'G', 5,{
        {0,1,1,1,1},
        {1,0,0,0,0},
        {1,0,1,1,1},
        {1,0,0,0,1},
        {0,1,1,1,1}}
    },
    { 'H', 5,{
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,1,1,1,1},
        {1,0,0,0,1},
        {1,0,0,0,1}}
    },
    { 'I', 3,{
        {1,1,1,0,0},
        {0,1,0,0,0},
        {0,1,0,0,0},
        {0,1,0,0,0},
        {1,1,1,0,0}}
    },
    { 'J', 5,{
        {0,0,1,1,1},
        {0,0,0,0,1},
        {0,0,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,0}}
    },
    { 'K', 5,{
        {1,0,0,1,0},
        {1,0,1,0,0},
        {1,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,0,1}}
    },
    { 'L', 5,{
        {1,0,0,0,0},
        {1,0,0,0,0},
        {1,0,0,0,0},
        {1,0,0,0,0},
        {1,1,1,1,1}}
    },
    { 'M', 5,{
        {1,1,0,1,1},
        {1,0,1,0,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,0,0,0,1}}
    },
    { 'N', 5,{
        {1,1,0,0,1},
        {1,0,1,0,1},
        {1,0,1,0,1},
        {1,0,1,0,1},
        {1,0,0,1,1}}
    },
    { 'O', 5,{
        {0,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,0}}
    },
    { 'P', 5,{
        {1,1,1,1,0},
        {1,0,0,0,1},
        {1,1,1,1,0},
        {1,0,0,0,0},
        {1,0,0,0,0}}
    },
    { 'Q', 5,{
        {0,1,1,0,0},
        {1,0,0,1,0},
        {1,0,0,1,0},
        {1,0,0,1,0},
        {0,1,1,1,1}}
    },
    { 'R', 5,{
        {1,1,1,1,0},
        {1,0,0,0,1},
        {1,1,1,1,0},
        {1,0,0,1,0},
        {1,0,0,0,1}}
    },
    { 'S', 5,{
        {0,1,1,1,1},
        {1,0,0,0,0},
        {0,1,1,1,0},
        {0,0,0,0,1},
        {1,1,1,1,0}}
    },
    { 'T', 5,{
        {1,1,1,1,1},
        {0,0,1,0,0},
        {0,0,1,0,0},
        {0,0,1,0,0},
        {0,0,1,0,0}}
    },
    { 'U', 5,{
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,0}}
    },
    { 'V', 5,{
        {1,0,0,0,1},
        {1,0,0,0,1},
        {0,1,0,1,0},
        {0,1,0,1,0},
        {0,0,1,0,0}}
    },
    { 'W', 5,{
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,0,1,0,1},
        {1,1,0,1,1},
        {1,0,0,0,1}}
    },
    { 'X', 5,{
        {1,0,0,0,1},
        {0,1,0,1,0},
        {0,0,1,0,0},
        {0,1,0,1,0},
        {1,0,0,0,1}}
    },
    { 'Y', 5,{
        {1,0,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,0},
        {0,0,1,0,0},
        {0,0,1,0,0}}
    },
    { 'Z', 5,{
        {1,1,1,1,1},
        {0,0,0,0,1},
        {0,1,1,1,0},
        {1,0,0,0,0},
        {1,1,1,1,1}}
    },
    { '!', 1,{
        {1,0,0,0,0},
        {1,0,0,0,0},
        {1,0,0,0,0},
        {0,0,0,0,0},
        {1,0,0,0,0}}
    },
    { '.', 2,{
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0},
        {1,1,0,0,0},
        {1,1,0,0,0}}
    },
    { ',', 2,{
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,1,0,0,0},
        {1,0,0,0,0}}
    },
    { '%', 5,{
        {1,0,0,0,1},
        {0,0,0,1,0},
        {0,0,1,0,0},
        {0,1,0,0,0},
        {1,0,0,0,1}}
    },
    { '(', 2,{
        {0,1,0,0,0},
        {1,0,0,0,0},
        {1,0,0,0,0},
        {1,0,0,0,0},
        {0,1,0,0,0}}
    },
    { ')', 2,{
        {1,0,0,0,0},
        {0,1,0,0,0},
        {0,1,0,0,0},
        {0,1,0,0,0},
        {1,0,0,0,0}}
    },
    { '{', 3,{
        {0,0,1,0,0},
        {0,1,0,0,0},
        {1,1,0,0,0},
        {0,1,0,0,0},
        {0,0,1,0,0}}
    },
    { '}', 3,{
        {1,0,0,0,0},
        {0,1,0,0,0},
        {0,1,1,0,0},
        {0,1,0,0,0},
        {1,0,0,0,0}}
    },
    { '=', 5,{
        {0,0,0,0,0},
        {1,1,1,1,1},
        {0,0,0,0,0},
        {1,1,1,1,1},
        {0,0,0,0,0}}
    },
    { '+', 5,{
        {0,0,1,0,0},
        {0,0,1,0,0},
        {1,1,1,1,1},
        {0,0,1,0,0},
        {0,0,1,0,0}}
    },
    { '-', 5,{
        {0,0,0,0,0},
        {0,0,0,0,0},
        {1,1,1,1,1},
        {0,0,0,0,0},
        {0,0,0,0,0}}
    },
    { '_', 5,{
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0},
        {1,1,1,1,1}}
    },
    { '?', 5,{
        {0,1,1,1,0},
        {1,0,0,0,1},
        {0,0,1,1,0},
        {0,0,0,0,0},
        {0,0,1,0,0}}
    },
    { ':', 1,{
        {0,0,0,0,0},
        {1,0,0,0,0},
        {0,0,0,0,0},
        {1,0,0,0,0},
        {0,0,0,0,0}}
    },
    { '/', 5,{
        {0,0,0,0,1},
        {0,0,0,1,0},
        {0,0,1,0,0},
        {0,1,0,0,0},
        {1,0,0,0,0}}
    },
    { '\\', 5,{
        {1,0,0,0,0},
        {0,1,0,0,0},
        {0,0,1,0,0},
        {0,0,0,1,0},
        {0,0,0,0,1}}
    },
    { '\'', 1,{
        {1,0,0,0,0},
        {1,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0}}
    },
    { '\"', 3,{
        {1,0,1,0,0},
        {1,0,1,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0}}
    },
    { '#', 3,{
        {0,1,0,1,0},
        {1,1,1,1,1},
        {0,1,0,1,0},
        {1,1,1,1,1},
        {0,1,0,1,0}}
    },
};

#define NUM_CHARS_IN_MAP (sizeof(g_character_map) / sizeof(Character_t))

Character_t Font_GetCharacter(char character)
{
    //- Searches the global character map for the character.
    //- If the character doesn't exist, it returns all 0's (no LEDs on)
    Character_t empty_char = { 0 };
    uint32_t i;
    char character_upper;
    //- Convert to uppercase... We dont' have lowercase letters right now.
    character_upper = (character >= 'a' && character <= 'z') ? (character - ('a' - 'A')) : character;
    //- Search map
    for (i = 0; i < NUM_CHARS_IN_MAP; i++)
    {
        if (character_upper == g_character_map[i].character)
        {
            return g_character_map[i];
        }
    }
    return empty_char;
}
