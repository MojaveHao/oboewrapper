APP_STL := c++_shared
APP_CPPFLAGS := -frtti -std=c++17
APP_PLATFORM := android-19
APP_CFLAGS += -Wno-error=format-security
APP_BUILD_SCRIPT := Android.mk
APP_ABI := armeabi-v7a x86