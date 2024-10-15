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
#include "imgui_helpers.h"

int fft_size = 4*1024;
float sample_rate = 48000.0f;
float max_freq = -1.0f; 
float min_freq = -1.0f; 
float bin_width = -1.0f;
int screenWidth = 1024;

// perf counters
int droppedBuffers=0;
int iterationsPerChunk = 0;
int processedChunks = 0;

// GUI state
int averaging = 1;
float fraction_overlap = .5f; // 0 to 1
float decay = .5f;
float volume = 1;
bool logX=true, logY=true;

Processor *pProcessor = NULL;
ScaleBufferBase *pScaleBuffer = nullptr;
BufferAverage bufferAverage;
ChunkerProcessor chunker;

void Spectrogrammer_Init()
{
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig font_cfg;
    font_cfg.SizePixels = 22.0f * 2;
    io.Fonts->AddFontDefault(&font_cfg);

    // Arbitrary scale-up
    // FIXME: Put some effort into DPI awareness
    ImGui::GetStyle().ScaleAllSizes(3.0f * 2);

    Audio_createSLEngine(sample_rate, 1024);
    Audio_createAudioRecorder();

    Audio_startPlay();
    
    pProcessor = new myFFT();
    pProcessor->init(fft_size, sample_rate);

    min_freq = pProcessor->bin2Freq(1);
    max_freq = pProcessor->bin2Freq(fft_size/2.0f);

    bufferAverage.setAverageCount(averaging);

    Init_waterfall(screenWidth, screenWidth+512);
}

void Spectrogrammer_Shutdown()
{
    Audio_deleteAudioRecorder();
    Audio_deleteSLEngine();
    Shutdown_waterfall();
}

void draw_log_scale(ImRect frame_bb)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    for (int e=2;e<5;e++) // number of zeroes
    {
        for (int i=0;i<10;i++)
        {
            float freq = (i + 1) * pow(10.0f, e);
            float x = pScaleBuffer->FreqToX(freq);

            if (x<0)
                continue;
            if ( x>frame_bb.GetWidth())
                break;

            if (i==0)
            {
                char str[255];
                sprintf(str, "%i", (int)freq); 
                float textWidth   = ImGui::CalcTextSize(str).x;
                // append HZ to last
                if (e==4)
                {
                    strcat(str, " Hz");
                }
                window->DrawList->AddText(
                    ImVec2(frame_bb.Min.x + x - textWidth/2, frame_bb.Min.y + 150), 
                    ImGui::GetColorU32(ImGuiCol_Text), 
                    str
                );
            }

            window->DrawList->AddLine(
                ImVec2(frame_bb.Min.x + x, frame_bb.Min.y),
                ImVec2(frame_bb.Min.x + x, frame_bb.Max.y),
                (i==0) ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled));
        }

    }
}

void draw_lin_scale(ImRect frame_bb)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    for (int m=0;m<50000;m+=5000)
    {
        for (int i=0;i<5000;i+=1000)
        {
            float freq = m + i;
            float x = pScaleBuffer->FreqToX(freq);

            if (i==0)
            {
                char str[255];
                sprintf(str, "%i", (int)freq); 
                float textWidth   = ImGui::CalcTextSize(str).x;
                window->DrawList->AddText(
                    ImVec2(frame_bb.Min.x + x - textWidth/2, frame_bb.Min.y + 150), 
                    ImGui::GetColorU32(ImGuiCol_Text), 
                    str
                );
            }

            window->DrawList->AddLine(
                ImVec2(frame_bb.Min.x + x, frame_bb.Min.y),
                ImVec2(frame_bb.Min.x + x, frame_bb.Max.y),
                (i==0) ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled));
        }

    }
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

    static float samplesAudio[1024], samplesFFT[1024];

    // pass available buffers to processor
    if (pRecQueue!=NULL)
    {
        sample_buf *buf = nullptr;
        while (pRecQueue->front(&buf))
        {
            pRecQueue->pop();

            // last block? copy samples for drawing it
            if (pRecQueue->size()==0)
            {
                int bufSize = AU_LEN(buf->cap_);
                int16_t *buff = (int16_t *)(buf->buf_);
                for (int n = 0; n < bufSize; n++)
                {
                    samplesAudio[n] = (float)buff[n];
                }
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

    ImGui::PlotLines("Waveform", samplesAudio, 1024);

    bool bScaleChanged = false;
    bScaleChanged |= ImGui::Checkbox("Log x", &logX);
    ImGui::SameLine();
    bScaleChanged |= ImGui::Checkbox("Log y", &logY);

    if (bScaleChanged || pScaleBuffer==NULL)
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

        pScaleBuffer->setOutputWidth(screenWidth, min_freq, max_freq);
        pScaleBuffer->PreBuild(pProcessor);        
    }

    ImGui::SliderFloat("overlap", &fraction_overlap, 0.0f, 0.99f);
    ImGui::SliderFloat("decay", &decay, 0.0f, 0.99f);
    if (ImGui::SliderInt("averaging", &averaging, 1,500))
        bufferAverage.setAverageCount(averaging);

    //if we have enough data queued process the fft
    static BufferIODouble *pBufferIO = NULL;
    while (chunker.Process(pProcessor, decay, fraction_overlap))
    {
        pBufferIO = pProcessor->getBufferIO();
        pBufferIO = bufferAverage.Do(pBufferIO);
        if (pBufferIO!=NULL)
        {
            pScaleBuffer->Build(pBufferIO, volume);
            
            // +1 to skip DC
            Draw_update(
                pScaleBuffer->GetBuffer()->GetData(), 
                pScaleBuffer->GetBuffer()->GetSize()
            );
            processedChunks++;
        }
    }

    // Draw FFT lines
    {
        uint32_t values_count = 1024;
        int frame_height = 150;

        bool hovered;
        ImRect frame_bb;
        if (block_add("fft", ImVec2(1024, frame_height), &frame_bb, &hovered))
        {
            const ImGuiStyle& style = GImGui->Style;
            ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);        

            static float data[8192];            
            static int size = 0;
            if (pBufferIO!=NULL)
            {
                size = pBufferIO->GetSize();
                float *pData = pBufferIO->GetData();
                
                for (int i=0;i<size;i++)
                {
                    float freq = pProcessor->bin2Freq(i);
                    data[2*i+0] = pScaleBuffer->FreqToX(freq);
                    data[2*i+1] = pData[i];
                }
            }

            draw_lines(frame_bb, data, size);
            if (logX)
                draw_log_scale(frame_bb);   
            else
                draw_lin_scale(frame_bb);   

            ImGui::InvisibleButton("no", ImVec2(1024, 15));         
        }
    }

    if (true)
    {
        float seconds_per_row = (((float)fft_size/sample_rate) * (float)averaging) * (1.0 - fraction_overlap);

        bool hovered;
        ImRect frame_bb;
        if (block_add("wfall", ImVec2(screenWidth, screenWidth+512), &frame_bb, &hovered))
        {
            Draw_waterfall(frame_bb);
            Draw_vertical_scale(frame_bb, seconds_per_row);
            //draw_log_scale(frame_bb);
        }
    }

    ImGui::End();

    // return processed buffers
    {
        sample_buf *buf = nullptr;
        while (chunker.getFreeBufferFrontAndPop(&buf)) {
            pFreeQueue->push(buf);
        }
    }
}
