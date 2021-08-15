// dear imgui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// If you are new to dear imgui, see examples/README.txt and documentation at the top of imgui.cpp.
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan graphics context creation, etc.)

#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include "imgui.h"
#include "gfxlib\gfx_imgui.h"
#include "time.h"
extern "C" {
#include "util_MD5.h"
}

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>

#define GUI_WIDTH (1200)
#define GUI_HEIGHT (900)

//- TODO: put this in a lib
#define UID_PATTERN (0xAC1DCAFE)
#define MAX_DATA    (1024)
#pragma pack(1)
typedef struct ELFHeader_s
{
    uint32_t uid;
    uint32_t totalELFSize;
    char     md5[32];
} ELFHeader_t;
#pragma pack()

ImVec4 g_ColorGreen = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
ImVec4 g_ColorRed   = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);

char ip_address[128] = { "192.168.1.27" };
char port[128] = {"56000"};
char elf_path[_MAX_PATH] = {"C:\\git\\projects\\code\\InfinityTable\\Release\\InfinityTable.ino.bin"};
bool go = false;
float progress = 0.0f;
int  g_datapacketsize = MAX_DATA;

#define LOG_LEN (16 * 1024)
char log[LOG_LEN] = { 0 };

FILE* g_logfile;

static void ConnectAndUpdate(void);

void Log(const char* fmt, ...)
{
    char new_line[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(new_line, sizeof(new_line), fmt, args);
    va_end(args);
    size_t len = strlen(new_line);
    memmove(log + len, log, strlen(log) + 1);
    memcpy(log, new_line, len);
    log[LOG_LEN - 256] = 0;
    if (g_logfile != NULL)
    {
        (void)fprintf(g_logfile, new_line);
        fflush(g_logfile);
    }
}

static void UpdateThread(void)
{
    for (;;)
    {
        if (go)
        {
            go = false;
            ConnectAndUpdate();
            progress = 0.0f;
        }
        else
        {
            Sleep(500);
        }
    }
}

static void DrawUpdaterGUI()
{
    ImGui::Text("%20s", "ESP32 IP Address"); ImGui::SameLine();
    ImGui::InputText("##ip_addr_input", ip_address, sizeof(ip_address));
    ImGui::Text("%20s", "ESP32 Port"); ImGui::SameLine();
    ImGui::InputText("##ip_port_input", port, sizeof(port));
    ImGui::Text("%20s", "ESP32 ELF Path"); ImGui::SameLine();
    ImGui::InputText("##elf_path_input", elf_path, sizeof(elf_path));
    ImGui::Text("%20s", "Max data size"); ImGui::SameLine();
    ImGui::SliderInt("##data_size_slider", &g_datapacketsize, 8, MAX_DATA);
    if (ImGui::Button("Update##update button", ImVec2(150, 50)))
    {
        go = true;
    }
    ImGui::ProgressBar(progress);
    if (g_logfile == NULL)
    {
        ImGui::Text("Could not open log file!");
    }
    float log_width = ImGui::GetContentRegionAvailWidth();
    float log_height = GUI_HEIGHT - ImGui::GetCursorPosY();
    ImGui::InputTextMultiline("##Log multi line edit", log, sizeof(log), ImVec2(log_width, log_height));
}

int main(int, char**)
{
    Gfx_App_t app;

    Gfx_ImGui_Init(&app, "ESP32 Updater", GUI_WIDTH, GUI_HEIGHT);

    HANDLE hThread;
    hThread = CreateThread(
        NULL,                   // default security attributes
        0,                      // use default stack size  
        (LPTHREAD_START_ROUTINE)UpdateThread,       // thread function name
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

    g_logfile = fopen("ESP32Updater.log.txt", "w+");

    // Main loop
    while (!Gfx_ImGui_WindowShouldClose(&app))
    {
        Gfx_ImGui_FrameStart(&app);

        ImGui::SetNextWindowPosCenter();
        ImGui::SetNextWindowSize(ImVec2(GUI_WIDTH, GUI_HEIGHT));
        //ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, ImVec2(0, 0));
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar;
        if (ImGui::Begin("Main GUI", NULL, flags))
        {
            DrawUpdaterGUI();
        }
        ImGui::End();
        //ImGui::PopStyleVar();

        Gfx_ImGui_FrameDraw(&app);
    }

    Gfx_ImGui_Shutdown(&app);

    return 0;
}

static SOCKET ConnectToESP32(void)
{
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL,
        * ptr = NULL,
        hints;
    int iResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        Log("WSAStartup failed with error: %d\n", iResult);
        return ConnectSocket;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(ip_address, port, &hints, &result);
    if (iResult != 0) {
        Log("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return ConnectSocket;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            Log("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return ConnectSocket;
        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        Log("Unable to connect to server!\n");
        WSACleanup();
        return ConnectSocket;
    }

    Log("Connected to %s at port %s\n", ip_address, port);

    DWORD timeout = 15000;
    setsockopt(ConnectSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(ConnectSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

    return ConnectSocket;
}

static void NetworkCleanup(SOCKET ConnectSocket)
{
    closesocket(ConnectSocket);
    WSACleanup();
}

//- https://stackoverflow.com/questions/8991192/check-the-file-size-without-opening-file-in-c/18040026
int64_t FileSize(const char* name)
{
    HANDLE hFile = CreateFile(name, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return -1; // error condition, could call GetLastError to find out more

    LARGE_INTEGER size;
    if (!GetFileSizeEx(hFile, &size))
    {
        CloseHandle(hFile);
        return -1; // error condition, could call GetLastError to find out more
    }

    CloseHandle(hFile);
    return size.QuadPart;
}

static FILE* OpenFile(int64_t* fileSize)
{
    FILE* f = NULL;

    *fileSize = FileSize(elf_path);

    if (*fileSize == -1)
    {
        Log("Error getting file size\n");
        return f;
    }

    Log("ELF file size: %d\n", *fileSize);

    f = fopen(elf_path, "rb");
    if (f == NULL)
    {
        Log("Error opening file in read mode\n");
        return f;
    }

    Log("File opened successfully.\n");

    return f;
}

static bool SocketSend(SOCKET ConnectSocket, char* data, int dataLen)
{
    int iResult;
    //- // Send an initial buffer
    iResult = send(ConnectSocket, data, dataLen, 0);
    if (iResult == SOCKET_ERROR) {
            Log("send failed with error: %d\n", WSAGetLastError());
            return false;
    }
    return true;
}

static int SocketRecv(SOCKET ConnectSocket, char* data, int dataLen)
{
    int iResult;
    iResult = recv(ConnectSocket, data, dataLen, 0);
    if (iResult > 0)
    {
        return iResult;
    }
    else if (iResult == 0)
    {
        Log("Connection closed unexpectedly\n");
        return 0;
    }
    else
    {
        Log("recv failed with error: %d\n", WSAGetLastError());
        return 0;

    }
}

static bool SendAndReceiveBuf(SOCKET ConnectSocket, char* buf, char* recvBuf, size_t bufLen)
{
    bool success;
    success = SocketSend(ConnectSocket, buf, (int)bufLen);
    if (success)
    {
        int bytes_received;
        bytes_received = SocketRecv(ConnectSocket, recvBuf, (int)bufLen);
        if (bytes_received == (int)bufLen)
        {
            if (memcmp(buf, recvBuf, bufLen) == 0)
            {
                return true;
            }
            else
            {
                Log("Send and receive mismatch.\n");
                return false;
            }
        }
        else
        {
            return false;

        }
    }
    return success;
}


static void ConnectAndUpdate(void)
{
    //- https://docs.microsoft.com/en-us/windows/win32/winsock/complete-client-code
    static BYTE file_buf[8 * 1024 * 1024]; //- 8MB buffer
    SOCKET ConnectSocket;
    FILE* f;
    int64_t file_size;
    Log("Attempting to load file %s to ESP32 at IP %s and port %s\n", elf_path, ip_address, port);

    f = OpenFile(&file_size);

    if (f != NULL)
    {
        if (file_size > sizeof(file_buf))
        {
            Log("Not enough bytes in file_buf to hold ELF of %llu", file_size);
        }
        else
        {
            ELFHeader_t header;
            ELFHeader_t header_response;
            bool fail = false;
            size_t bytes_read = 0;
            while ((bytes_read < file_size) && !fail)
            {
                size_t this_bytes;
                this_bytes = fread(file_buf, sizeof(BYTE), file_size - bytes_read, f);
                if (this_bytes > 0)
                {
                    bytes_read += this_bytes;
                }
                else
                {
                    if (feof(f))
                    {
                        Log("End of file reached\n");
                        break;
                    }
                    else
                    {
                        Log("File read error\n");
                        fail = true;
                    }
                }
            }

            MD5_t md5;
            MD5_Compute(file_buf, bytes_read, &md5);
            (void)memcpy(header.md5, md5.result, 32);
            Log("File MD5: %s\n", header.md5);

            ConnectSocket = ConnectToESP32();
            if (ConnectSocket != INVALID_SOCKET)
            {
                header.uid = UID_PATTERN;
                header.totalELFSize = file_size;
                bool success;

                success = SendAndReceiveBuf(ConnectSocket, (char*)&header, (char*)&header_response, sizeof(ELFHeader_t));
                if (success)
                {
                    int packet_size = g_datapacketsize;
                    size_t bytes_sent = 0LLU;
                    while (success && bytes_sent < file_size)
                    {
                        size_t bytes_to_send = file_size - bytes_sent;
                        bytes_to_send = bytes_to_send > packet_size ? packet_size : bytes_to_send;
                        success = SocketSend(ConnectSocket, (char*)file_buf + bytes_sent, bytes_to_send);
                        if (success)
                        {
                            bytes_sent += bytes_to_send;
                            progress = ((float)bytes_sent / (float)file_size);
                        }
                    }
                    Log("Sent %d of %d bytes\n", bytes_sent, file_size);
                }
                NetworkCleanup(ConnectSocket);
            }
            fclose(f);
        }
    }


    //- 
    //- Log("Bytes Sent: %ld\n", iResult);
    //- 
    //- // shutdown the connection since no more data will be sent
    //- iResult = shutdown(ConnectSocket, SD_SEND);
    //- if (iResult == SOCKET_ERROR) {
    //-     Log("shutdown failed with error: %d\n", WSAGetLastError());
    //-     closesocket(ConnectSocket);
    //-     WSACleanup();
    //-     return;
    //- }
    //- 
    //- // Receive until the peer closes the connection
    //- do {
    //- 
    //-     iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
    //-     if (iResult > 0)
    //-         printf("Bytes received: %d\n", iResult);
    //-     else if (iResult == 0)
    //-         printf("Connection closed\n");
    //-     else
    //-         printf("recv failed with error: %d\n", WSAGetLastError());
    //- 
    //- } while (iResult > 0);

    return;
}
