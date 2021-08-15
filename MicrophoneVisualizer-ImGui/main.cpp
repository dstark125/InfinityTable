// dear imgui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// If you are new to dear imgui, see examples/README.txt and documentation at the top of imgui.cpp.
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan graphics context creation, etc.)

#include <stdio.h>
#include "imgui.h"
#include "gfxlib\gfx_imgui.h"
#include "serial.h"
#include "time.h"
#include "math.h"

#define GUI_WIDTH (1400)
#define GUI_HEIGHT (1000)

ImVec4 g_ColorGreen = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
ImVec4 g_ColorRed   = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);

#define NOISE_CALC_SAMPLES (15)

typedef struct NoiseHelper_s
{
    uint32_t freq;
    uint8_t  freqSamples[NOISE_CALC_SAMPLES];
    uint32_t sampleIdx;
    uint32_t sampleSum;
}NoiseHelper_t;

static void UpdateNoise(NoiseHelper_t* helper, uint8_t samples[NUM_SAMPLES], float noiseOut[NUM_SAMPLES])
{
    //- Gather this sample
    helper->freqSamples[helper->sampleIdx] = samples[helper->freq];
    helper->sampleSum += helper->freqSamples[helper->sampleIdx];
    helper->sampleIdx++;
    //- If we have a full buffer, compute
    if (helper->sampleIdx == NOISE_CALC_SAMPLES)
    {
        //- https://www.mathsisfun.com/data/standard-deviation-formulas.html
        float variance_sq_sum = 0.0f;
        uint32_t average = helper->sampleSum / NOISE_CALC_SAMPLES;
        for (uint32_t i = 0; i < NOISE_CALC_SAMPLES; i++)
        {
            float variance = ((float)helper->freqSamples[i] - (float)average);
            float variance_sq = (variance * variance);
            variance_sq_sum += variance_sq;
        }
        float std_dev = sqrtf((1.0f / NOISE_CALC_SAMPLES) * variance_sq_sum);
        noiseOut[helper->freq] = (average + std_dev);

        //- Reset
        helper->freq = (helper->freq + 1) % NUM_SAMPLES;
        helper->sampleIdx = 0U;
        helper->sampleSum = 0U;
    }
}

static void DrawArduino(NoiseHelper_t noiseHelpers[4])
{
    static float seconds = 0.1f;
    static float equilize_max = 2.0f;
    static float dropping_max = 0.0f;
    static float equilize_min = 0.5f;
    static bool serial_enabled = true;
    static bool frozen = false;
    static bool equilize = false;
    static float all_time_maxs[NUM_SAMPLES] = { 0.0f };
    static ArduinoSamples_t samples = { 0 };
    static float noise[NUM_SAMPLES] = { 0U };
    float equilizer[NUM_SAMPLES] = { 0.0f };
    float sample_maxs[NUM_SAMPLES] = { 0.0f };
    float sample_avgs[NUM_SAMPLES] = { 0.0f };
    float sample_noise_cancelled[NUM_SAMPLES] = { 0.0f };
    float all_max = 0.0f;
    float all_time_average;
    float all_time_sum = 0.0f;
    int   all_time_n = 0;
    float loudness_sum = 0.0f;
    char maxes_string[1024] = "";
    char temp_string[64];
    double time_now = Time_Get();
    float framerate;
    bool max_found = false;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(GUI_WIDTH, GUI_HEIGHT));

    if (!frozen)
    {
        Serial_GetSamples(&samples);
    }

    UpdateNoise(&noiseHelpers[0], samples.lastSample, noise);
    UpdateNoise(&noiseHelpers[1], samples.lastSample, noise);
    UpdateNoise(&noiseHelpers[2], samples.lastSample, noise);
    UpdateNoise(&noiseHelpers[3], samples.lastSample, noise);

    for (uint32_t i = 0; i < NUM_SAMPLES; i++)
    {
        float sum = 0.0f;
        for (uint32_t j = 0; j < SAMPLE_HISTORY; j++)
        {
            if ((time_now - samples.timeReceived[j]) < seconds)
            {
                sum += samples.samples[i][j];
                if (samples.samples[i][j] > sample_maxs[i])
                {
                    sample_maxs[i] = samples.samples[i][j];
                }
            }
        }
        sample_avgs[i] = sum / SAMPLE_HISTORY;
        if (sample_maxs[i] > dropping_max)
        {
            dropping_max = sample_maxs[i];
            max_found = true;
        }
        if (sample_maxs[i] > all_time_maxs[i])
        {
            all_time_maxs[i] = sample_maxs[i];
        }
        if (all_time_maxs[i] > all_max)
        {
            all_max = all_time_maxs[i];
        }
        if (all_time_maxs[i] != 0.0f )
        {
            all_time_sum += all_time_maxs[i];
            all_time_n++;
        }
        loudness_sum += sample_maxs[i];
        sample_noise_cancelled[i] = sample_maxs[i] - noise[i];
    }
    if (!max_found)
    {
        dropping_max = dropping_max > 30.0f ? dropping_max - 1.0f : 30.0f;
    }
    all_time_average = all_time_sum / all_time_n;
    (void)sprintf(maxes_string, "{");
    if (equilize)
    {
        for (uint32_t i = 0; i < NUM_SAMPLES; i++)
        {
            float eq_value = all_time_maxs[i] > 0.0f ? all_time_average / all_time_maxs[i] : 1.0f;
            eq_value = eq_value < equilize_min ? equilize_min : eq_value;
            eq_value = eq_value > equilize_max ? equilize_max : eq_value;
            equilizer[i] = eq_value;
            (void)sprintf(temp_string, "%.3ff, ", eq_value);
            (void)strcat(maxes_string, temp_string);
        }
    }
    else
    {
        for (uint32_t i = 0; i < NUM_SAMPLES; i++)
        {
            (void)sprintf(temp_string, "%u, ", (unsigned int)all_time_maxs[i]);
            (void)strcat(maxes_string, temp_string);
        }
    }
    size_t maxes_string_len = strlen(maxes_string);
    if (maxes_string_len > 2U)
    {
        maxes_string[maxes_string_len - 2U] = 0;
    }
    (void)strcat(maxes_string, "}");

    framerate = samples.timeReceived[samples.head + 1 % SAMPLE_HISTORY] - samples.timeReceived[samples.head];
    framerate /= SAMPLE_HISTORY;
    framerate = 1.0f / framerate;

    ImGui::Begin("Microphone Visualizer");
    ImGui::Text("Time: %f", Time_Get());
    ImGui::Text("---- Max samples over time period");
    ImGui::PlotHistogram("Maxes over sample period##Graph_Maxs", sample_maxs, NUM_SAMPLES, 0, 0, 0.0f, all_max, ImVec2(GUI_WIDTH, GUI_HEIGHT * 0.25f));
    ImGui::Text("---- Max samples over time period - dropping max");
    ImGui::PlotHistogram("Samples max dropping##Graph_Maxs", sample_maxs, NUM_SAMPLES, 0, 0, 0.0f, dropping_max, ImVec2(GUI_WIDTH, GUI_HEIGHT * 0.25f));
    ImGui::Text("---- Noise compensated");
    ImGui::PlotHistogram("Samples noise compensated##Graph_Maxs", sample_noise_cancelled, NUM_SAMPLES, 0, 0, 0.0f, dropping_max, ImVec2(GUI_WIDTH, GUI_HEIGHT * 0.25f));
    ImGui::Text("---- Noise");
    ImGui::PlotHistogram("Computed Noise##Noise", noise, NUM_SAMPLES, 0, 0, 0.0f, all_max, ImVec2(GUI_WIDTH, GUI_HEIGHT * 0.15f));
    ImGui::Text("---- All Time Maxes");
    ImGui::PlotHistogram("All time maxes##All_Maxes", all_time_maxs, NUM_SAMPLES, 0, 0, 0.0f, all_max, ImVec2(GUI_WIDTH, GUI_HEIGHT * 0.15f));
    ImGui::Text("Seq: %u", samples.sampleSeq); ImGui::SameLine();
    ImGui::Text("Dropping max: %.1f", dropping_max); ImGui::SameLine();
    ImGui::Text("Loudness sum: %.1f", loudness_sum); ImGui::SameLine();
    ImGui::Text("Microphone Rate: %.1fhz", framerate); ImGui::SameLine();
    if (ImGui::Button("Clear all time maxes##button"))
    {
        (void)memset(all_time_maxs, 0, sizeof(all_time_maxs));
    }
    if (ImGui::Checkbox("Serial Enabled##checkbox", &serial_enabled))
    {
        Serial_Enable(serial_enabled);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Frozen##checkbox", &frozen);
    ImGui::SameLine();
    ImGui::SliderFloat("Sample time##slider", &seconds, 0.02f, 1.0f);
    ImGui::Text("This is a string of the max levesls that can be copied and pasted inside MicrophoneProcessor.ino as an array for noise cancellation or equilizer.");
    ImGui::Text("Check this box to equilize the maxes."); ImGui::SameLine();
    ImGui::Checkbox("Equilize##checkbox", &equilize); ImGui::SameLine();
    ImGui::PushItemWidth(150.0f);
    ImGui::SliderFloat("Min##equilize slider", &equilize_min, 0.0f, 1.0f); ImGui::SameLine();
    ImGui::SliderFloat("Max##equilize slider", &equilize_max, 1.0f, 10.0f);
    ImGui::PopItemWidth();
    ImGui::PushItemWidth(GUI_WIDTH - 20.0f);
    ImGui::InputText("##noise string input", maxes_string, sizeof(maxes_string));
    ImGui::PopItemWidth();
    if (equilize)
    {
        ImGui::PlotLines("Equilizer##graph", equilizer, NUM_SAMPLES, 0, NULL, equilize_min, equilize_max, ImVec2(GUI_WIDTH, GUI_HEIGHT * 0.2f));
    }
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
}

int main(int, char**)
{
    Gfx_App_t app;
    NoiseHelper_t noise[4] = {0};

    noise[0].freq = 0;
    noise[1].freq = 16;
    noise[2].freq = 32;
    noise[3].freq = 48;

    Gfx_ImGui_Init(&app, "Microphone Visualizer", GUI_WIDTH, GUI_HEIGHT);

    StartSerialThread();

    // Main loop
    while (!Gfx_ImGui_WindowShouldClose(&app))
    {
        Gfx_ImGui_FrameStart(&app);

        DrawArduino(noise);

        Gfx_ImGui_FrameDraw(&app);
    }

    Gfx_ImGui_Shutdown(&app);

    return 0;
}
