#include "imgui.h"
#include "imgui_internal.h"
#include "waterfall.h"
#include "Processor.h"
#include "ChunkerProcessor.h"
#include "ScaleBuffer.h"
#include "BufferAverage.h"
#include "ScaleBufferBase.h"
#include "audio/audio_main.h"
#include "BufferAverage.h"

int fft_size = 4*1024;
int sampleRate = 48000;
int screenWidth = 1024;
int droppedBuffers=0;
int iterationsPerChunk = 0;
int processedChunks = 0;
float fraction_overlap = .5f; // 0 to 1
float decay = .5f;

Processor *pProcessor = NULL;
ScaleBufferBase *pScaleBuffer = nullptr;
BufferAverage bufferAverage;
ChunkerProcessor chunker;

void Spectrogrammer_Init()
{
    Audio_createSLEngine(44100, 1024);
    Audio_createAudioRecorder();

    Audio_startPlay();


    pProcessor = new myFFT();
    pProcessor->init(fft_size, sampleRate);

    pScaleBuffer = new ScaleBufferLogLog();
    int minFreq = ceil(((float)sampleRate/2.0)/(float)fft_size);
    int maxFreq = sampleRate/2;
    pScaleBuffer->setOutputWidth(screenWidth, minFreq, maxFreq);
    pScaleBuffer->PreBuild(pProcessor);

    bufferAverage.setAverageCount(1);

    Init_waterfall(screenWidth, screenWidth+512);
}

void Spectrogrammer_Shutdown()
{
    Audio_deleteAudioRecorder();
    Audio_deleteSLEngine();
    Shutdown_waterfall();
}

void Spectrogrammer_MainLoopStep()
{
    ImGuiIO& io = ImGui::GetIO();

    AudioQueue* pFreeQueue = nullptr;
    AudioQueue* pRecQueue = nullptr;

    float sampleRate;
    GetBufferQueues(&sampleRate, &pFreeQueue, &pRecQueue);

    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoTitleBar;
    window_flags |= ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoResize;

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f)); // place the next window in the top left corner (0,0)
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize); // make the next window fullscreen
    ImGui::Begin("imgui window", NULL, window_flags); // create a window

    int c = 0;
    static float samplesAudio[1024], samplesFFT[1024];

    // pass available buffers to processor
    if (pRecQueue!=NULL)
    {
        sample_buf *buf = nullptr;
        while (pRecQueue->front(&buf))
        {
            pRecQueue->pop();

            if (c==0)
            {
                int bufSize = AU_LEN(buf->cap_);
                int16_t *buff = (int16_t *)(buf->buf_);
                for (int n = 0; n < 1024; n++)
                {
                    samplesAudio[n] = (float)buff[n];
                }
                c++;
            }

            if (chunker.pushAudioChunk(buf) == false)
            {
                droppedBuffers++;
            }
        }    
    }    
    else
    {
        ImGui::Text("pRecQueue is NULL");
    }   

    ImGui::PlotLines("Samples Audio", samplesAudio, 1024);

    static bool logX=true,logY=true;
    bool b = false;
    b |= ImGui::Checkbox("Log x", &logX);
    ImGui::SameLine();
    b |= ImGui::Checkbox("Log y", &logY);
    if (b)
    {
        delete pScaleBuffer;

        if (logX && logY)
            pScaleBuffer = new ScaleBufferLogLog();
        else if (!logX && !logY)
            pScaleBuffer = new ScaleBufferLinLin();
        else if (logX)
            pScaleBuffer = new ScaleBufferLogLin();
        else if (logY)
            pScaleBuffer = new ScaleBufferLinLog();

        int screenWidth = 1024;
        int minFreq = ceil(((float)sampleRate/2.0)/(float)fft_size);
        int maxFreq = sampleRate/2;
        pScaleBuffer->setOutputWidth(screenWidth, minFreq, maxFreq);
        pScaleBuffer->PreBuild(pProcessor);        
    }

    c = 0;

    //if we have enough data queued process the fft
    while (chunker.Process(pProcessor, decay, fraction_overlap))
    {
        BufferIODouble *bufferIO = pProcessor->getBufferIO();
        if (bufferIO!=nullptr)
        {
            bufferIO = bufferAverage.Do(bufferIO);
            if (bufferIO!=nullptr)
            {
                processedChunks++;

                if (pScaleBuffer) {

                    float volume = 1;
                    pScaleBuffer->Build(bufferIO, volume);
                    
                    // +1 to skip DC
                    float *t = pScaleBuffer->GetBuffer()->GetData()+1;
                    Draw_update(t, pScaleBuffer->GetBuffer()->GetSize());
                    
                    if (c==0)
                    {
                        for(int i=0;i<1024;i++)
                            samplesFFT[i] = t[i];

                        c++;
                    }                    
                }
            }

            iterationsPerChunk++;
        }
    }

    ImGui::SliderFloat("overlap", &fraction_overlap, 0.0f, 0.99f);
    ImGui::SliderFloat("decay", &decay, 0.0f, 0.99f);
    static int averaging = 1;
    if (ImGui::SliderInt("averaging", &averaging, 1,500))
        bufferAverage.setAverageCount(averaging);
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

    /*
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

        ImGui::PlotLines(
            "FFT", 
            samplesFFT, 
            1024, 
            0, // values_offset
            NULL, // overlay_text
            0, // float scale_min = FLT_MAX,
            FLT_MAX, // float scale_max, 
            ImVec2(1024, 100), 
            sizeof(float)// int stride = sizeof(float)
        );
        ImGui::PopStyleVar();
    */

    {
        char label[] = "afsdfs";
        uint32_t values_count = 1024;
        int frame_height = 100;
        float *pData = &samplesFFT[0];

        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems == false)
        {
            const ImGuiStyle& style = g.Style;
            const ImGuiID id = window->GetID(label);

            const ImVec2 frame_size(1024, frame_height);

            const ImRect frame_bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos.x + frame_size.x, window->DC.CursorPos.y + frame_size.y));
            ImGui::ItemSize(frame_bb, 0);
            if (ImGui::ItemAdd(frame_bb, id, NULL, ImGuiItemFlags_NoNav))
            {
                bool hovered;
                ImGui::ButtonBehavior(frame_bb, id, &hovered, NULL);

                ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);        

                float scale_min = FLT_MAX;
                float scale_max = FLT_MAX;

                float v_min = FLT_MAX;
                float v_max = -FLT_MAX;
                for (int i = 0; i < values_count; i++)
                {
                    const float v = pData[i];
                    if (v != v) // Ignore NaN values
                        continue;
                    v_min = ImMin(v_min, v);
                    v_max = ImMax(v_max, v);
                }
                if (scale_min == FLT_MAX)
                    scale_min = v_min;
                if (scale_max == FLT_MAX)
                    scale_max = v_max;

                ImU32 col = IM_COL32(200, 200, 200, 200);

                const float inv_scale = (scale_min == scale_max) ? 0.0f : (1.0f / (scale_max - scale_min));
                ImVec2 tp0 = ImVec2( 0.0f, 1.0f - ImSaturate((pData[0] - scale_min) * inv_scale) );
                for (int i = 1; i < values_count; i++)
                {
                    const ImVec2 tp1 = ImVec2( (float)i/(float)values_count, 1.0f - ImSaturate((pData[i] - scale_min) * inv_scale) );
                    ImVec2 pos0 = ImLerp(frame_bb.Min, frame_bb.Max, tp0);
                    ImVec2 pos1 = ImLerp(frame_bb.Min, frame_bb.Max, tp1);
                    window->DrawList->AddLine(pos0, pos1, col);
                    tp0 = tp1;
                }
            }
        }
    }

    float time_per_row = (((float)fft_size/sampleRate) * (float)averaging) * (1.0 - fraction_overlap);

    Draw_waterfall(time_per_row);

    ImGui::End();

    // return processed buffers
    {
        sample_buf *buf = nullptr;
        while (chunker.getFreeBufferFrontAndPop(&buf)) {
            pFreeQueue->push(buf);
        }
    }
}
