/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-websocket-server-arduino/
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

// Import required libraries
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "html.h"
#include "LEDControl.h"
#include "WifiControl.h"
#include "config.h"
#include "SoftwareUpdate.h"
#include "Microphone.h"
#include "Log.h"

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
//- Create a websocket to communicate with clients
AsyncWebSocket ws("/ws");

bool g_connected = false;

//- Send status message to the client (browser)
//- This is currently at 1Hz, and contains everything to update the web page
static void SendStatusMessage(void)
{
    String status;
    const LEDControl_Config_t * led_config;
    led_config = LEDControl_GetConfig();
    //- Send key=value piped together
    status = "MB=";
    status += led_config->maxBrightness;
    status += "|SS=";
    status += led_config->scrollSpeed;
    status += "|Mode=";
    status += led_config->mode;
    status += "|Red=";
    status += led_config->red;
    status += "|Green=";
    status += led_config->green;
    status += "|Blue=";
    status += led_config->blue;
    //- textAll sends to all clients, so works for all connected browsers
    ws.textAll(status);
}

//- Sends the log message to all clients
static void SendLog(void)
{
    String message;
    //- Get the full log string. Consume empties the string after you get it
    Log_Consume(message);
    //- Only send if we have messages
    if (message.length() > 0)
    {
        //- key=value pair
        message = "Log=" + message;
        ws.textAll(message);
    }
}

//- Process a command received from a client through the websocket
static bool ProcessCommand(const char* key, const char* val)
{
    bool handled = false;
    if (strcmp(key, "MB") == 0) //- Max brightness
    {
        if (val == NULL)
        {
            Serial.println("Invalid max brightness command received.");
        }
        else
        {
            Serial.print("Max brighness ");
            Serial.println(val);
            LEDControl_SetMaxBrightness(atoi(val));
            handled = true;
        }
    }
    else if (strcmp(key, "SS") == 0) //- Scroll speed
    {
        if (val == NULL)
        {
            Serial.println("Invalid scroll speed command received.");
        }
        else
        {
            Serial.print("Scroll speed ");
            Serial.println(val);
            LEDControl_SetScrollSpeed(atoi(val));
            handled = true;
        }
    }
    else if (strcmp(key, "DT") == 0) //- Display text (string)
    {
        if (val == NULL)
        {
            Serial.println("Invalid display text command received.");
        }
        else
        {
            Serial.print("Display text: ");
            Serial.println(val);
            LEDControl_SetDisplayText(val);
            handled = true;
        }
    }
    else if (strcmp(key, "Mode") == 0) //- Mode change
    {
        if (val == NULL)
        {
            Serial.println("Invalid mode command received.");
        }
        else
        {
            Serial.print("Mode ");
            Serial.println(val);
            LEDControl_SetMode((LEDControl_Mode_t)atoi(val));
            handled = true;
        }
    }
    else if (strcmp(key, "Red") == 0)  //- Red slider value
    {
        if (val == NULL)
        {
            Serial.println("Invalid red command received.");
        }
        else
        {
            Serial.print("Red ");
            Serial.println(val);
            LEDControl_SetRed((uint8_t)atoi(val));
            handled = true;
        }
    }
    else if (strcmp(key, "Green") == 0) //- Green slider value 
    {
        if (val == NULL)
        {
            Serial.println("Invalid green command received.");
        }
        else
        {
            Serial.print("Green ");
            Serial.println(val);
            LEDControl_SetGreen((uint8_t)atoi(val));
            handled = true;
        }
    }
    else if (strcmp(key, "Blue") == 0) //- Blue slider value
    {
        if (val == NULL)
        {
            Serial.println("Invalid blue command received.");
        }
        else
        {
            Serial.print("Blue ");
            Serial.println(val);
            LEDControl_SetBlue((uint8_t)atoi(val));
            handled = true;
        }
    }
    else if (strcmp(key, "SoftwareUpdate") == 0) //- Software update button was clicked
    {
        SoftwareUpdate_CommandReceived();
        handled = true;
    }
    else if (strcmp(key, "NoiseButton") == 0) //- Generate noise floor button was clicked
{
        Microphone_NoiseCommandReceived();
        handled = true;
    }
    return handled;
}

//- Processes a web socket data message
//- Parses it out, and the passes the key/value to ProcessCommand() to do the real work
static void HandleWebSocketMessage(void* arg, uint8_t* data, size_t len)
{
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    //- Check to make sure the frame is complete
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) 
    {
        char* tok_key;
        //- Add null character
        data[len] = 0;
        //- Split the string at the = if there is one
        tok_key = strtok((char*)data, "=");
        if (tok_key != NULL)
        {
            char* tok_val;
            bool handled = false;
            //- Split again to get a pointer to the value, if there is one
            tok_val = strtok(NULL, "=");
            //- Pass both strings in. Note tok_val may be NULL
            handled = ProcessCommand(tok_key, tok_val);
            if (!handled)
            {
                Serial.print("Unhandled command received: ");
                Serial.println((char*)data);
            }
        }
    }
}

//- Websocket event callback for processing message
static void OnEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type,
    void* arg, uint8_t* data, size_t len) 
{
    switch (type) {
    case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
    case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
    case WS_EVT_DATA:
        //- Right now we only really care about DATA
        HandleWebSocketMessage(arg, data, len);
        break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
        break;
    }
}

//- Initializes the global web socket
static void InitWebSocket()
{
    ws.onEvent(OnEvent);
    server.addHandler(&ws);
}

//- HTML processor. See html.h for the placeholders that are replaced.
static String Processor(const String& var)
{
    //- The websocket needs to know what IP address to send data to, so replace the placeholder with our IP
    if (var == "IP_ADDR_PLACEHOLDER") {
        return WiFi.localIP().toString();
    }
    return String();
}

//- Setup function
void WifiControl_Setup(void)
{
    Serial.print("Connecting to ");
    Serial.println(SSID);
    //- Start wifi stack
    WiFi.begin(SSID, PASSWORD);
    //- Wait until connected
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    // Print local IP address and start web server
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    InitWebSocket();

    //- Setup async web server
    //- On HTTP_GET, send index_html which is defined in html.h
    //- Note Processor is called to update placeholders in the HTML before it's sent
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send_P(200, "text/html", index_html, Processor);
        });

    // Start ascynchronous server
    server.begin();

    g_connected = true;
}

//- Periodic update of wifi stack
void WifiControl_Update(void) 
{
    static uint32_t sequence = 0;
    static uint32_t last_send = 0;
    static uint32_t last_cleanup = 0;
    uint32_t time_now = millis();
    //- Perform certain tasks based at set intervals
    if (time_now - last_cleanup > 666)
    {
        ws.cleanupClients();
        last_cleanup = time_now;
    }
    if (time_now - last_send > WIFI_STATUS_SEND_MS)
    {
        SendStatusMessage();
        last_send = time_now;
    }
    //- Send log every time - it only sends if theres data. This way we can keep up.
    SendLog();
}

//- Retreives the IP address as a string, or "NOT CONNECTED"
String WifiControl_GetIPAddress(void)
{
    String ip_addr;
    if (g_connected)
    {
        ip_addr = WiFi.localIP().toString();
    }
    else
    {
        ip_addr = "NOT CONNECTED";
    }
    return ip_addr;
}
