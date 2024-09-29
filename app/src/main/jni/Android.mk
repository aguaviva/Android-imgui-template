LOCAL_PATH := $(call my-dir)

###############################

include $(CLEAR_VARS)

LOCAL_MODULE    := imgui
LOCAL_LDLIBS    := -llog -landroid -lGLESv3 -lEGL -lOpenSLES -lm 

FILE_LIST := imgui/imgui.cpp
FILE_LIST += imgui/imgui_demo.cpp
FILE_LIST += imgui/imgui_draw.cpp
FILE_LIST += imgui/imgui_tables.cpp
FILE_LIST += imgui/imgui_widgets.cpp
FILE_LIST += imgui/backends/imgui_impl_android.cpp
FILE_LIST += imgui/backends/imgui_impl_opengl3.cpp


LOCAL_SRC_FILES := $(FILE_LIST:$(LOCAL_PATH)/%=%)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/imgui

LOCAL_CFLAGS    := -Oz -fvisibility=hidden -ffunction-sections -fdata-sections -fno-stack-protector -fomit-frame-pointer -flto
LOCAL_LDFLAGS   := -Wl,--gc-sections -s

include $(BUILD_STATIC_LIBRARY)

###############################

include $(CLEAR_VARS)

LOCAL_MODULE    := audio
LOCAL_LDLIBS    := -llog -landroid  -lm 

FILE_LIST := $(wildcard $(LOCAL_PATH)/audio/*.cpp)
LOCAL_SRC_FILES := $(FILE_LIST:$(LOCAL_PATH)/%=%)

LOCAL_CFLAGS    := -Oz -fvisibility=hidden -ffunction-sections -fdata-sections -fno-stack-protector -fomit-frame-pointer -flto
LOCAL_LDFLAGS   := -Wl,--gc-sections -s

include $(BUILD_STATIC_LIBRARY)

###############################

include $(CLEAR_VARS)

LOCAL_MODULE    := flappybird
LOCAL_LDLIBS    := -llog -landroid -lGLESv3 -lEGL -lOpenSLES -lm -u ANativeActivity_onCreate

FILE_LIST := $(wildcard $(LOCAL_PATH)/app/*.c)
FILE_LIST += $(wildcard $(LOCAL_PATH)/app/*.cpp)

LOCAL_SRC_FILES := $(FILE_LIST:$(LOCAL_PATH)/%=%)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/imgui
LOCAL_SHARED_LIBRARIES := imgui audio

LOCAL_CFLAGS    := -Oz -fvisibility=hidden -ffunction-sections -fdata-sections -fno-stack-protector -fomit-frame-pointer -flto
LOCAL_LDFLAGS   := -Wl,--gc-sections -s
include $(BUILD_SHARED_LIBRARY)
