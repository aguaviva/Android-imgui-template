#include "imgui.h"
#include "backends/imgui_impl_android.h"
#include "backends/imgui_impl_opengl3.h"
#include <android/log.h>
#include "android_native_app_glue.h"
#include <android/asset_manager.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <stdlib.h>

static int image_width = 256;
static int image_height = 256;
static GLuint image_texture;


void Init_waterfall()
{

    // Create a OpenGL texture identifier
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int32_t* image_data = (int32_t*)malloc(image_width*image_height*4);
    for (int y=0;y<image_height;y++)
    {
        for (int x=0;x<image_width;x++)
        {
            image_data[x+y*image_width] = (0xff<<24) |((x&y)<<16) |((x&y)<<8) | (x&y);
        }
    }

    // Upload pixels into texture
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    

    free(image_data);
}


void Draw_waterfall()
{
    ImGui::Image((void*)(intptr_t)image_texture, ImVec2(image_width, image_height));
}

void Shutdown_waterfall()
{
    glDeleteTextures(1, &image_texture);
}
