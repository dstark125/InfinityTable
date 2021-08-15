
#include "config.h"
#include "Log.h"
#include "Microphone.h"
#include "HardwareSerial.h"

//- Using hardwareserial to talk to another arduino
//- That arduino processes the microphone input, which is highly interrupt based
//- So basically it reads the mic, does processing, and sends the spectrum out serial as binary
HardwareSerial MicSerial(1);

//- For reference
//- #define HEADER (0xAC1D)
//- #define TRAILER (0xCAFE)
//- #define BUFFER_SIZE (1024)

//- Process a received byte, insert it into the state machine.
//- If we received the expected bytes and gotten the end message, we have a valid message
static bool ProcessByte(uint8_t* message, size_t messageLen, uint8_t this_byte)
{
    static uint32_t state = 0;
    static uint32_t state_2_count = 0;
    bool            ret = false;

    //- State machine
    switch (state)
    {
    case 0:
        //- Start code first byte
        if (this_byte == 0x1D || this_byte == 0xAC)
        {
            state++;
        }
        else
        {
            state = 0;
        }
        break;
    case 1:
        //- Start code second byte
        if (this_byte == 0x1D || this_byte == 0xAC)
        {
            state++;
        }
        else
        {
            state = 0;
        }
        state_2_count = 0;
        break;
    case 2:
        //- Got start code, insert bytes into buffer
        message[state_2_count] = this_byte;
        state_2_count++;
        if (state_2_count >= messageLen)
        {
            state_2_count = 0;
            state++;
        }
        break;
    case 3:
        //- End code first byte
        if (this_byte == 0xFE || this_byte == 0xCA)
        {
            state++;
        }
        else
        {
            state = 0;
        }
        break;
    case 4:
        //- End code second byte
        if (this_byte == 0xFE || this_byte == 0xCA)
        {
            ret = true;
            state = 0;
        }
        break;
    }
    return ret;
}

void Microphone_Init(void)
{
    //- Initialize serial module on pins 16 and 17
    MicSerial.begin(115200, SERIAL_8N1, 16, 17);
}

bool Microphone_GetNewSpectrum(Microphone_Spectrum_t* spectrum, uint32_t timeout)
{
    unsigned long start_time;
    bool got_message = false;
    start_time = millis();
    //- Send request message
    MicSerial.write("RQ");
    //- Try to get message
    while((!got_message) && ((millis() - start_time) < timeout))
    {
        int new_byte = MicSerial.read();
        if (new_byte != -1)
        {
            //- We got a byte, process it in the state machine
            if (ProcessByte((uint8_t*)spectrum->spectrum, sizeof(spectrum->spectrum), new_byte))
            {
                got_message = true;
            }
        }
    }
    //- If not got_message, that means we didn't get a message.  Try to put arduino in request mode.
    if (!got_message)
    {
        MicSerial.write("REQUEST");
    }
    return got_message;
}

void Microphone_NoiseCommandReceived(void)
{
    static unsigned long first_seq_recv = 0;
    static long seq = 0;
    unsigned long time_now;

    time_now = millis();

    //- Must receive N clicks in S seconds to set noise profile
    if (time_now - first_seq_recv > BUTTON_CLICKS_TIMEOUT)
    {
        seq = 0;
    }

    if (seq == 0)
    {
        first_seq_recv = time_now;
    }

    seq++;
    //- We hit the threshold, tell the arduino to compute noise profile
    if (seq == BUTTON_NUM_CLICKS)
    {
        Log_Println("Sending command to regerate noise profile");
        MicSerial.write("NOISE");
    }
}
