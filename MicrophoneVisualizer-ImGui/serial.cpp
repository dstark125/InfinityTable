
#include <Windows.h>
#include <stdio.h>
#include <stdint.h>
#include "serial.h"
#include "time.h"

ArduinoSamples_t g_samples;
static bool g_enabled = true;

static HANDLE OpenSerialPort(uint32_t port)
{
    HANDLE hComm;
    COMMTIMEOUTS timeouts;
    COMMCONFIG dcbSerialParams;

    char file_name[64];

    (void)sprintf(file_name, "\\.\\COM%u", port);

    hComm = CreateFile(file_name,
        GENERIC_READ | GENERIC_WRITE,
        0,
        0,
        OPEN_EXISTING,
        0,
        0);
    if (hComm == INVALID_HANDLE_VALUE)
    {
        return NULL;
    }

    if (!GetCommState(hComm, &dcbSerialParams.dcb))
    {
        printf("error getting state \n");
    }

    dcbSerialParams.dcb.BaudRate = 115200;
    dcbSerialParams.dcb.ByteSize = 8;
    dcbSerialParams.dcb.StopBits = ONESTOPBIT;
    dcbSerialParams.dcb.Parity = NOPARITY;

    if (!SetCommState(hComm, &dcbSerialParams.dcb))
    {
        printf(" error setting serial port state \n");
    }

    SetCommMask(hComm, EV_RXCHAR);

    GetCommTimeouts(hComm, &timeouts);
    //COMMTIMEOUTS timeouts = {0};

    timeouts.ReadIntervalTimeout = 25;
    timeouts.ReadTotalTimeoutConstant = 25;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 25;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hComm, &timeouts))
    {
        printf("error setting port state \n");
    }

    return hComm;
}

#define HEADER (0xAC1D)
#define TRAILER (0xCAFE)
#define BUFFER_SIZE (1024)

static bool ProcessByte(uint8_t* message, size_t messageLen, uint8_t this_byte)
{
    static uint32_t state = 0;
    static uint32_t state_2_count = 0;
    static uint8_t  temp_buf[BUFFER_SIZE];
    bool            ret = false;
    if (messageLen > BUFFER_SIZE)
    {
        printf("THIS SHOULD BE AN ASSERTION -> BUFFER_SIZE TOO SMALL.\n");
        return false;
    }
    switch (state)
    {
    case 0:
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
        temp_buf[state_2_count] = this_byte;
        state_2_count++;
        if (state_2_count >= messageLen)
        {
            state_2_count = 0;
            state++;
        }
        break;
    case 3:
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
        if (this_byte == 0xFE || this_byte == 0xCA)
        {
            (void)memcpy(message, temp_buf, messageLen);
            ret = true;
            state = 0;
        }
        break;
    }
    return ret;
}

void SerialReadThread(void)
{
    for (;;)
    {
        uint8_t     recvBuf[64];
        bool        result;
        uint8_t     this_byte;
        DWORD       bytes_read;
        bool        connected = false;
        HANDLE      hSerial = NULL;
        FILE*       fhdl;
        double      last_receive_time;

        while (!g_enabled)
        {
            Sleep(250);
        }

        while (!connected && g_enabled)
        {
            double start_t;
            uint32_t port_num;
            for (port_num = 0; port_num < 25; port_num++)
            {
                start_t = Time_Get();
                printf("Trying to connect to COMM port %u\n", port_num);
                hSerial = OpenSerialPort(port_num);
                while ((!connected) && (hSerial != NULL) && (Time_Get() - start_t) < 5.0)
                {
                    result = ReadFile(hSerial, &this_byte, sizeof(uint8_t), &bytes_read, 0);
                    if (result)
                    {
                        if (bytes_read > 0)
                        {
                            printf("%x", this_byte);
                            if (ProcessByte((uint8_t*)recvBuf, sizeof(recvBuf), this_byte))
                            {
                                printf("\n");
                                printf("Connected to port %u\n", port_num);
                                connected = true;
                            }
                        }
                    }
                }
                if (connected)
                {
                    break;
                }
                else if (hSerial != NULL)
                {
                    (void)CloseHandle(hSerial);
                    hSerial = NULL;
                }
            }
            Sleep(500);
        }

        last_receive_time = Time_Get();

        while (connected && g_enabled)
        {
            result = ReadFile(hSerial, &this_byte, sizeof(uint8_t), &bytes_read, 0);
            if (result)
            {
                if (bytes_read > 0)
                {
                    //printf("%x", this_byte);
                    if (ProcessByte((uint8_t*)recvBuf, sizeof(recvBuf), this_byte))
                    {
                        g_samples.head = (g_samples.head + 1) % SAMPLE_HISTORY;
                        g_samples.timeReceived[g_samples.head] = Time_Get();
                        for (int i = 0; i < NUM_SAMPLES; i++)
                        {
                            g_samples.samples[i][g_samples.head] = recvBuf[i];
                            g_samples.lastSample[i] = recvBuf[i];
                        }
                        g_samples.sampleSeq++;
                        last_receive_time = Time_Get();
                    }
                }
                else
                {
                    if (Time_Get() - last_receive_time > 10.0)
                    {
                        printf("Timeout...\n");
                        (void)CloseHandle(hSerial);
                        hSerial = NULL;
                        connected = false;
                    }
                }
            }
            else
            {
                printf("read error...\n");
                (void)CloseHandle(hSerial);
                hSerial = NULL;
                connected = false;
            }
        }
        //- If enabled went false we want to close the handle
        if (hSerial != NULL)
        {
            (void)CloseHandle(hSerial);
            hSerial = NULL;
        }
    }
}

void Serial_GetSamples(ArduinoSamples_t* samples)
{
    *samples = g_samples;
}

void StartSerialThread(void)
{
    HANDLE hThread;
    hThread = CreateThread(
        NULL,                   // default security attributes
        0,                      // use default stack size  
        (LPTHREAD_START_ROUTINE)SerialReadThread,       // thread function name
        NULL,                   // argument to thread function 
        0,                      // use default creation flags 
        NULL);                  // returns the thread identifier 


    // Check the return value for success.
    // If CreateThread fails, terminate execution. 
    // This will automatically clean up threads and memory. 

    if (hThread == NULL)
    {
        ExitProcess(3);
    }

    g_enabled = true;
}

void Serial_Enable(bool enabled)
{
    g_enabled = enabled;
}
