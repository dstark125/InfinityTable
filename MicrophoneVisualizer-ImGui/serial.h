
#include <stdint.h>

#define NUM_SAMPLES (64)
#define SAMPLE_HISTORY (200)
typedef struct ArduinoSamples_s
{
    int         head;
    float       samples[NUM_SAMPLES][SAMPLE_HISTORY];
    uint8_t     lastSample[NUM_SAMPLES];
    double      timeReceived[SAMPLE_HISTORY];
    uint32_t    sampleSeq;
} ArduinoSamples_t;

void StartSerialThread(void);

void Serial_GetSamples(ArduinoSamples_t* samples);

void Serial_Enable(bool enabled);
