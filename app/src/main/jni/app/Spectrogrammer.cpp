#include "imgui.h"
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
ScaleBufferBase *pScale = nullptr;
BufferAverage bufferAverage;
ChunkerProcessor chunker;

void Spectrogrammer_Init()
{
    Audio_createSLEngine(44100, 1024);
    Audio_createAudioRecorder();

    Audio_startPlay();


    pProcessor = new myFFT();
    pProcessor->init(fft_size, sampleRate);

    pScale = new ScaleBufferLogLog();
    int minFreq = ceil(((float)sampleRate/2.0)/(float)fft_size);
    int maxFreq = sampleRate/2;
    pScale->setOutputWidth(screenWidth, minFreq, maxFreq);
    pScale->PreBuild(pProcessor);

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
    float samplesAudio[1024], samplesFFT[1024];

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

                if (pScale) {

                    float volume = 1;
                    pScale->Build(bufferIO, volume);
                    
                    // +1 to skip DC
                    float *t = pScale->GetBuffer()->GetData()+1;
                    Draw_update(t, pScale->GetBuffer()->GetSize());
                    
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

    static bool logX=true,logY=true;
    bool b = false;
    b |= ImGui::Checkbox("Log x", &logX);
    ImGui::SameLine();
    b |= ImGui::Checkbox("Log y", &logY);
    if (b)
    {
        delete pScale;

        if (logX && logY)
            pScale = new ScaleBufferLogLog();
        else if (!logX && !logY)
            pScale = new ScaleBufferLinLin();
        else if (logX)
            pScale = new ScaleBufferLogLin();
        else if (logY)
            pScale = new ScaleBufferLinLog();

        int screenWidth = 1024;
        int minFreq = ceil(((float)sampleRate/2.0)/(float)fft_size);
        int maxFreq = sampleRate/2;
        pScale->setOutputWidth(screenWidth, minFreq, maxFreq);
        pScale->PreBuild(pProcessor);        
    }

    ImGui::SliderFloat("overlap", &fraction_overlap, 0.0f, 0.99f);
    ImGui::SliderFloat("decay", &decay, 0.0f, 0.99f);
    static int averaging = 1;
    if (ImGui::SliderInt("averaging", &averaging, 1,500))
        bufferAverage.setAverageCount(averaging);
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

    //ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, 0);
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
    //ImGui::PopStyleVar();

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
