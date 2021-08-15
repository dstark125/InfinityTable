/*
 Name:		InfinityTable.ino
 Created:	2/15/2021 8:54:25 PM
 Author:	David
*/

#include "LEDControl.h"
#include "WifiControl.h"
#include "SoftwareUpdate.h"
#include "Microphone.h"
#include "Log.h"


void setup() {
    Serial.begin(115200);
    //- Initialize all modules that require initialization.
    Microphone_Init();
    //- To get better randomness, seed fastLED with the microphone (if available)
    LEDControl_SetFastLEDRandomSeed();
    WifiControl_Setup();
    LEDControl_Setup();
    Log_Println("Setup() complete.");
}

void loop() 
{
    for (;;)
    {
        //- Infinitely update all modules.
        //- Note I don't like that loop() is a function called over and over, so I just make my own
        LEDControl_Update();
        WifiControl_Update();
        SoftwareUpdate_Update();
    }
}

