
#include "config.h"
#include "stdint.h"

//- This is what we get from the arduino - array of bins from 0 to 256 amplitude
typedef struct Microphone_Spectrum_s
{
    uint8_t spectrum[MICROPHONE_F_BINS];
}Microphone_Spectrum_t;

void Microphone_Init(void);

bool Microphone_GetNewSpectrum(Microphone_Spectrum_t* spectrum, uint32_t timeout);

void Microphone_NoiseCommandReceived(void);
