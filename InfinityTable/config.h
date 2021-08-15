

//- WiFi
#define WIFI_STATUS_SEND_MS (1000)
#define SSID     ("YOUR_WIFI_SSIID")
#define PASSWORD ("YOUR_WIFI_PASSWORD")

//- LEDs
#define TOTAL_LEDS_PER_STRIP            (200) //- There are really 202, but we don't need to write the last 2 black
#define NUM_LEDS_PER_STRIP              (197)
#define FIRST_LED_OFFSET                (3) //- 0 based
#define NUM_STRIPS                      (5)
#define DESIRED_FPS                     (60) //- Target framerate
#define DESIRED_LOOP_TIME               (1000 / DESIRED_FPS)

//- Microphone
#define MICROPHONE_TIMEOUT              (100) //- Milliseconds
#define MICROPHONE_F_BINS               (64) //- Must match what's on the Arduino
#define SPECTRUM_HISTORY_LEN            (12) //- 70hz so about 14ms each and we want 120ms of history
#define SPECTRUM_MAX_TIME_MS            (120) //- When looking at the audio spectrum, compare results up to this age
#define MICROPHONE_DROPPING_MAX_MIN     (30U)

//- Parallelogram
#define MAX_PARALLELOGRAMS              (20)
#define PARALLELOGRAM_MAX_SPEED         (5)  //- Note more speeds = lower min speed. Speed of 5 means move once per 5 frames.

//- Software 
#define SOFTARE_UPDATE_PORT             (56000) //- TCP port to connect to for the ESP32Updater
#define SOFTARE_UPDATE_LISTEN_TIMEOUT_S (30)
#define SOFTARE_UPDATE_RECV_TIMEOUT_S   (15)

//- Flashy rainbow
#define FLASHY_RAINBOW_NUM_FLASHES      (10)    //- Number of LEDs to turn on every animation frame
#define FLASHY_RAINBOW_FADE_RATE        (35)    //- How fast they fade out

//- Other
#define BUTTON_CLICKS_TIMEOUT           (5000) //- To get into mode, must do BUTTON_NUM_CLICKS clicks in BUTTON_CLICKS_TIMEOUT seconds
#define BUTTON_NUM_CLICKS               (5)    //- To get into mode, must do BUTTON_NUM_CLICKS clicks in BUTTON_CLICKS_TIMEOUT seconds

