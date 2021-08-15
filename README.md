# InfinityTable
Code n such for the infinity table:

ESP32Updater 
  - Custom OTA updater for Windows using the Dear ImGui framework (https://github.com/ocornut/imgui)
  - If you haven't checked out ImGui, I highly recommend you do.
  - I have my own libraries in another repo, so you'll need to download and get ImGui working with this application yourself, or just used the checked-in EXE.

InfinityTable
  - All of the code for the ESP32 in the Infinity Table
  - Talks with MicrophoneProcessor (Arduino Nano) via serial
 
 MicrophoneProcessor
  - All of the code for the Arduino Nano in the Infinity Table
  - Talks with InfinityTable (ESP32) via serial

MicrophoneVisualizer-ImGui
  - Another ImGui application I wrote to help develop an algorithm for displaying the microphone data as it is much easier to rapid-prototype on Windows than on the table
  - This code is a huge mess, sorry
  - Your best bet is to run the EXE with a working MicrophoneProcessor attached
