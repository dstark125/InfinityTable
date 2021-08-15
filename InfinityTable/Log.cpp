
#include "Log.h"

//- Global string that has messages appended to it, and then consumed
String g_log = "";

void Log_Print(String message)
{
    //- Send the message to the serial console if it's connected
    Serial.println(message);
    //- Add to the global log data, but clamp at 512
    if (g_log.length() < 512)
    {
        g_log += message;
    }
}

void Log_Println(String message)
{
    //- Just append a newline to the end of the message
    Log_Print(message + "\n");
}

void Log_Consume(String& out)
{
    //- Update the output param with the string, then clear
    out = g_log;
    g_log = "";
}

