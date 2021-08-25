
/*
 Name:		InfinityTable.ino
 Created:	2/15/2021 8:54:25 PM
 Author:	David
*/

#include "FastLED.h"
#include "config.h"
#include "font.h"
#include "LEDControl.h"
#include "WifiControl.h"
#include "Microphone.h"
#include "Log.h"

#define NUM_WALLS (4)
#define MIN_LERP_T ((float)1U / (float)UINT8_MAX) //- Minimum lerp_t value that will turn an LED on

//- Fire macros
#define FIRE_X (NUM_STRIPS)
#define FIRE_Y (16)
#define CENTREX ((FIRE_X / 2) - 1)
#define CENTREY ((FIRE_Y / 2) - 1)

//- Helper structures
typedef struct Wall_s
{
    uint32_t firstIdx;
    uint32_t len;
}Wall_t;

typedef struct Paralellogram_s
{
    uint32_t offset;
    uint32_t speed;
    uint32_t thickness;
    CRGB     color;
    bool     direction;
}Paralellogram_t;

typedef struct SpectrumHistory_s
{
    Microphone_Spectrum_t spectrums[SPECTRUM_HISTORY_LEN];
    uint32_t              times[SPECTRUM_HISTORY_LEN];
    uint8_t               frequencyMaxs[MICROPHONE_F_BINS];
    uint8_t               head;
    uint8_t               droppingMax;
}SpectrumHistory_t;

typedef struct FireLEDs_s
{
    CRGB leds[FIRE_X][FIRE_Y];
}FireLEDs_t;

//- Global data structure
typedef struct LEDControl_Data_s
{
    CRGB*               leds[NUM_STRIPS];
    bool                displayTextDirty;
    LEDControl_Mode_t   curMode;
    uint32_t            lastAnimate;
    uint32_t            animateFrameNumber;
    uint32_t            scrollCursor;
    //- The CRGB version of config.red/green/blue
    CRGB                solidColor;
    CRGB                solidColorHasChanged;
    bool                lastColorWasBlack;
    //- Walls
    Wall_t              walls[NUM_WALLS];
    //- Parallelograms
    Paralellogram_t     paralellograms[MAX_PARALLELOGRAMS];
    uint32_t            numParalellograms;
    CRGB                waveBackground;
    //- Microphone
    Microphone_Spectrum_t spectrum;
    SpectrumHistory_t   spectrumHistory;
    //- Fire
    CRGBPalette16       firePallet;
    //- Run-time config stuff
    int                 BRsize;
    int                 RGsize;
    int                 GBsize;
}LEDControl_Data_t;

CRGB  g_RAWLEDS[NUM_STRIPS][TOTAL_LEDS_PER_STRIP] = { 0 };

LEDControl_Config_t g_config;
LEDControl_Data_t   g_data;

//- Data structures used to programatically transition modes
typedef bool (*UpdateFuncPtr)(void);
typedef struct ModeData_s
{
    UpdateFuncPtr modeInitFunc;
    UpdateFuncPtr modeUpdateFunc;
}ModeData_t;

//- Need function prototypes before the modeData table
static bool ModeTextUpdate(void);
static bool ModeTextInit(void);
static bool ModeRainbowUpdate(void);
static bool ModeRainbowInit(void);
static bool ModeSolidUpdate(void);
static bool ModeSolidInit(void);
static bool ModeParallelogramUpdate(void);
static bool ModeParallelogramInit(void);
static bool ModeFlashyRainbowUpdate(void);
static bool ModeFlashyInit(void);
static bool ModeFlashyColorUpdate(void);
static bool ModeFlashyInit(void);
static bool ModeFireUpdate(void);
static bool ModeFireInit(void);
static bool ModeMicrophoneEqInverseUpdate(void);
static bool ModeMicrophoneEqSolidUpdate(void);
static bool ModeMicrophoneEqRainbowUpdate(void);
static bool ModeMicrophoneEqFullUpdate(void);
static bool ModeMicrophoneAmbientUpdate(void);
static bool ModeMicrophoneInit(void);
static bool ModeOffInit(void);

ModeData_t  modeData[LED_MODE_COUNT] = 
{
    {ModeRainbowInit,       ModeRainbowUpdate},
    {ModeTextInit,          ModeTextUpdate}, 
    {ModeSolidInit,         ModeSolidUpdate}, 
    {ModeParallelogramInit, ModeParallelogramUpdate},
    {ModeFlashyInit,        ModeFlashyRainbowUpdate},
    {ModeFlashyInit,        ModeFlashyColorUpdate},
    {ModeFireInit,          ModeFireUpdate},
    {ModeMicrophoneInit,    ModeMicrophoneEqInverseUpdate},
    {ModeMicrophoneInit,    ModeMicrophoneEqSolidUpdate},
    {ModeMicrophoneInit,    ModeMicrophoneEqRainbowUpdate},
    {ModeMicrophoneInit,    ModeMicrophoneEqFullUpdate},
    {ModeMicrophoneInit,    ModeMicrophoneAmbientUpdate},
    {ModeOffInit,           NULL},
};

//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------
//- Helper Functions -----------------------------------------------------------------
//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------

static bool ColorIsEqual(CRGB c1, CRGB c2)
{
    return (c1.r == c2.r) && (c1.g == c2.g) && (c1.b == c2.b);
}

//- Helper function to generate indices
static uint32_t DecLEDIdx(uint32_t idx, uint32_t decNum = 1)
{
    //- Decrement the index by decNum, wrapping around.
    //- Note if decNum is > NUM_LEDS_PER_STRIP, this wont work
    //- Could assert or something but I think this is OK for now.
    int32_t new_idx = (int32_t)idx - (int32_t)decNum;
    if (new_idx < 0)
    {
        return  NUM_LEDS_PER_STRIP + new_idx;
    }
    return new_idx;
}

//- Helper function to generate indices
static uint32_t IncLEDIdx(uint32_t idx, uint32_t incNum = 1)
{
    //- Increment the index by incNum wrapping at max index
    //- Note this does handle incNum > NUM_LEDS_PER_STRIP
    return (idx + incNum) % NUM_LEDS_PER_STRIP;
}

//- Get the FastLED value to be used in a lerp operation
static fract8 FloatLerpToFastLED(float lerpT)
{
    //- I'm used to using lerpT 0.0-1.0
    //- So this function maps 0.0-1.0 to the fastLED value
    lerpT = lerpT > 1.0f ? 1.0f : lerpT;
    fract8 lerp_fastled = UINT8_MAX - (lerpT * UINT8_MAX);
    return lerp_fastled;
}

//- This function should be called if we want to set a new solid color for the table
static void UpdateSolidColor(uint8_t r, uint8_t g, uint8_t b)
{
    //- It updates all of the appropriate data structures
    //- Also sets some flags for later consumption
    if ((g_data.solidColor.r == 0) && (g_data.solidColor.g == 0) && (g_data.solidColor.b == 0))
    {
        g_data.lastColorWasBlack = true;
    }
    //- Update the config object (basically status)
    g_config.red   = r;
    g_config.green = g;
    g_config.blue  = b;
    //- Set the data to be used by mode functions
    g_data.solidColor = CRGB(r,g,b);
    g_data.solidColorHasChanged = true;
}

//- Helper for using CRGB class... Just calls the main function.
static void UpdateSolidColorCRGB(CRGB color)
{
    UpdateSolidColor(color.r, color.g, color.b);
}

//- Based on the initial configuration, grabs the rainbow color associated with index
static CRGB GetRainbowColor(uint32_t idx)
{
    CRGB color;
    //- Note idx should be in the range of 0 to (NUM_LEDS_PER_STRIP -1)
    //- Mostly copied from:
    //- https://www.reddit.com/r/gifs/comments/fx5dgf/this_600_led_infinity_mirror_coffee_table_i_built/
    if( idx < g_data.BRsize)
    {
        int     i = idx;
        uint8_t val = (uint8_t)round(255 * float(i) / float(g_data.BRsize));
        color = CRGB(val, 0, 255 - val);
    }
    else if (idx < (g_data.RGsize + g_data.BRsize))
    {
        int     i = (idx - g_data.BRsize);
        uint8_t val = (uint8_t)round(255 * float(i) / float(g_data.RGsize));
        color = CRGB(255 - val, val, 0);
    }
    else
    {
        int     i = ((idx - g_data.BRsize) - g_data.RGsize);
        uint8_t val = (uint8_t)round(255 * float(i) / float(g_data.GBsize));
        color = CRGB(0, 255 - val, val);
    }
    return color;
}

//- Copy the data from stip 0 to the other strips quickly
static void CopyFirstStripToOthers(void)
{
    for (int32_t i = 1; i < NUM_STRIPS; i++)
    {
        (void)memcpy(g_data.leds[i], g_data.leds[0], sizeof(CRGB) * NUM_LEDS_PER_STRIP);
    }
}

//- Fill the whole table with a single color
static void FillTableSolid(CRGB color)
{
    fill_solid(g_data.leds[0], NUM_LEDS_PER_STRIP, color);
    CopyFirstStripToOthers();
}

//- Fill the whole table with a rainbow
static bool FillRainbowTable(void)
{
    //- Just do the work for the first strip, then copy it
    for (int32_t i = 0; i < NUM_LEDS_PER_STRIP; i++)
    {
        g_data.leds[0][i] = GetRainbowColor(i);
    }
    CopyFirstStripToOthers();
}

//- Check if it is time to perform an animation, based on config
static bool IsTimeToAnimate(void)
{
    bool animate = false;
    //- If scrollspeed is 1, don't do anything
    if (g_config.scrollSpeed > 1)
    {
        //- Check if it is time to do the next animation
        int32_t time_now = millis();
        uint32_t scroll_time;
        float    scroll_percentage;
        //- Compute the time between frames
        scroll_percentage = (float)g_config.scrollSpeed / 100.0f;
        scroll_time = (uint32_t)(float)DESIRED_LOOP_TIME / scroll_percentage;
        //- Check time since last frame
        if (time_now - g_data.lastAnimate >= scroll_time)
        {
            //- It's time, reset data
            g_data.lastAnimate = time_now;
            g_data.animateFrameNumber++;
            animate = true;
        }
    }
    return animate;
}

//- Scrlls the LEDs if it's animation time.
static bool ScrollLEDs(bool clockwise)
{
    bool needs_draw = false;
    //- Update animation if needed
    if (IsTimeToAnimate())
    {
        //- Move the cursor
        g_data.scrollCursor = clockwise ? IncLEDIdx(g_data.scrollCursor) : DecLEDIdx(g_data.scrollCursor);
        //- Iterate strips, moving the data
        for (uint32_t i = 0; i < NUM_STRIPS; i++)
        {
            if (clockwise)
            {
                CRGB last_led;
                last_led = g_data.leds[i][NUM_LEDS_PER_STRIP - 1LLU];
                memmove(g_data.leds[i] + 1LLU, g_data.leds[i], sizeof(CRGB) * (NUM_LEDS_PER_STRIP - 1LLU));
                g_data.leds[i][0] = last_led;
            }
            else
            {
                CRGB first_led;
                first_led = g_data.leds[i][0];
                memmove(g_data.leds[i], g_data.leds[i] + 1LLU, sizeof(CRGB) * (NUM_LEDS_PER_STRIP - 1LLU));
                g_data.leds[i][NUM_LEDS_PER_STRIP - 1LLU] = first_led;
            }
        }
        needs_draw = true;
    }
    return needs_draw;
}

//- Maps text to LEDs
static void FillLEDsWithText(uint32_t start, bool clearFirst = true, const char* text = NULL)
{
    uint32_t display_len;
    uint32_t pixel_col;
    uint32_t num_filled;
    uint32_t i;
    const char* display_text;
    //- You may not want to blank all the LEDs if you want to say make a cool background color or something
    if (clearFirst)
    {
        FillTableSolid(CRGB::Black);
    }
    //- If text not provided, use text from web server
    display_text = text == NULL ? g_config.displayText : text;
    pixel_col = start;
    num_filled = 0;
    //- Iterate the pixels, mapping characters
    display_len = strlen(display_text);
    for (i = 0; i < display_len; i++)
    {
        Character_t this_char;
        uint32_t char_row;
        uint32_t char_col;
        //- Get this character, loop rows and columns setting them to solid color if they are 1
        this_char = Font_GetCharacter(display_text[i]);
        for (char_col = 0; char_col < this_char.numCols; char_col++)
        {
            for (char_row = 0; char_row < CHARACTER_ROWS; char_row++)
            {
                if (this_char.leds[char_row][char_col] == 1U)
                {
                    g_data.leds[char_row][pixel_col] = g_data.solidColor;
                }
            }
            pixel_col = IncLEDIdx(pixel_col);
            num_filled++;
            if (num_filled >= NUM_LEDS_PER_STRIP)
            {
                break;
            }
        }
        //- Put one column space between characters
        pixel_col = IncLEDIdx(pixel_col);
        num_filled++;
        if (num_filled >= NUM_LEDS_PER_STRIP)
        {
            break;
        }
    }
}

//- Checks if there has been an update made to the solid color
//- If there has, it applies it to the LEDs that are not black.
static bool CheckSolidColor(void)
{
    //- Check for change
    if (g_data.solidColorHasChanged)
    {
        uint32_t i;
        uint32_t j;
        //- Iterate all LEDs, updating colors of the ones that aren't black.
        for (i = 0; i < NUM_STRIPS; i++)
        {
            for (j = 0; j < NUM_LEDS_PER_STRIP; j++)
            {
                if (g_data.leds[i][j].r != 0 || g_data.leds[i][j].g != 0 || g_data.leds[i][j].b != 0)
                {
                    g_data.leds[i][j] = g_data.solidColor;
                }
            }
        }
        g_data.solidColorHasChanged = false;
    }
}

//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------
//- Mode Functions -------------------------------------------------------------------
//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------
//- Text Mode ------------------------------------------------------------------------
//------------------------------------------------------------------------------------

static bool ModeTextUpdate(void)
{
    bool need_redraw = false;
    if (g_data.displayTextDirty) 
    {
        //- Text changed, start redrawing from the current scroll cursor (makes it smooth add/delete)
        FillLEDsWithText(g_data.scrollCursor);
        g_data.displayTextDirty = false;
        return true;
    }
    else if (g_data.lastColorWasBlack) 
    {
        //- LEDS were disabled last time and need to be filled again
        g_data.lastColorWasBlack = false;
        FillLEDsWithText(g_data.scrollCursor);
        return true;
    }
    else
    {
        //- No updates, just handle solid color changes if there are some
        need_redraw = CheckSolidColor();
    }
    //- Scroll the LEDs if time
    return ScrollLEDs(false) || need_redraw;
}

static bool ModeTextInit(void)
{
    //- Start text at 0, re-initialize data
    FillLEDsWithText(0U);
    g_data.scrollCursor = 0U;
    g_data.lastAnimate = millis();
    return true;
}

//------------------------------------------------------------------------------------
//- Rainbow Mode ---------------------------------------------------------------------
//------------------------------------------------------------------------------------

static bool ModeRainbowUpdate(void)
{
    //- Just scroll
    return ScrollLEDs(true);
}

static bool ModeRainbowInit(void)
{
    //- Fiill the table and re-initialize data
    FillRainbowTable();
    g_data.lastAnimate = millis();
    return true;
}

//------------------------------------------------------------------------------------
//- Solid Color Mode -----------------------------------------------------------------
//------------------------------------------------------------------------------------

static bool ModeSolidUpdate(void)
{
    //- Only update if there's a new solid color
    bool needs_redraw = g_data.solidColorHasChanged;
    if (needs_redraw)
    {
        uint32_t i;
        //- Just fill with one solid color
        FillTableSolid(g_data.solidColor);
        g_data.solidColorHasChanged = false;
    }
    return needs_redraw;
}

static bool ModeSolidInit(void)
{
    //- Fill with the current color
    FillTableSolid(g_data.solidColor);
    return true;
}

//------------------------------------------------------------------------------------
//- Parallelogram Mode ---------------------------------------------------------------
//------------------------------------------------------------------------------------

static bool ModeParallelogramUpdate(void)
{
    bool needs_redraw = false;
    //- Honors animation rate
    if (IsTimeToAnimate())
    {
        g_data.solidColorHasChanged = false; //- We set this every frame, so don't care about solid color change
        g_data.waveBackground = g_data.solidColor;
        //- Fill the background solid - we have no idea what's going where
        FillTableSolid(g_data.waveBackground);
        //- Iterate parallelograms
        for (int32_t i = 0; i < g_data.numParalellograms; i++)
        {
            Paralellogram_t* par = &g_data.paralellograms[i];
            uint32_t animate_place_in_strip;
            //- Get the rainbow color, based on this one index and the frame number
            animate_place_in_strip = (((NUM_LEDS_PER_STRIP / g_data.numParalellograms) * i) + g_data.animateFrameNumber) % NUM_LEDS_PER_STRIP;
            par->color = GetRainbowColor(animate_place_in_strip);
            //- Draw this parallelogram
            for (int32_t j = 0; j < NUM_STRIPS; j++)
            {
                for (int32_t k = 0; k < par->thickness; k++)
                {
                    uint32_t idx = par->direction ? IncLEDIdx(par->offset, j + k) : DecLEDIdx(par->offset, j + k);
                    g_data.leds[j][idx] = par->color;
                }
            }
            //- Only update the offset at the speed
            if ((g_data.animateFrameNumber % par->speed) == 0)
            {
                par->offset = (par->direction ? IncLEDIdx(par->offset, 1) : DecLEDIdx(par->offset, 1));
            }
        }
        needs_redraw = true;
    }
    return needs_redraw;
}

//- Comparison function for sorting the parallelograms
static int ParallelogramCompareFunc(const void* a, const void* b) 
{
    Paralellogram_t* wave_a = (Paralellogram_t*)a;
    Paralellogram_t* wave_b = (Paralellogram_t*)b;
    //- Sort based on thickness
    if (wave_a->thickness < wave_b->thickness)
    {
        return 1;
    }
    else if (wave_a->thickness > wave_b->thickness)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

static bool ModeParallelogramInit(void)
{
    //- Generate random parallelograms
    g_data.waveBackground = g_data.solidColor;
    g_data.numParalellograms = random(4, MAX_PARALLELOGRAMS);
    for (int32_t i = 0; i < g_data.numParalellograms; i++)
    {
        //- Start with them equally spaced
        g_data.paralellograms[i].offset = ((NUM_LEDS_PER_STRIP / g_data.numParalellograms) * i) % NUM_LEDS_PER_STRIP; //- % shouldn't be necessary but just in case
        g_data.paralellograms[i].speed = random(1, PARALLELOGRAM_MAX_SPEED);
        g_data.paralellograms[i].thickness = random(2, 50);
        //- Iterate direction every other one
        g_data.paralellograms[i].direction = i % 2;
        g_data.paralellograms[i].color = GetRainbowColor(g_data.paralellograms[i].offset);
    }
    //- Sort so the thickest are first
    //- This makes it so that all are always drawn (thick one would occlude smaller ones)
    qsort(g_data.paralellograms, g_data.numParalellograms, sizeof(Paralellogram_t), ParallelogramCompareFunc);
    //- Call the update function cause it actually draws
    ModeParallelogramUpdate();
    return true;
}

//------------------------------------------------------------------------------------
//- Flashy Colors Mode ---------------------------------------------------------------
//------------------------------------------------------------------------------------

//- Update function for flashy mode, taken from reddit:
//- https://www.reddit.com/r/gifs/comments/fx5dgf/this_600_led_infinity_mirror_coffee_table_i_built/
//- https://pastebin.com/tTuRDBYw
static bool FlashyFadeUpdate(bool useRainbow)
{
    bool needs_redraw = false;
    if (IsTimeToAnimate())
    {        
        //- Random columns and rows to turn on this frame
        int on_strip[FLASHY_RAINBOW_NUM_FLASHES];
        int on_led[FLASHY_RAINBOW_NUM_FLASHES];
        //- Get random flashes locations on the table
        for (int i = 0; i < FLASHY_RAINBOW_NUM_FLASHES; i++)
        {
            on_strip[i] = random8(0, NUM_STRIPS);
            on_led[i]   = random8(0, NUM_LEDS_PER_STRIP);
        }
        //- Fade all of the current LEDs
        for (int strip_idx = 0; strip_idx < NUM_STRIPS; strip_idx++)
        {
            for (int led_idx = 0; led_idx < NUM_LEDS_PER_STRIP; led_idx++)
            {
                g_data.leds[strip_idx][led_idx].fadeToBlackBy(FLASHY_RAINBOW_FADE_RATE);
            }
        }
        //- Set the color for the flashed LEDs
        for (int i = 0; i < FLASHY_RAINBOW_NUM_FLASHES; i++)
        {
            uint32_t strip_idx = on_strip[i];
            uint32_t led_idx   = on_led[i];
            CRGB color;
            //- Choose from solid or rainbow
            color = useRainbow ? GetRainbowColor((led_idx + g_data.scrollCursor) % NUM_LEDS_PER_STRIP) : g_data.solidColor;
            //turns on LEDs that were selected
            g_data.leds[strip_idx][led_idx] = color;
        }
        g_data.scrollCursor++;
        needs_redraw = true;
    }
    return needs_redraw;
}

static bool ModeFlashyRainbowUpdate(void)
{
    //- Update with normal function, rainbow mode
    return FlashyFadeUpdate(true);
}

static bool ModeFlashyColorUpdate(void)
{
    //- Update with normal function, solid color mode
    return FlashyFadeUpdate(false);
}

static bool ModeFlashyInit(void)
{
    //- Just empty the table at first - LEDs will flash on
    FillTableSolid(CRGB::Black);
    g_data.scrollCursor = 0U;
    g_data.lastAnimate = millis();
    return true;
}

//------------------------------------------------------------------------------------
//- Fire Mode ------------------------------------------------------------------------
//------------------------------------------------------------------------------------

//- This is an implementation of Fire2012... I don't like it as much as Fire2018
//- https://github.com/FastLED/FastLED/blob/master/examples/Fire2012/Fire2012.ino
//- #define NUM_LEDS_FIRE (30)
//- #define FIRE_COOLING                    (55) 
//- #define FIRE_SPARKING                   (120)
//- static void Fire2012(void)
//- {
//-     bool clockwise = true;
//-     //- https://www.reddit.com/r/gifs/comments/fx5dgf/this_600_led_infinity_mirror_coffee_table_i_built/
//-     //- https://github.com/FastLED/FastLED/blob/master/examples/Fire2012/Fire2012.ino
//-     static uint8_t heat[NUM_LEDS_FIRE];
//- 
//-     // Step 1.  Cool down every cell a little
//-     for (int i = 0; i < NUM_LEDS_FIRE; i++) {
//-         heat[i] = qsub8(heat[i], random8(0, ((FIRE_COOLING * 10) / NUM_LEDS_FIRE) + 2));
//-     }
//- 
//-     // Step 2.  Heat from each cell drifts 'up' and diffuses a little
//-     for (int k = NUM_LEDS_FIRE - 1; k >= 2; k--) {
//-         heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
//-     }
//- 
//-     // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
//-     if (random8() < FIRE_SPARKING) {
//-         int y = random8(7);
//-         heat[y] = qadd8(heat[y], random8(160, 255));
//-     }
//- 
//-     // Step 4.  Map from heat cells to LED colors
//-     for (int j = 0; j < NUM_LEDS_FIRE; j++) {
//-         CRGB color = HeatColor(heat[j]);
//-         int pixelnumber;
//-         if (clockwise) {
//-             pixelnumber = (NUM_LEDS_FIRE - 1) - j;
//-         }
//-         else {
//-             pixelnumber = j;
//-         }
//-         g_data.leds[0][pixelnumber] = color;
//-     }
//- }

//- Fire2018 taken from Stefan Petrick
//- https://gist.github.com/StefanPetrick/1ba4584e534ba99ca259c1103754e4c5#file-fire2018-ino-L41
static void Fire2018(FireLEDs_t* fire, CRGBPalette16 pal)
{
    static uint8_t heat[FIRE_X][FIRE_Y];

    uint8_t noise[FIRE_X][FIRE_Y];

    // get one noise value out of a moving noise space
    uint16_t ctrl1 = inoise16(11 * millis(), 0, 0);
    // get another one
    uint16_t ctrl2 = inoise16(13 * millis(), 100000, 100000);
    // average of both to get a more unpredictable curve
    uint16_t  ctrl = ((ctrl1 + ctrl2) / 2);

    // this factor defines the general speed of the heatmap movement
    // high value = high speed
    uint8_t speed = 27;

    // here we define the impact of the wind
    // high factor = a lot of movement to the sides
    uint32_t x = 3 * ctrl * speed;

    // this is the speed of the upstream itself
    // high factor = fast movement
    uint32_t y = 15 * millis() * speed;

    // just for ever changing patterns we move through z as well
    uint32_t z = 3 * millis() * speed;

    // ...and dynamically scale the complete heatmap for some changes in the
    // size of the heatspots.
    // The speed of change is influenced by the factors in the calculation of ctrl1 & 2 above.
    // The divisor sets the impact of the size-scaling.
    uint32_t scale_x = ctrl1 / 2;
    uint32_t scale_y = ctrl2 / 2;

    // Calculate the noise array based on the control parameters.
    for (uint8_t i = 0; i < FIRE_X; i++) {
        uint32_t ioffset = scale_x * (i - CENTREX);
        for (uint8_t j = 0; j < FIRE_Y; j++) {
            uint32_t joffset = scale_y * (j - CENTREY);
            uint16_t data = ((inoise16(x + ioffset, y + joffset, z)) + 1);
            noise[i][j] = data >> 8;
        }
    }


    // Draw the first (lowest) line - seed the fire.
    // It could be random pixels or anything else as well.
    for (uint8_t x = 0; x < FIRE_X; x++) {
        // draw
        fire->leds[x][0] = ColorFromPalette(pal, noise[x][0]);
        // and fill the lowest line of the heatmap, too
        heat[x][FIRE_Y - 1] = noise[x][0];
    }

    // Copy the heatmap one line up for the scrolling.
    for (uint8_t y = 0; y < FIRE_Y - 1; y++) {
        for (uint8_t x = 0; x < FIRE_X; x++) {
            heat[x][y] = heat[x][y + 1];
        }
    }

    // Scale the heatmap values down based on the independent scrolling noise array.
    for (uint8_t y = 0; y < FIRE_Y - 1; y++) {
        for (uint8_t x = 0; x < FIRE_X; x++) {

            // get data from the calculated noise field
            uint8_t dim = noise[x][y];

            // This number is critical
            // If it´s to low (like 1.1) the fire dosn´t go up far enough.
            // If it´s to high (like 3) the fire goes up too high.
            // It depends on the framerate which number is best.
            // If the number is not right you loose the uplifting fire clouds
            // which seperate themself while rising up.
            dim = dim / 1.4;

            dim = 255 - dim;

            // here happens the scaling of the heatmap
            heat[x][y] = scale8(heat[x][y], dim);
        }
    }

    // Now just map the colors based on the heatmap.
    for (uint8_t y = 0; y < FIRE_Y - 1; y++) {
        for (uint8_t x = 0; x < FIRE_X; x++) {
            fire->leds[x][(FIRE_Y -1) - y] = ColorFromPalette(pal, heat[x][y]);
        }
    }

}

static bool ModeFireUpdate(void)
{
    bool needs_redraw = false;
    if (IsTimeToAnimate())
    {
        static FireLEDs_t fire;
        //- Just call the main function from the internet
        Fire2018(&fire, g_data.firePallet);
        //- Apply to all walls
        for (uint32_t wall_idx = 0; wall_idx < NUM_WALLS; wall_idx++)
        {
            Wall_t* wall = &g_data.walls[wall_idx];
            //- Go both forwards and backwards from this corner
            for (uint32_t led_idx = 0; led_idx < FIRE_Y; led_idx++)
            {
                uint32_t wall_idx_forward = IncLEDIdx(wall->firstIdx, led_idx);
                uint32_t wall_idx_backward = DecLEDIdx(wall->firstIdx, led_idx);
                for (uint32_t strip_idx = 0; strip_idx < NUM_STRIPS; strip_idx++)
                {
                    g_data.leds[strip_idx][wall_idx_forward]  = fire.leds[strip_idx][led_idx];
                    g_data.leds[strip_idx][wall_idx_backward] = fire.leds[strip_idx][led_idx];
                }
            }
        }
        needs_redraw = true;
    }
    return needs_redraw;
}

static bool ModeFireInit(void)
{
    //- Just empty the table
    FillTableSolid(CRGB::Black);
    return true;
}

//------------------------------------------------------------------------------------
//- Microphone Mode ------------------------------------------------------------------
//------------------------------------------------------------------------------------

//- Processes a new spectrum from the microphone
static void ProcessNewSpectrum(SpectrumHistory_t* history, Microphone_Spectrum_t* newSpectrum)
{
    uint32_t time_now;
    uint32_t i;
    uint32_t j;

    bool     max_found = false;

    time_now = millis();

    //- Copy this spectrum to history, update pointers
    history->spectrums[history->head] = (*newSpectrum);
    history->times[history->head] = time_now;
    history->head = (history->head + 1) % SPECTRUM_HISTORY_LEN;

    //- Process all bins:
    //- For each item in history, within the specified time from, find max
    //- COmpute dropping max
    for (i = 0; i < MICROPHONE_F_BINS; i++)
    {
        history->frequencyMaxs[i] = 0U;
        for (j = 0; j < SPECTRUM_HISTORY_LEN; j++)
        {
            //- Check receive time
            if ((time_now - history->times[j]) < SPECTRUM_MAX_TIME_MS)
            {
                //- Check this bin, this spectrum
                if (history->spectrums[j].spectrum[i] > history->frequencyMaxs[i])
                {
                    history->frequencyMaxs[i] = history->spectrums[j].spectrum[i];
                }
            }
        }
        //- If this max is higher than the dropping max, update the dropping max
        if (history->frequencyMaxs[i] > history->droppingMax)
        {
            history->droppingMax = history->frequencyMaxs[i];
            max_found = true;
        }
    }
    //- If we didn't find a new max, drop the dropping max
    //- This means that if we go from loud to quiet, the graph still looks cool visually
    if (!max_found)
    {
        history->droppingMax = history->droppingMax > MICROPHONE_DROPPING_MAX_MIN ? history->droppingMax - 1U : MICROPHONE_DROPPING_MAX_MIN;
    }

    //- Log_Println(String(history->droppingMax) + "-" + String(newSpectrum->spectrum[0]));
}

//- Writes an equilizer bin color, depending on mode and config
static void WriteEqColor(uint32_t stripIdx, uint32_t microphoneBin, CRGB color, bool fullTable)
{
    //- Full table splits the EQ up among all bins
    if (fullTable)
    {
        //- Full table. Calculate how many columsn to do based on num leds
        uint32_t cols_per_freq = (uint32_t)((float)NUM_LEDS_PER_STRIP / (float)MICROPHONE_F_BINS);
        for (uint32_t i = 0; i < cols_per_freq; i++)
        {
            uint32_t idx = (microphoneBin * cols_per_freq) + i;
            g_data.leds[stripIdx][idx] = color;
        }
    }
    else
    {
        //- Draw on the two longest walls. Eventually down-sample to the shorter walls too.
        uint32_t wall_0_idx = g_data.walls[0].firstIdx + microphoneBin;
        uint32_t wall_2_idx = g_data.walls[2].firstIdx + microphoneBin;
        g_data.leds[stripIdx][wall_0_idx] = color;
        g_data.leds[stripIdx][wall_2_idx] = color;
    }
}

//- Update microphone equilizer mode
static bool EqUpdate(bool inverseBackground, bool rainbow, bool fullTable)
{
    bool needs_redraw = false;
    //- Check for microphone spectrum, returns true if we get a new one
    if (Microphone_GetNewSpectrum(&g_data.spectrum, MICROPHONE_TIMEOUT))
    {
        SpectrumHistory_t* history;
        uint32_t i;
        CRGB bar_color;
        CRGB background_color;

        g_data.scrollCursor++;

        //- Do initial processing of this spectrum
        history = &g_data.spectrumHistory;
        ProcessNewSpectrum(history, &g_data.spectrum);

        //- Get the color to use
        bar_color        = rainbow ? GetRainbowColor(g_data.scrollCursor % NUM_LEDS_PER_STRIP) : g_data.solidColor;
        background_color = inverseBackground ? -bar_color : CRGB::Black;
        //- Update back of table
        FillTableSolid(background_color);
        
        //- Iterate each bin, drawing them
        for (i = 0; i < MICROPHONE_F_BINS; i++)
        {
            //- TODO: Perhaps if I just fill a float array of NUM_LEDS_PER_STRIP that will be easier to do LED math on
            //- Figure out what percentage of the dropping max this is, and then apply that upward
            float num_leds_up = (((float)history->frequencyMaxs[i] / (float)history->droppingMax) * (float)NUM_STRIPS);
            //- Clamp to in bounds
            num_leds_up = constrain(num_leds_up, 0.0f, (float)NUM_STRIPS);
            //- Strip 0 is the top, we want to draw from the bottom up
            uint8_t strip_idx = (NUM_STRIPS - 1);
            while(num_leds_up > 0.0f)
            {
                //- Interpolate this color.... May get rid of this it doesn't do much
                CRGB color = bar_color.lerp8(background_color, FloatLerpToFastLED(num_leds_up));
                WriteEqColor(strip_idx, i, color, fullTable);
                strip_idx   -= 1;
                num_leds_up -= 1.0f;
            }
        }
        needs_redraw = true;
    }
    else
    {
        //- If we haven't gotten a microphone profile just draw timeout to alert the user
        FillLEDsWithText(0U, true, "Timeout");
        needs_redraw = true;
    }
    return needs_redraw;
}

static bool ModeMicrophoneEqInverseUpdate(void)
{
    return EqUpdate(true, false, false);
}

static bool ModeMicrophoneEqSolidUpdate(void)
{
    return EqUpdate(false, false, false);
}

static bool ModeMicrophoneEqRainbowUpdate(void)
{
    return EqUpdate(false, true, false);
}

static bool ModeMicrophoneEqFullUpdate(void)
{
    return EqUpdate(true, false, true);
}

//- Computes an alpha filter (helps slow down noisy signals)
static float AlphaFilter(float oldVal, float newVal, float alpha)
{
    return (oldVal + (newVal * alpha));
}

//- This mode is hard, needs to be re-worked.
//- It attempts to identify loud/unique noises in the sound profile, but doesn't work too well.
//- It returns a number that basically quanitifies activity in the room.
static float GetAmbientModeAlpha(Microphone_Spectrum_t* spectrum)
{
    static uint16_t last_loudness = 0U;
    static uint16_t max_delta = 0;
    static float alpha = 0.0;
    float loudness_delta_pct_of_max;
    uint16_t loudness = 0;
    int32_t loudness_delta;
    //- Sum up all of the bins
    for (uint32_t i = 0; i < MICROPHONE_F_BINS; i++)
    {
        loudness += g_data.spectrum.spectrum[i];
    }
    
    if (loudness < 40)
    {
        //- If below some threshold it's quiet. Decrease the alpha depending on where it's at.
        alpha = AlphaFilter(alpha, alpha > 0.25f ? -0.004f  : -0.001f, 1.0f);
    }
    else
    {
        //- It's "loud"... try to get some metric for loudness
        //- Compare it to the last time we were in this function
        loudness_delta = (int32_t)(loudness + 6) - (int32_t)last_loudness;
        last_loudness = loudness;
        //- Check for new max delta (another dropping average)
        if (abs(loudness_delta) > max_delta)
        {
            max_delta = abs(loudness_delta);
        }
        else if (max_delta > MICROPHONE_DROPPING_MAX_MIN)
        {
            //-  Drop max delta if it wasn't set and it's not above some min value
            max_delta--;
        }
        //- Now figure out how much this changed as a percentage of the activity in the room
        loudness_delta_pct_of_max = ((float)loudness_delta / (float)max_delta);
        //- Valid values are 0 to 1.1 for alpha, so map to that
        //- Basically the lerp is mapped to the LED wall
        //- So with that in mind, the LEDs are slowly turning on from 0 to 1.0.
        //- At 1.0 all LEDs are on, outter LEDs are min. So the edges get brighter from 1.0 to 1.1
        loudness_delta_pct_of_max *= 1.1f;
        //- TODO: Try line of best fit: https://www.varsitytutors.com/hotmath/hotmath_help/topics/line-of-best-fit
        //- Apply this value
        alpha = AlphaFilter(alpha, loudness_delta_pct_of_max, alpha < 0.1f ? 0.01: 0.0075f);
        //- If alpha was increasing, then make sure it's a minimum display value.
        alpha = (loudness_delta_pct_of_max > 0.0) && (alpha < MIN_LERP_T) ? MIN_LERP_T : alpha;
    }
    alpha = constrain(alpha, 0.0f, 1.1f);
    //- Log_Println(String(loudness) + "," + String(alpha));
    return alpha;
}

static bool ModeMicrophoneAmbientUpdate(void)
{
    bool needs_redraw = false;
    //- Check for microphone spectrum, returns true if we get a new one
    if (Microphone_GetNewSpectrum(&g_data.spectrum, MICROPHONE_TIMEOUT))
    {
        float alpha;
        //- Process this spectrum
        alpha = GetAmbientModeAlpha(&g_data.spectrum);
        //- Draw the spectrum on each wall individually
        for (uint32_t wall_idx = 0; wall_idx < NUM_WALLS; wall_idx++)
        {
            Wall_t* wall = &g_data.walls[wall_idx];
            uint32_t middle = wall->len / 2U;
            //- Iterate the wall
            for (uint32_t led_idx = 0; led_idx <= wall->len; led_idx++)
            {
                float lerp_t;
                //- Produce the lerp_t. We are going from 0 to 1.1 and then clamping at 1.0 because lerp is only valid between 0.0 and 1.0
                //- We start with the percent of the distance from middle to wall
                if (led_idx <= middle)
                {
                    lerp_t = ((float)led_idx / (float)middle);
                }
                else
                {
                    lerp_t = ((float)(wall->len - led_idx) / (float)middle);
                }
                //- Apply alpha
                lerp_t *= alpha;
                //- Get fastLED version of the lerp
                fract8 lerp_fastled = FloatLerpToFastLED(lerp_t);
                for (uint32_t strip_idx = 0; strip_idx < NUM_STRIPS; strip_idx++)
                {
                    //- Fill data
                    g_data.leds[strip_idx][wall->firstIdx + led_idx] = g_data.solidColor.lerp8(CRGB::Black, lerp_fastled);
                }
            }
        }
        needs_redraw = true;
    }
    return needs_redraw;
}

static bool ModeMicrophoneInit(void)
{
    //- Tell the user what's happening
    FillLEDsWithText(0U, true, "Initializing...");
    g_data.scrollCursor = 0;
    return true;
}

//------------------------------------------------------------------------------------
//- Off Mode -------------------------------------------------------------------------
//------------------------------------------------------------------------------------

static bool ModeOffInit(void)
{
    //- Off, just black LEDs. No update.
    FillTableSolid(CRGB::Black);
    return true;
}

//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------
//- Mode transitioning ---------------------------------------------------------------
//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------

//- Used to do a smooth transition between modes
static bool TransitionModes(LEDControl_Mode_t fromMode, LEDControl_Mode_t toMode)
{
    Log_Println("Transitioning modes");
    //- Fade out the old mode
    for (int32_t i = g_config.maxBrightness; i > 0; i-=10)
    {
        if (modeData[fromMode].modeUpdateFunc != NULL)
        {
            (void)modeData[fromMode].modeUpdateFunc();
        }
        FastLED.setBrightness(i);
        FastLED.show();
    }
    //- Initialize new mode
    (void)modeData[toMode].modeInitFunc();
    //- Fade in new mode
    for (int32_t i = 0; i < g_config.maxBrightness; i+=10)
    {
        if (modeData[toMode].modeUpdateFunc != NULL)
        {
            (void)modeData[toMode].modeUpdateFunc();
        }
        FastLED.setBrightness(i);
        FastLED.show();
    }
    //- Set to appropriate brightness
    FastLED.setBrightness(g_config.maxBrightness);
    Log_Println("Mode transition done");
    return true;
}

//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------
//- Public Functions -----------------------------------------------------------------
//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------
//- Setup Function -------------------------------------------------------------------
//------------------------------------------------------------------------------------

void LEDControl_Setup(void) 
{
    int32_t i;
    g_data.animateFrameNumber = 0U;
    //- Have to give fastLED constant pin number
    FastLED.addLeds<NEOPIXEL, 32>(g_RAWLEDS[0], TOTAL_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<NEOPIXEL, 33>(g_RAWLEDS[1], TOTAL_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<NEOPIXEL, 25>(g_RAWLEDS[2], TOTAL_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<NEOPIXEL, 26>(g_RAWLEDS[3], TOTAL_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<NEOPIXEL, 27>(g_RAWLEDS[4], TOTAL_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
    //- Initialize LED array pointers
    for (i = 0; i < NUM_STRIPS; i++)
    {
        g_data.leds[i] = g_RAWLEDS[i] + FIRST_LED_OFFSET;
    }
    //- Show each row as white initially
    //- Helps debug connection issues
    Log_Println("Displaying rows.");
    FastLED.setBrightness(255);
    for (i = 0; i < NUM_STRIPS; i++)
    {
        fill_solid(g_data.leds[i], NUM_LEDS_PER_STRIP, CRGB::White);
        FastLED.show();
        delay(500U);
        //- Clear before moving on
        fill_solid(g_data.leds[i], NUM_LEDS_PER_STRIP, CRGB::Black);
    }
    //- Setup walls
    Log_Println("Setting up walls.");
    g_data.walls[0].firstIdx = 0;
    g_data.walls[0].len      = 62;
    g_data.walls[1].firstIdx = g_data.walls[0].firstIdx + g_data.walls[0].len + 1;
    g_data.walls[1].len      = 35;
    g_data.walls[2].firstIdx = g_data.walls[1].firstIdx + g_data.walls[1].len + 1;
    g_data.walls[2].len      = 61;
    g_data.walls[3].firstIdx = g_data.walls[2].firstIdx + g_data.walls[2].len + 1;
    g_data.walls[3].len      = NUM_LEDS_PER_STRIP - (g_data.walls[2].firstIdx + g_data.walls[2].len);
    //- Show walls initial position for debugging
    g_data.leds[0][g_data.walls[0].firstIdx] = CRGB::Red;
    g_data.leds[0][g_data.walls[1].firstIdx] = CRGB::Blue;
    g_data.leds[0][g_data.walls[2].firstIdx] = CRGB::Green;
    g_data.leds[0][g_data.walls[3].firstIdx] = CRGB::White;
    //- Copy data down
    CopyFirstStripToOthers();
    FastLED.show();
    delay(1000U);
    //- Figure out runtime config
    //- Similar to Porcupine's code.
    //- https://pastebin.com/H2pU6Ecg
    //- Basically the green section is huge normally and red is tiny, so trying to get rainbow balanced
    g_data.BRsize = (int)((NUM_LEDS_PER_STRIP / 3) * 1.2f); //section between 100% blue and 100% red
    g_data.RGsize = (int)((NUM_LEDS_PER_STRIP / 3) * 1.2f); //section between 100% red and 100% green
    g_data.GBsize = (int)((NUM_LEDS_PER_STRIP / 3) * 0.6f); //section between 100% green and 100% blue
    int biasSum = g_data.BRsize + g_data.RGsize + g_data.GBsize;
    //- Not sure what the math above will result in, so update sizes until the sum is correct
    while (biasSum < NUM_LEDS_PER_STRIP)
    {
        g_data.BRsize = (biasSum % 3) == 0 ? g_data.BRsize + 1 : g_data.BRsize;
        g_data.RGsize = (biasSum % 3) == 1 ? g_data.RGsize + 1 : g_data.RGsize;
        g_data.GBsize = (biasSum % 3) == 2 ? g_data.GBsize + 1 : g_data.GBsize;
        biasSum++;
    }
    //- Display IP address so we know where to connect.
    //- In future may add a button for this.
    String ip_addr;
    ip_addr = WifiControl_GetIPAddress();
    Log_Println("Displaying IP address:" + ip_addr);
    strcpy(g_config.displayText, ip_addr.c_str());
    i = 0;
    UpdateSolidColorCRGB(GetRainbowColor(i));
    FillLEDsWithText(g_data.walls[0].firstIdx);
    FillLEDsWithText(g_data.walls[2].firstIdx, false);
    unsigned long start_t = millis();
    //- For a few seconds, rotate through rainbow colors :)
    while ((millis() - start_t) < 4000)
    {
        i = IncLEDIdx(i);
        UpdateSolidColorCRGB(GetRainbowColor(i));
        CheckSolidColor();
        FastLED.show();
    }
    //- Initialize text
    strcpy(g_config.displayText, "");

    //- Go into normal operation
    Log_Println("Starting normal operation.");
    g_config.maxBrightness = 125;
    g_config.scrollSpeed = 50;
    //- Force mode change, initialize ambient microphone mode
    //- This is good because it means if the table resets somehow while nobody is home, nominally the LEDs will be OFF.
    //- And ambient mode is supposed to be standby anyways.
    g_data.curMode = LED_MODE_OFF;
    g_config.mode = LED_MODE_MICRPHONE_AMB;
    //- Initialize color randomly
    UpdateSolidColor(random8(), random8(), random8());
    FastLED.setBrightness(g_config.maxBrightness);

    //- Fire setup
    g_data.firePallet = HeatColors_p;
}

//------------------------------------------------------------------------------------
//- Periodic Update Function ---------------------------------------------------------
//------------------------------------------------------------------------------------

void LEDControl_Update(void)
{
    bool call_show = false;

    //- Check for mode change
    if (g_data.curMode != g_config.mode)
    {
        //- Check for mode validity
        if (g_config.mode >= 0 && g_config.mode < LED_MODE_COUNT)
        {
            //- Change
            call_show |= TransitionModes(g_data.curMode, g_config.mode);
            g_data.curMode = g_config.mode;
        }
        else
        {
            //- Set to text so next loop it switches to text
            g_config.mode = LED_MODE_TEXT;
            (void)strcpy(g_config.displayText, "INVALID MODE");
        }
    }
    else
    {
        //- No mode change, just update current mode
        if (modeData[g_data.curMode].modeUpdateFunc != NULL)
        {
            call_show |= modeData[g_data.curMode].modeUpdateFunc();
        }
    }

    //- Only draw when needed
    if (call_show)
    {
        FastLED.show();
    }
    else
    {
        FastLED.delay(0);
    }
}

//------------------------------------------------------------------------------------
//- Command/Status Function ----------------------------------------------------------
//------------------------------------------------------------------------------------

void LEDControl_SetMaxBrightness(int32_t maxBrightness)
{
    g_config.maxBrightness = maxBrightness;
    FastLED.setBrightness(g_config.maxBrightness);
}

void LEDControl_SetScrollSpeed(int32_t scrollSpeed)
{
    g_config.scrollSpeed = scrollSpeed;
}

void LEDControl_SetMode(LEDControl_Mode_t mode)
{
    //- Only set valid modes
    if (mode >= 0 && mode < LED_MODE_COUNT)
    {
        g_config.mode = mode;
    }
}

void LEDControl_SetRed(uint8_t red)
{
    UpdateSolidColor(red, g_config.green, g_config.blue);
}

void LEDControl_SetGreen(uint8_t green)
{
    UpdateSolidColor(g_config.red, green, g_config.blue);
}

void LEDControl_SetBlue(uint8_t blue)
{
    UpdateSolidColor(g_config.red, g_config.green, blue);
}

void LEDControl_SetDisplayText(const char* text)
{
    size_t len;
    uint16_t i = 0;
    //- Loop string or until displayText is full
    while ((text[i] != '/0') && (i < DISPLAY_TEXT_MAX_LEN - 2U)) //- Minus 2 for null char
    {
        g_config.displayText[i] = text[i];
        i++;
    }
    //- Put null on the end
    g_config.displayText[i] = 0;
    g_data.displayTextDirty = true;
}

const LEDControl_Config_t * LEDControl_GetConfig(void)
{
    //- Doing a pointer here because the text is large
    //- Anybody getting this pointer should NOT assign to it
    return &g_config;
}

void LEDControl_SetFastLEDRandomSeed(void)
{
    unsigned long start_t;
    bool got_microphone = false;
    uint32_t total_mic_sum = 0;
    uint32_t count = 0;
    start_t = millis();
    //- For 1500ms, get microphone spectrums and set random seed based on sum of all of the bins in that time
    while (millis() - start_t < 1500)
    {
        if (Microphone_GetNewSpectrum(&g_data.spectrum, MICROPHONE_TIMEOUT))
        {
            uint16_t sum = 0;
            for (uint32_t i = 0; i < MICROPHONE_F_BINS; i++)
            {
                sum += g_data.spectrum.spectrum[i];
            }
            count++;
            total_mic_sum += (count * sum);
            got_microphone = true;
        }
    }
    //- Set the seed
    if (got_microphone)
    {
        Log_Println("Seeded random with " + String(count) + " samples summing to: " + String(total_mic_sum));
        random16_set_seed(total_mic_sum);
    }
    else
    {
        random16_set_seed(analogRead(34)); //- Pin 34 is input ony ADC pin https://randomnerdtutorials.com/esp32-pinout-reference-gpios/
        Log_Println("Seeded random with pin 34");
    }
}
