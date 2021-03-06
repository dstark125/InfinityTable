/*
* Taken largely from the internet. A mix of
* https://github.com/adafruit/Adafruit_Learning_System_Guides/blob/master/Tiny_Music_Visualizer/Piccolo/Piccolo.ino
* and
* https://create.arduino.cc/projecthub/shajeeb/32-band-audio-spectrum-visualizer-analyzer-902f51
*/

#include <avr/pgmspace.h>
#include "ffft.h"
#include <math.h>

// Microphone connects to Analog Pin 0.  Corresponding ADC channel number
// varies among boards...it's ADC0 on Uno and Mega, ADC7 on Leonardo.
// Other boards may require different settings; refer to datasheet.
#define ADC_CHANNEL 0

//- FFT_N is defined in fft.h
#define SAMPLES (FFT_N / 2)

int16_t       capture[FFT_N];    // Audio capture buffer
complex_t     bfly_buff[FFT_N];  // FFT "butterfly" buffer
uint16_t      spectrum[SAMPLES]; // Spectrum output buffer
uint8_t       output[SAMPLES];   // FFT requires uin16 for spectrum, but I haven't seen values over 200
volatile byte samplePos = 0;     // Buffer position counter
bool          filter_output = true;

// This is low-level noise that's subtracted from each FFT output column.
// Generated by the MicrophoneVisualizer IMGUI tool
//- Can be overriden at runtime... I suggest doing this.
//- static uint8_t noise[SAMPLES] = { 0.0f };
static uint8_t noise[SAMPLES] = { 128, 47, 13, 10, 8, 10, 10, 9, 6, 7, 7, 7, 6, 7, 7, 7, 5, 6, 5, 6, 6, 5, 6, 5, 5, 5, 5, 5, 5, 5, 6, 7, 5, 4, 3, 4, 4, 4, 4, 5, 4, 4, 5, 4, 4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 6 };

// These are scaling quotients for each FFT output column, sort of a
// graphic EQ in reverse.  Most music is pretty heavy at the bass end.
// Generated by the MicrophoneVisualizer IMGUI tool
//static const float eq[SAMPLES] = { 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f };
static const float eq[SAMPLES] = { 1.000f, 0.478f, 0.478f, 0.521f, 0.478f, 0.478f, 0.484f, 1.016f, 0.645f, 0.490f, 0.478f, 0.666f, 0.580f, 0.701f, 0.903f, 0.991f, 0.677f, 0.689f, 1.161f, 0.557f, 0.580f, 0.797f, 1.270f, 0.797f, 0.991f, 0.924f, 1.098f, 1.161f, 0.865f, 0.945f, 2.000f, 2.000f, 1.451f, 2.000f, 2.000f, 1.767f, 2.000f, 2.000f, 1.016f, 1.069f, 1.693f, 1.354f, 0.752f, 0.549f, 1.129f, 2.000f, 1.847f, 2.000f, 2.000f, 2.000f, 2.000f, 2.000f, 2.000f, 2.000f, 2.000f, 2.000f, 2.000f, 1.935f, 1.625f, 1.935f, 2.000f, 2.000f, 2.000f, 1.505f };

#define HEADER (0xAC1D)
#define TRAILER (0xCAFE)
//- Send the spectrum out via serial
static void SendSerialMessage(uint8_t* buffer, size_t bufferLen)
{
    uint16_t temp;
    //- Write header->data->trailer
    temp = HEADER;
    Serial.write((uint8_t*)&temp, sizeof(uint16_t));
    Serial.write(buffer, bufferLen);
    temp = TRAILER;
    Serial.write((uint8_t*)&temp, sizeof(uint16_t));
}

//- Waits for a new spectrum to be ready, and computes
static void GetNewSpectrum(void)
{
    while (ADCSRA & _BV(ADIE)); // Wait for audio sampling to finish

    fft_input(capture, bfly_buff);   // Samples -> complex #s
    samplePos = 0;                   // Reset sample counter
    ADCSRA |= _BV(ADIE);             // Resume sampling interrupt
    fft_execute(bfly_buff);          // Process complex data
    fft_output(bfly_buff, spectrum); // Complex -> spectrum
}

//- Programatically figure out the noise profile
static void GenerateNoiseProfile(void)
{
    uint32_t i;
    //- Has to be static can't create on stack
    static uint8_t freq_maxes[SAMPLES] = { 0 };
    //- We can't use millis. BUT we read from the microphone at about 70hz, so
    //- take that into account when looping
    digitalWrite(13, HIGH);
    for (uint32_t num_checks = 0; num_checks < 150; num_checks++)
    {
        GetNewSpectrum();
        for (i = 0; i < SAMPLES; i++)
        {
            uint8_t this_sample = constrain(spectrum[i], 0U, UINT8_MAX);
            freq_maxes[i] = (freq_maxes[i] < spectrum[i]) ? spectrum[i] : freq_maxes[i];
        }
    }
    for (i = 0; i < SAMPLES; i++)
    {
        noise[i] = freq_maxes[i];
    }
    digitalWrite(13, LOW);
}

//- Commandable modes
typedef enum Mode_e
{
    MODE_FREERUN = 0,
    MODE_REQUEST,
    MODE_IDLE,
}Mode_t;

static void HandleNewSpectrum(void)
{
    //- Start up in request mode (must send RQ to get data)
    static Mode_t mode = MODE_REQUEST;
    bool request = false;
    //- Don't block on serial, just check
    if (Serial.available())
    {
        char command[16] = { 0 };
        bool done = false;
        int idx = 0;
        //- Serial data available, get it
        while (!done)
        {
            //- Get the latest byte
            int this_byte = Serial.read();
            if (this_byte != -1)
            {
                command[idx++] = (char)this_byte;
                //- If we are out of buffer room just bail
                //- Note I'm doing strstr so I don't care about location, just if we recieved the commands
                if (idx == 16)
                {
                    done = true;
                }
                else if (strstr(command, "FREE") != NULL) //- Freerun (always send data out)
                {
                    mode = MODE_FREERUN;
                    done = true;
                }
                else if (strstr(command, "IDLE") != NULL) //- Idle (don't do anything)
                {
                    mode = MODE_IDLE;
                    done = true;
                }
                else if (strstr(command, "REQUEST") != NULL) //- Request (only send data when get RQ)
                {
                    mode = MODE_REQUEST;
                    done = true;
                }
                else if (strstr(command, "RQ") != NULL) //- RQ - send data out
                {
                    request = true;
                    done = true;
                }
                else if (strstr(command, "FILTER") != NULL) //- Toggle filtering enable/disable
                {
                    filter_output = !filter_output;
                    done = true;
                }
                else if (strstr(command, "NOISE") != NULL) //- Compute the noise floor
                {
                    GenerateNoiseProfile();
                    done = true;
                }
            }
        }
    }

    //- Run each mode
    switch (mode)
    {
    case MODE_FREERUN:
        //- Send data as it's available
        //Serial.println("Data freerun");
        SendSerialMessage((uint8_t*)output, sizeof(output));
        break;
    case MODE_REQUEST:
        //- Only send if we got an RQ
        if (request)
        {
            //Serial.println("Data request");
            SendSerialMessage((uint8_t*)output, sizeof(output));
            digitalWrite(13, !digitalRead(13));
        }
        else
        {
            digitalWrite(13, LOW);
        }
        break;
    case MODE_IDLE:
        //- Don't send data because we are idle
        break;
    }
}

void setup() 
{
    Serial.begin(115200);

    //- LED pin
    pinMode(13, OUTPUT);

    // Init ADC free - run mode; f = (16MHz / prescaler) / 13 cycles / conversion
    ADMUX = ADC_CHANNEL; // Channel sel, right-adj, use AREF pin
    ADCSRA = _BV(ADEN) | // ADC enable
        _BV(ADSC) | // ADC start
        _BV(ADATE) | // Auto trigger
        _BV(ADIE) | // Interrupt enable
        _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); // 128:1 / 13 = 9615 Hz
    ADCSRB = 0;                // Free run mode, no high MUX bit
    DIDR0 = 1 << ADC_CHANNEL; // Turn off digital input for ADC pin
    TIMSK0 = 0;                // Timer0 off

    sei(); // Enable interrupts

    //- On startup, get the noise profile
    GenerateNoiseProfile();
}

void loop() 
{
    //- Block until we have new data
    GetNewSpectrum();
    
    // Remove noise and apply EQ levels
    for (uint8_t x = 0; x < FFT_N / 2; x++)
    {
        //- Apply filter if filtering
        if (filter_output)
        {
            uint16_t val = 0U;
            //- Check noise level
            if (spectrum[x] > noise[x])
            {
                //- This frequency if above the noise floor
                //- Subtract the noise floor and the multiply by the equilizer value
                val = (spectrum[x] - noise[x]) * eq[x];
            }
            //- Clamp to UINT8_MAX
            output[x] = val > UINT8_MAX ? UINT8_MAX : val;
        }
        else
        {
            //- No filtering, copy over data
            output[x] = spectrum[x];
        }
    }

    //- Got new spectrum, do other work we need to do
    HandleNewSpectrum();
}

ISR(ADC_vect){ 
    // Audio-sampling interrupt
    static const int16_t noiseThreshold = 4;
    int16_t              sample = ADC; // 0-1023

    capture[samplePos] =
        ((sample > (512 - noiseThreshold)) &&
            (sample < (512 + noiseThreshold))) ? 0 :
        sample - 512; // Sign-convert for FFT; -512 to +511

    if (++samplePos >= FFT_N) ADCSRA &= ~_BV(ADIE); // Buffer full, interrupt off
}