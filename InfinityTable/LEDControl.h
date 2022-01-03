
#include <stdint.h>

//- Buffer len for text
#define DISPLAY_TEXT_MAX_LEN (512)

//- Various modes as an enum for easy programming
typedef enum LEDControl_Mode_e
{
    LED_MODE_RAINBOW = 0,
    LED_MODE_TEXT,
    LED_MODE_SOLID,
    LED_MODE_PARALLELOGRAM,
    LED_MODE_FLASHY_RAINBOW,
    LED_MODE_FLASHY_COLOR,
    LED_MODE_FIRE,
    LED_MODE_MICRPHONE_EQ_INVERSE,
    LED_MODE_MICRPHONE_EQ_SOLID,
    LED_MODE_MICRPHONE_EQ_RAINBOW,
    LED_MODE_MICRPHONE_EQ_FULL,
    LED_MODE_MICRPHONE_AMB,
    LED_MODE_OFF,
    LED_MODE_COUNT
}LEDControl_Mode_t;

//- Config - basically status to be read by other modules
typedef struct LEDControl_Config_s
{
    uint8_t             maxBrightness;
    int32_t             scrollSpeed;
    LEDControl_Mode_t   mode;
    uint8_t             red;
    uint8_t             green;
    uint8_t             blue;
    char                displayText[DISPLAY_TEXT_MAX_LEN];
}LEDControl_Config_t;

//- Public functions

//- Init/drawing
void LEDControl_Setup(void);
void LEDControl_Update(void);

//- Command
void LEDControl_SetMaxBrightness(int32_t maxBrightness);
void LEDControl_SetScrollSpeed(int32_t scrollSpeed);
void LEDControl_SetMode(LEDControl_Mode_t mode);
void LEDControl_SetDisplayText(const char* text);
void LEDControl_SetRed(uint8_t red);
void LEDControl_SetGreen(uint8_t green);
void LEDControl_SetBlue(uint8_t blue);

void LEDControl_GetFPS(float* loopFPS, float* drawFPS);
//- Status
const LEDControl_Config_t * LEDControl_GetConfig(void);

//- Other
void LEDControl_SetFastLEDRandomSeed(void);