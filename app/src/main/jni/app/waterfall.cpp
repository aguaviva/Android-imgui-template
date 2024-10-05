#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_android.h"
#include "backends/imgui_impl_opengl3.h"
#include "android_native_app_glue.h"
#include <android/asset_manager.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <stdlib.h>
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
            image_data[x+y*image_width] = TO16BITS((x&y)<<16, (x&y)<<8, x&y);
        }
    }

    // Upload pixels into texture
    //glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
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


void Draw_waterfall()
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImVec2 iv = ImGui::GetCursorPos();

    float foffset = offset/(float)(image_height-1);

    ImGui::Image((void*)(intptr_t)image_texture, ImVec2(image_width, image_height-offset), ImVec2(0,foffset), ImVec2(1,1));
    ImGui::SetCursorPos(ImVec2(iv.x, iv.y+image_height-offset));
    ImGui::Image((void*)(intptr_t)image_texture, ImVec2(image_width, offset), ImVec2(0,0), ImVec2(1,foffset ));

    //ImGui::Image((void*)(intptr_t)image_texture, ImVec2(image_width, image_height), ImVec2(0,0), ImVec2(1,1));

    window->DrawList->AddLine(
        ImVec2(200, iv.y),
        ImVec2(200, iv.y+image_height),
        IM_COL32(200, 200, 200, 200));

}

void Shutdown_waterfall()
{
    glDeleteTextures(1, &image_texture);
}
