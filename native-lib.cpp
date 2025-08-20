/*
* native-lib.cpp - Oboe Wrapper for C#
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

#include <jni.h>
#include <string>
#include <unordered_map>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include "oboe/Oboe.h"

extern "C" JNIEXPORT JNICALL jstring Java_net_blophy_nova_oboe_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

extern "C" typedef enum {
    AUDIO_STATE_IDLE,
    AUDIO_STATE_PLAYING,
    AUDIO_STATE_PAUSED,
    AUDIO_STATE_STOPPED
} AudioState;

// 前置声明
class UnityAudioPlayer;

// 全局变量
static std::unordered_map<void *, UnityAudioPlayer *> g_audioPlayers;
static int g_nextPlayerId = 1;
static AAssetManager *g_assetManager = nullptr;

// Unity音频播放器类
class UnityAudioPlayer : public oboe::AudioStreamCallback {
public:
    UnityAudioPlayer() :
            m_state(AUDIO_STATE_IDLE),
            m_currentTime(0.0f),
            m_volume(1.0f),
            m_loop(false),
            m_audioStream(nullptr) {
    }

    ~UnityAudioPlayer() {
        if (m_audioStream) {
            m_audioStream->close();
            m_audioStream.reset();
        }
    }

    // 设置音频剪辑
    bool setClip(const std::string &clipPath) {
        // 这里需要实现从assets加载音频文件的逻辑
        // 伪代码:
        // 1. 使用AAssetManager_open打开音频文件
        // 2. 解析音频文件格式(WAV, MP3, OGG等)
        // 3. 解码音频数据到内存
        m_clipPath = clipPath;
        return true;
    }

    // 播放控制
    void play() {
        if (m_state == AUDIO_STATE_PAUSED) {
            unpause();
            return;
        }

        // 创建并配置音频流
        oboe::AudioStreamBuilder builder;
        builder.setDirection(oboe::Direction::Output);
        builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
        builder.setSharingMode(oboe::SharingMode::Exclusive);
        builder.setFormat(oboe::AudioFormat::Float);
        builder.setChannelCount(oboe::ChannelCount::Stereo);
        builder.setSampleRate(48000); // 标准采样率
        builder.setCallback(this);

        oboe::Result result = builder.openStream(m_audioStream);
        if (result != oboe::Result::OK) {
            // 处理错误
            return;
        }

        result = m_audioStream->requestStart();
        if (result == oboe::Result::OK) {
            m_state = AUDIO_STATE_PLAYING;
        }
    }

    void playWithDelay(float delay) {
        // 实现延迟播放逻辑
        // 可以使用定时器或基于音频帧计数实现
        m_delayTime = delay;
        // 设置定时器，在延迟后调用play()
    }

    void pause() {
        if (m_audioStream && m_state == AUDIO_STATE_PLAYING) {
            m_audioStream->pause();
            m_state = AUDIO_STATE_PAUSED;
        }
    }

    void stop() {
        if (m_audioStream) {
            m_audioStream->stop();
            m_state = AUDIO_STATE_STOPPED;
            m_currentTime = 0.0f;
        }
    }

    void unpause() {
        if (m_audioStream && m_state == AUDIO_STATE_PAUSED) {
            m_audioStream->start();
            m_state = AUDIO_STATE_PLAYING;
        }
    }

    // 时间控制
    float getCurrentTime() const {
        return m_currentTime;
    }

    void setCurrentTime(float time) {
        m_currentTime = time;
        // 需要根据时间定位到具体的音频帧位置
    }

    void offsetTime(float offset) {
        m_currentTime += offset;
        // 确保时间在有效范围内 [0, 音频长度]
    }

    void resetTime() {
        m_currentTime = 0.0f;
    }

    void restartTime() {
        m_currentTime = 0.0f;
        if (m_state == AUDIO_STATE_PLAYING) {
            stop();
            play();
        }
    }

    // 音量控制
    void setVolume(float volume) {
        m_volume = volume;
        // 如果需要，可以实时更新音频流的音量
    }

    float getVolume() const {
        return m_volume;
    }

    // 循环设置
    void setLoop(bool loop) {
        m_loop = loop;
    }

    bool getLoop() const {
        return m_loop;
    }

    // 状态查询
    bool isPlaying() const {
        return m_state == AUDIO_STATE_PLAYING;
    }

    AudioState getState() const {
        return m_state;
    }

    // Oboe回调接口
    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *audioStream,
            void *audioData,
            int32_t numFrames) override {

        if (m_state != AUDIO_STATE_PLAYING) {
            return oboe::DataCallbackResult::Stop;
        }

        // 计算每帧的时间增量
        float frameTime = numFrames / static_cast<float>(audioStream->getSampleRate());

        // 更新当前时间
        m_currentTime += frameTime;

        // 获取音频数据并写入audioData
        // 这里需要根据m_currentTime从音频数据中提取相应的帧

        // 如果到达音频末尾
        if (m_currentTime >= getMusicLength()) {
            if (m_loop) {
                m_currentTime = 0.0f; // 循环播放
            } else {
                m_state = AUDIO_STATE_STOPPED;
                return oboe::DataCallbackResult::Stop;
            }
        }

        return oboe::DataCallbackResult::Continue;
    }

    void onErrorAfterClose(oboe::AudioStream *audioStream, oboe::Result error) override {
        // 处理音频流错误
        m_state = AUDIO_STATE_STOPPED;
    }

private:
    AudioState m_state;
    float m_currentTime;
    float m_volume;
    bool m_loop;
    std::string m_clipPath;
    float m_delayTime;
    std::shared_ptr<oboe::AudioStream> m_audioStream;

    // 伪代码: 音频数据相关成员
    // std::vector<float> m_audioData;
    // int m_sampleRate;
    // int m_channels;

    float getMusicLength() const {
        // 返回音频长度(秒)
        // 伪代码: return m_audioData.size() / (m_sampleRate * m_channels);
        return 0.0f; // 需要根据实际音频数据计算
    }
};

void* AudioPlayer_Create() {
    UnityAudioPlayer* player = new UnityAudioPlayer();
    void* handle = reinterpret_cast<void*>(g_nextPlayerId++);
    g_audioPlayers[handle] = player;
    return handle;
}

void AudioPlayer_Destroy(void* player) {
    auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        delete it->second;
        g_audioPlayers.erase(it);
    }
}

void AudioPlayer_Play(void* player) {
    auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->play();
    }
}

void AudioPlayer_PlayWithDelay(void* player, float delay) {
    auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->playWithDelay(delay);
    }
}

void AudioPlayer_Pause(void* player) {
    auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->pause();
    }
}

void AudioPlayer_Stop(void* player) {
    auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->stop();
    }
}

void AudioPlayer_UnPause(void* player) {
    auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->unpause();
    }
}

float AudioPlayer_GetCurrentTime(void* player) {
    auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        return it->second->getCurrentTime();
    }
    return 0.0f;
}

void AudioPlayer_SetCurrentTime(void* player, float time) {
    auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->setCurrentTime(time);
    }
}

void AudioPlayer_OffsetTime(void* player, float offset) {
    auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->offsetTime(offset);
    }
}

void AudioPlayer_ResetTime(void* player) {
    auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->resetTime();
    }
}

void AudioPlayer_RestartTime(void* player) {
    auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->restartTime();
    }
}

void AudioPlayer_SetClip(void* player, const char* clipPath) {
    auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->setClip(clipPath);
    }
}

extern "C" JNIEXPORT JNICALL float AudioPlayer_GetMusicLength(void *player) {
    auto it = g_audioPlayers.find((void *) player);
    if (it != g_audioPlayers.end()) {
        // 需要在实际的UnityAudioPlayer中实现getMusicLength方法
        return 0.0f;
    }
    return 0.0f;
}

extern "C" JNIEXPORT JNICALL void AudioPlayer_SetVolume(void *player, float volume) {
    auto it = g_audioPlayers.find((void *) player);
    if (it != g_audioPlayers.end()) {
        it->second->setVolume(volume);
    }
}

extern "C" JNIEXPORT JNICALL float AudioPlayer_GetVolume(void *player) {
    auto it = g_audioPlayers.find((void *) player);
    if (it != g_audioPlayers.end()) {
        return it->second->getVolume();
    }
    return 0.0f;
}

extern "C" JNIEXPORT JNICALL jint AudioPlayer_IsPlaying(void *player) {
    auto it = g_audioPlayers.find((void *) player);
    if (it != g_audioPlayers.end()) {
        return it->second->isPlaying() ? 1 : 0;
    }
    return 0;
}

extern "C" JNIEXPORT JNICALL void AudioPlayer_SetLoop(void *player, int loop) {
    auto it = g_audioPlayers.find((void *) player);
    if (it != g_audioPlayers.end()) {
        it->second->setLoop(loop != 0);
    }
}

extern "C" JNIEXPORT JNICALL int AudioPlayer_GetLoop(void *player) {
    auto it = g_audioPlayers.find((void *) player);
    if (it != g_audioPlayers.end()) {
        return it->second->getLoop() ? 1 : 0;
    }
    return 0;
}

extern "C" JNIEXPORT JNICALL AudioState AudioPlayer_GetState(void *player) {
    auto it = g_audioPlayers.find((void *) player);
    if (it != g_audioPlayers.end()) {
        return it->second->getState();
    }
    return AUDIO_STATE_IDLE;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_net_blophy_nova_oboe_MainActivity_CreateAudioPlayer(JNIEnv *env, jobject thiz) {
    auto *player = new UnityAudioPlayer();
    void *handle = reinterpret_cast<void *>(g_nextPlayerId++);
    g_audioPlayers[handle] = player;
    return (long) handle;
}

extern "C"
JNIEXPORT void JNICALL
Java_net_blophy_nova_oboe_MainActivity_DestroyAudioPlayer(JNIEnv *env, jobject thiz, jlong player) {
    auto it = g_audioPlayers.find((void *) player);
    if (it != g_audioPlayers.end()) {
        delete it->second;
        g_audioPlayers.erase(it);
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_net_blophy_nova_oboe_MainActivity_Play(JNIEnv *env, jobject thiz, jlong player) {
    auto it = g_audioPlayers.find((void *) player);
    if (it != g_audioPlayers.end()) {
        it->second->play();
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_net_blophy_nova_oboe_MainActivity_PlayWithDelay(JNIEnv *env, jobject thiz, jlong player,
                                                     jfloat delay) {
    auto it = g_audioPlayers.find((void *) player);
    if (it != g_audioPlayers.end()) {
        it->second->playWithDelay(delay);
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_net_blophy_nova_oboe_MainActivity_Pause(JNIEnv *env, jobject thiz, jlong player) {
    auto it = g_audioPlayers.find((void *) player);
    if (it != g_audioPlayers.end()) {
        it->second->pause();
    }
}


extern "C"
JNIEXPORT void JNICALL
Java_net_blophy_nova_oboe_MainActivity_Stop(JNIEnv *env, jobject thiz, jlong player) {
    auto it = g_audioPlayers.find((void *) player);
    if (it != g_audioPlayers.end()) {
        it->second->stop();
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_net_blophy_nova_oboe_MainActivity_Unpause(JNIEnv *env, jobject thiz, jlong player) {
    auto it = g_audioPlayers.find((void *) player);
    if (it != g_audioPlayers.end()) {
        it->second->unpause();
    }
}
extern "C"
JNIEXPORT jfloat JNICALL
Java_net_blophy_nova_oboe_MainActivity_GetCurrentTime(JNIEnv *env, jobject thiz, jlong player) {
    auto it = g_audioPlayers.find((void *) player);
    if (it != g_audioPlayers.end()) {
        return it->second->getCurrentTime();
    }
    return 0.0f;
}
extern "C"
JNIEXPORT void JNICALL
Java_net_blophy_nova_oboe_MainActivity_SetCurrentTime(JNIEnv *env, jobject thiz, jlong player,
                                                      jfloat time) {
    auto it = g_audioPlayers.find((void *) player);
    if (it != g_audioPlayers.end()) {
        it->second->setCurrentTime(time);
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_net_blophy_nova_oboe_MainActivity_OffsetTime(JNIEnv *env, jobject thiz, jlong player,
                                                  jfloat offset) {
    auto it = g_audioPlayers.find((void *) player);
    if (it != g_audioPlayers.end()) {
        it->second->offsetTime(offset);
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_net_blophy_nova_oboe_MainActivity_ResetTime(JNIEnv *env, jobject thiz, jlong player) {
    auto it = g_audioPlayers.find((void *) player);
    if (it != g_audioPlayers.end()) {
        it->second->restartTime();
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_net_blophy_nova_oboe_MainActivity_RestartTime(JNIEnv *env, jobject thiz, jlong player) {
    auto it = g_audioPlayers.find((void *) player);
    if (it != g_audioPlayers.end()) {
        it->second->resetTime();
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_net_blophy_nova_oboe_MainActivity_SetClip(JNIEnv *env, jobject thiz, jlong player,
                                               jstring clip_path) {
    auto it = g_audioPlayers.find((void *) player);
    if (it != g_audioPlayers.end()) {
        it->second->setClip(std::string(env->GetStringUTFChars(clip_path, NULL)));
    }
}