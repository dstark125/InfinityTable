
#include "Arduino.h" //- Not sure why I need this here
#include "config.h"
#include "LEDControl.h"
#include "Update.h"
#include "Log.h"
#include "MD5Builder.h"
#include "stdint.h"

//- For network stuff
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>


//- TODO: put this in a lib
#define UID_PATTERN (0xAC1DCAFE)
#define MAX_DATA    (512)
//- Header we will receive from update program
typedef struct ELFHeader_s
{
    uint32_t uid;
    uint32_t totalELFSize;
    char     md5[32];
}__attribute__((packed)) ELFHeader_t;

//- Data used to track the progress of an update
typedef struct UpdateTracker_s
{
    ELFHeader_t        header;
    uint32_t           totalBytes;
    uint32_t           bytesWritten;
    bool               beginSuccess;
    bool               fullFileReceived;
}UpdateTracker_t;

//- Global helpers for controlling update start/stop
bool update_software = false;
unsigned long update_start_time = 0;
LEDControl_Mode_t led_mode_to_restore = LED_MODE_OFF;

//- Receive on a sucket until a certain length has been received
static uint32_t ReceiveData(int sock, uint8_t* buf, size_t bufLen)
{
    int len;
    uint32_t received = 0;
    do 
    {
        //- Receive remaining data
        len = recv(sock, buf + received, bufLen - received, 0);
        //- Check for error
        if (len < 0) 
        {
            int32_t error = errno;
            if (error == 11)
            {
                Log_Println("Timeout on receiving data from socket.");
            }
            else
            {
                Log_Println("Error occurred during receiving: errno " + String(error));
            }
        }
        else if (len == 0) 
        {
            Log_Println("Connection closed");
        }
        else 
        {
            //- No errors, received data
            //- Log_Println("Received byte: " + String(len));
            received += len;
        }
    } while (received < bufLen && len > 0); //- Keep going while haven't gotten enough and no errors
    return received;
}

//- Sends bufLen bytes of data in a socket
static bool SendData(int sock, uint8_t* buf, size_t bufLen)
{
    int written;
    size_t bytes_sent = 0;
    //- Loop until done and while no errors
    do 
    {
        //- Rend remaining data
        written = send(sock, buf + bytes_sent, bufLen - bytes_sent, 0);
        //- Check for error
        if (written <= 0) {
            int32_t error = errno;
            Log_Println("Error occurred during sending: errno " + String(error));
        }
        else
        {
            bytes_sent += written;
        }
    } while (bytes_sent < bufLen && written > 0);
    //- True/false was sent
    return bytes_sent == bufLen;
}

//- Finishes a software load. Checks for success, aborts if necessary depending on the work that was done.
static void WrapupSoftwareLoad(UpdateTracker_t* updateTracker)
{
    //- Check for start
    if (updateTracker->beginSuccess)
    {
        //- Do we think we received the full thing?
        if (updateTracker->fullFileReceived)
        {
            //- Does the header data match what we wrote?
            if (updateTracker->bytesWritten == updateTracker->header.totalELFSize)
            {
                //- We think we're good, try to apply update
                //- Note MD5 check is done in end()
                bool committed = Update.end();
                Log_Println("WrapupSoftwareLoad Update MD5: " + Update.md5String());
                if (committed)
                {
                    //- Good. Reboot into the old software
                    Log_Println("WrapupSoftwareLoad SUCCESSFULLY COMMITTED SOFTWARE");
                    Log_Println("WrapupSoftwareLoad Rebooting ESP");
                    ESP.restart();
                }
                else
                {
                    Log_Println("WrapupSoftwareLoad error on Update.end(): " + String(Update.getError()));
                    Update.abort();
                }
            }
            else
            {
                Log_Println("WrapupSoftwareLoad bytes written to update and total file size don't match.");
                Update.abort();
            }
        }
        else
        {
            Log_Println("WrapupSoftwareLoad Update tracker reports full file was not received.");
            Update.abort();
        }
    }
    else
    {
        Log_Println("WrapupSoftwareLoad() has nothing to do because begin was not successful.");
    }
}

//- Handle a new ELF data chunk received
static bool ProcessReceivedDataPacket(UpdateTracker_t* updateTracker, uint8_t* data, uint32_t packetBytes, bool* done)
{
    bool success = true;
    //- Update the byte tracker, handling being done
    updateTracker->totalBytes += packetBytes;
    if (updateTracker->totalBytes == updateTracker->header.totalELFSize)
    {
        Log_Println("Successfully received full file.");
        updateTracker->fullFileReceived = true;
        *done = true;
    }
    //- If we successfully started the update library, write the bytes
    if (updateTracker->beginSuccess)
    {
        size_t written;
        written = Update.write(data, packetBytes);
        //- Check for error
        if (written != packetBytes)
        {
            Log_Println("Update.write() did not write the correct number of bytes.");
            success = false;
        }
        updateTracker->bytesWritten += written;
    }
    else
    {
        Log_Println("Can't write data because Update.begin() failed!");
        success = false;
    }
    return success;
}

//- Tries to receive and update the ESP32 software
static void ReceiveSoftwareUpdate(int sock)
{
    bool success;
    UpdateTracker_t update_tracker = { 0 };
    static ELFHeader_t header;
    uint32_t bytes_recevied;

    //- Get the first header
    bytes_recevied = ReceiveData(sock, (uint8_t*)&header, sizeof(ELFHeader_t));

    if (bytes_recevied == sizeof(ELFHeader_t))
    {
        //- Check validity of this header
        if (header.uid == UID_PATTERN)
        {
            //- Get MD5 out
            char md5_str[33] = { 0 };
            memcpy(md5_str, header.md5, 32);

            //- Header is mostly complete, echo it back
            if (SendData(sock, (uint8_t*)&header, sizeof(ELFHeader_t)))
            {
                update_tracker.totalBytes = 0U;
                update_tracker.header = header;
                //- Start the updater library now that we know the size
                update_tracker.beginSuccess = Update.begin(header.totalELFSize, U_FLASH);
                //- Apply the MD5 string for safety
                if (Update.setMD5(md5_str))
                {
                    //- We are ready to receive the update
                    static uint8_t receive_buf[MAX_DATA]; //- Static so it's on the heap
                    bool done = false;
                    Log_Println("File MD5: " + String(md5_str));
                    //- Receive the whole file
                    do
                    {
                        size_t bytes_to_receive = sizeof(receive_buf);
                        size_t bytes_remaining = update_tracker.header.totalELFSize - update_tracker.totalBytes;
                        bytes_to_receive = bytes_remaining < bytes_to_receive ? bytes_remaining : bytes_to_receive;
                        //- Get new data
                        bytes_recevied = ReceiveData(sock, receive_buf, bytes_to_receive);
                        if (bytes_recevied > 0)
                        {
                            //- Got data, apply it
                            success = ProcessReceivedDataPacket(&update_tracker, receive_buf, bytes_recevied, &done);
                        }
                        else
                        {
                            Log_Println("Stopped receiving bytes - exiting software load procedure.");
                            success = false;
                        }
                    } while (success && !done);
                }
            }
            else
            {
                Log_Println("Error setting MD5. ELF Header MD5 length: " + String(strlen(md5_str)));
            }
        }
        else
        {
            Log_Println("Header had UID: " + String(header.uid) + " but expected: " + String(UID_PATTERN));
        }
    }
    else
    {
        Log_Println("Failed to receive ELF header. Return value: " + String(bytes_recevied));
    }

    //- Always wrapup the load, just in case we need to clean anything up
    WrapupSoftwareLoad(&update_tracker);
}

//- Waits for updater connection, and attempts to update if it gets one
static void WaitForConnectionAndUpdate(void)
{
    char addr_str[128];
    int ip_protocol = 0;
    //- int keepAlive = 1;
    //- int keepIdle = KEEPALIVE_IDLE;
    //- int keepInterval = KEEPALIVE_INTERVAL;
    //- int keepCount = KEEPALIVE_COUNT;
    struct sockaddr_storage dest_addr;

    //- Setup the socket information
    struct sockaddr_in* dest_addr_ip4 = (struct sockaddr_in*)&dest_addr;
    dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4->sin_family = AF_INET;
    dest_addr_ip4->sin_port = htons(SOFTARE_UPDATE_PORT);
    ip_protocol = IPPROTO_IP;

    //- Create a listen socket
    int listen_sock = socket(AF_INET, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        int32_t error = errno;
        Log_Println("Unable to create socket: errno " + String(error));
        return;
    }

    //- Set the socket timeout to something long
    int opt = 1;
    struct timeval to;
    to.tv_sec = SOFTARE_UPDATE_LISTEN_TIMEOUT_S;
    to.tv_usec = 0;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(listen_sock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));

    Log_Println("Socket created");

    //- Bind the listening socket now that it's setup
    int err = bind(listen_sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (err == 0) {
        Log_Println("Socket bound, port " + String(SOFTARE_UPDATE_PORT));

        //- Listen for new connections
        err = listen(listen_sock, 1);
        if (err == 0) 
        {
            Log_Println("Socket listening");

            struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
            socklen_t addr_len = sizeof(source_addr);

            //- Wait until someone connects
            int sock = accept(listen_sock, (struct sockaddr*)&source_addr, &addr_len);
            if (sock < 0) 
            {
                int32_t error = errno;
                //- Figure out what went wrong
                if (error == 11)
                {
                    Log_Println("Software Update timed out while waiting for connection.");
                }
                else
                {
                    Log_Println("Unable to accept connection: errno " + String(error));
                }
            }
            else
            {
                //- We got a connection
                // Set tcp keepalive option
                //- setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
                //- setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
                //- setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
                //- setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
                // Convert ip address to string
                if (source_addr.ss_family == PF_INET) {
                    inet_ntoa_r(((struct sockaddr_in*)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
                }
                Log_Println("Socket accepted ip address: " + String(addr_str));

                //- Set timeout much shorter than accept timeout
                struct timeval to;
                to.tv_sec = SOFTARE_UPDATE_RECV_TIMEOUT_S;
                to.tv_usec = 0;
                setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));

                //- Try to do update
                ReceiveSoftwareUpdate(sock);

                //- If we got here, we didn't update
                //- Cleanup
                shutdown(sock, 0);
                close(sock);
            }

        }
        else{
            int32_t error = errno;
            if (error == 11)
            {
                Log_Println("Software Update timed out while waiting for connection.");
            }
            else
            {
                Log_Println("Error occurred during listen: errno " + String(error));
            }
        }
    }
    else{
        int32_t error = errno;
        Log_Println("Socket unable to bind: errno " + String(error));
    }

    //- Cleanup
    close(listen_sock);

    return;
}

//- Process receiving a command to update software
void SoftwareUpdate_CommandReceived(void)
{
    static unsigned long first_seq_recv = 0;
    static long seq = 0;
    unsigned long time_now;

    time_now = millis();

    //- Must get N button presses in S seconds in order to update
    if (time_now - first_seq_recv > BUTTON_CLICKS_TIMEOUT)
    {
        seq = 0;
    }

    if (seq == 0)
    {
        first_seq_recv = time_now;
    }
    
    seq++;

    //- We hit the required count, update sofware
    if (seq == BUTTON_NUM_CLICKS)
    {
        //- For cleanliness, backup the mode in case we want to go back later
        //- Also transition to OFF so the LEDs aren't stale and using power.
        Log_Println("Enabling software update mode");
        update_software = true;
        update_start_time = time_now;
        const LEDControl_Config_t * cfg = LEDControl_GetConfig();
        led_mode_to_restore = cfg->mode;
        LEDControl_SetMode(LED_MODE_OFF);
    }
}

void SoftwareUpdate_Update(void)
{
    //- Don't do anything if not updating
    if (update_software)
    {
        unsigned long time_now = millis();
        //- Let the other code run for 5 seconds to transition LEDs off
        if (time_now - update_start_time < 5000)
        {
            return;
        }
        Log_Println("Starting software update sequence");
        //- Do update
        WaitForConnectionAndUpdate();
        //- If we got here update failed, restore mode
        LEDControl_SetMode(led_mode_to_restore);
        update_software = false;
    }
}
