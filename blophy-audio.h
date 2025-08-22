/*
 * blophy-audio.h - Oboe Wrapper for C#
 * Copyright (C) 2025  MojaveHao
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "oboe/Oboe.h"
#include <memory>
#include "libnyquist/include/libnyquist/Common.h"
#include "libnyquist/include/libnyquist/Decoders.h"

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "UnityAudioPlayer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) printf(__VA_ARGS__); printf("\n")
#define LOGW(...) printf(__VA_ARGS__); printf("\n")
#define LOGE(...) printf(__VA_ARGS__); printf("\n")
#endif

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

extern "C" typedef enum {
    AUDIO_STATE_IDLE,
    AUDIO_STATE_PLAYING,
    AUDIO_STATE_PAUSED,
    AUDIO_STATE_STOPPED
} AudioState;

class UnityAudioPlayer final : public oboe::AudioStreamCallback {
    public:
        UnityAudioPlayer();
        ~UnityAudioPlayer() override;

        bool setClip(const std::string& clipPath);
        void play();
        void playWithDelay(float delay);
        void pause();
        void stop();
        void unpause();

        float getCurrentTime() const;
        void setCurrentTime(float time);
        void offsetTime(float offset);
        void resetTime();
        void restartTime();

        void setVolume(float volume);
        float getVolume() const;

        void setLoop(bool loop);
        bool getLoop() const;

        bool isPlaying() const;
        AudioState getState() const;

        // Oboe回调接口
        oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream* audioStream,
            void* audioData,
            int32_t numFrames
        ) override;

        void onErrorAfterClose(oboe::AudioStream* audioStream, oboe::Result error) override;

    private:
        bool loadAudioData(const std::string& filePath);
        float getMusicLength() const;
        void scheduleDelayedPlay();
        void generateSilence(float* buffer, int32_t numFrames, int32_t channels);
        void generateSineWave(float* buffer, int32_t numFrames, int32_t channels, float frequency = 440.0f);

        AudioState m_state;
        float m_currentTime;
        float m_volume;
        bool m_loop;
        std::string m_clipPath;
        float m_delayTime;
        std::shared_ptr<oboe::AudioStream> m_audioStream;

        // 音频数据相关
        std::unique_ptr<nqr::AudioData> m_audioData;
        std::vector<float> m_interleavedData;
        int m_sampleRate;
        int m_channels;
        int m_totalFrames;

        // 延迟播放相关
        std::thread m_delayThread;
        std::atomic<bool> m_delayCancelled;
        std::mutex m_delayMutex;
        std::condition_variable m_delayCondition;
};

// C接口函数声明
extern "C" {
    EXPORT void* Create();
    EXPORT void Destroy(void* player);
    EXPORT void Play(void* player);
    EXPORT void PlayWithDelay(void* player, float delay);
    EXPORT void Pause(void* player);
    EXPORT void Stop(void* player);
    EXPORT void UnPause(void* player);
    EXPORT float GetCurrentTime(void* player);
    EXPORT void SetCurrentTime(void* player, float time);
    EXPORT void OffsetTime(void* player, float offset);
    EXPORT void ResetTime(void* player);
    EXPORT void RestartTime(void* player);
    EXPORT void SetClip(void* player, const char* clipPath);
    EXPORT void SetVolume(void* player, float volume);
    EXPORT float GetVolume(void* player);
    EXPORT void SetLoop(void* player, bool loop);
    EXPORT bool GetLoop(void* player);
    EXPORT bool IsPlaying(void* player);
    EXPORT AudioState GetState(void* player);
}
