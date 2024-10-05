#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_android.h"
#include "backends/imgui_impl_opengl3.h"
#include "android_native_app_glue.h"
#include <android/asset_manager.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <stdlib.h>
#include <math.h>
#include "colormaps.h"

static int image_width = 512;
static int image_height = 512;
static GLuint image_texture;
static int offset = 0;

void Init_waterfall(int16_t width, int16_t height)
{
    image_width = width;
    image_height = height;

    // Create a OpenGL texture identifier
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    uint16_t* image_data = (uint16_t*)malloc(image_width*image_height*2);
    for (int y=0;y<image_height;y++)
    {
        for (int x=0;x<image_width;x++)
        {
            image_data[x+y*image_width] = 0;
        }
    }

    // Upload pixels into texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image_width, image_height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, image_data);
    

    free(image_data);
}

void update_texture_row(uint16_t *image_data)
{
    glBindTexture(GL_TEXTURE_2D, image_texture);
    glTexSubImage2D(GL_TEXTURE_2D,
        0,
        0,
        offset,
        image_width,
        1, //height
        GL_RGB, GL_UNSIGNED_SHORT_5_6_5, image_data);    
    glBindTexture(GL_TEXTURE_2D, 0);

    offset--;
    if (offset<0)
        offset = image_height-1;

}

void Draw_update(uint16_t *pData, uint32_t size)
{
    if (pData==NULL)
        return;

    uint16_t image_data[1024];
    for(int i=0;i<image_width;i++)
    {
        image_data[i] = GetMagma(pData[i]);
    }

    update_texture_row(image_data);
}

void Draw_update(float *pData, uint32_t size)
{
    if (pData==NULL)
        return;

    uint16_t image_data[1024];
    for(int i=0;i<image_width;i++)
    {
        image_data[i] = GetMagma(pData[i] *255);
    }

    update_texture_row(image_data);
}


void Draw_waterfall(float seconds_per_row)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImVec2 iv = ImGui::GetCursorPos();

    float foffset = offset/(float)(image_height-1);

    ImGui::Image((void*)(intptr_t)image_texture, ImVec2(image_width, image_height-offset), ImVec2(0,foffset), ImVec2(1,1));
    ImGui::SetCursorPos(ImVec2(iv.x, iv.y+image_height-offset));
    ImGui::Image((void*)(intptr_t)image_texture, ImVec2(image_width, offset), ImVec2(0,0), ImVec2(1,foffset ));
    
    // find closest scale
    float d = 25;
    float notch_distance = d;
    float scale = 1;
    float min_err = 10000;
    float scale_list[] = {1.0f/20.0f, 1.0f/10.0f, 1.0f/2.0f, 1.0f, 3.0f, 4.0f, 10.0f, 30.0f, 60.0f, 120.0f, 300.0f, 600.0f, 1200.0f, 3600.0f};
    for (int i=0;i<14;i++)
    {        
        float nd = scale_list[i]/seconds_per_row;
        float err = abs(nd-d);
        if (err<min_err)
            min_err = err;
        if (err>min_err)
            break;
        notch_distance = nd;
        scale = scale_list[i];
    }
    
    // find time magnitude
    float total_time = seconds_per_row * image_height;
    int time_magnitude = -1;
    if (total_time>60*60)
        time_magnitude=3;
    else if (total_time>60)
        time_magnitude=2;
    else if (total_time>20)
        time_magnitude=1;
    else 
        time_magnitude=0;

    // draw scale
    ImU32 col = IM_COL32(200, 200, 200, 200);
    for(int i=0;;i++)
    {
        float y = floor((float)i * notch_distance);
        if (y>=image_height)
            break;

        bool long_notch = (i % 5)==0;

        window->DrawList->AddLine(
            ImVec2(iv.x, iv.y + y),
            ImVec2(iv.x + (long_notch?50:25), iv.y + y),
            col);
        
        if (long_notch)
        {
            int t = floor(i*scale);
            char s[255];
            int l;

            switch(time_magnitude)
            {
                case 0: l = sprintf(s, "%0.2fs", i*scale); break;
                case 1: l = sprintf(s, "%02is", t % 60); break;
                case 2: l = sprintf(s, "%im%02is", t/60, t % 60); break;
                case 3: l = sprintf(s, "%ih%02im", t/(60*60), (t%(60*60)) / 60); break;
            }

            window->DrawList->AddText(ImVec2(iv.x + 50, iv.y + y), col, s, s+l);
        }

    }
}

void Shutdown_waterfall()
{
    glDeleteTextures(1, &image_texture);
}
