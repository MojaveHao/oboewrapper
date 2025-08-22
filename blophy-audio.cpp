/*
 * blophy-audio.cpp - Oboe Wrapper for C#
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

#include "blophy-audio.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

// 全局变量
static std::unordered_map<void *, UnityAudioPlayer *> g_audioPlayers;
static auto g_nextPlayerId = 1;
static AAssetManager* g_assetManager = nullptr;

// 设置AssetManager (从Java端调用)
extern "C" JNIEXPORT void JNICALL
Java_net_blophy_audio_setAssetManager(JNIEnv *env, jclass clazz, const jobject assetManager) {
    g_assetManager = AAssetManager_fromJava(env, assetManager);
}

UnityAudioPlayer::UnityAudioPlayer() :
        m_state(AUDIO_STATE_IDLE),
        m_currentTime(0.0f),
        m_volume(1.0f),
        m_loop(false),
        m_delayTime(0.0f),
        m_sampleRate(48000),
        m_channels(2),
        m_totalFrames(0),
        m_delayCancelled(false) {
    // 不再生成测试音频，等待setClip调用
}

UnityAudioPlayer::~UnityAudioPlayer() {
    m_delayCancelled = true;
    m_delayCondition.notify_all();
    if (m_delayThread.joinable()) {
        m_delayThread.join();
    }

    if (m_audioStream) {
        m_audioStream->close();
        m_audioStream.reset();
    }
}

std::vector<uint8_t> loadAssetData(const std::string& filename) {
    std::vector<uint8_t> buffer;

    if (!g_assetManager) {
        LOGE("AssetManager is not set!");
        return buffer;
    }

    AAsset* asset = AAssetManager_open(g_assetManager, filename.c_str(), AASSET_MODE_UNKNOWN);
    if (!asset) {
        LOGE("Failed to open asset: %s", filename.c_str());
        return buffer;
    }

    const off_t size = AAsset_getLength(asset);
    buffer.resize(size);

    const int read = AAsset_read(asset, buffer.data(), size);
    if (read != size) {
        LOGE("Failed to read asset: %s", filename.c_str());
        buffer.clear();
    }

    AAsset_close(asset);
    return buffer;
}

bool UnityAudioPlayer::setClip(const std::string &clipPath) {
    m_clipPath = clipPath;
    return loadAudioData(clipPath);
}

void UnityAudioPlayer::play() {
    if (m_state == AUDIO_STATE_PAUSED) {
        unpause();
        return;
    }

    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output);
    builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    builder.setSharingMode(oboe::SharingMode::Exclusive);
    builder.setFormat(oboe::AudioFormat::Float);
    builder.setChannelCount(m_channels);
    builder.setSampleRate(m_sampleRate);
    builder.setCallback(this);

    oboe::Result result = builder.openStream(m_audioStream);
    if (result != oboe::Result::OK) {
        // 处理错误
        std::cerr << "Failed to open audio stream: " << oboe::convertToText(result) << std::endl;
        return;
    }

    result = m_audioStream->requestStart();
    if (result == oboe::Result::OK) {
        m_state = AUDIO_STATE_PLAYING;
    } else {
        std::cerr << "Failed to start audio stream: " << oboe::convertToText(result) << std::endl;
    }
}

void UnityAudioPlayer::playWithDelay(const float delay) {
    m_delayTime = delay;
    m_delayCancelled = false;

    if (m_delayThread.joinable()) {
        m_delayThread.join();
    }

    m_delayThread = std::thread(&UnityAudioPlayer::scheduleDelayedPlay, this);
}

void UnityAudioPlayer::pause() {
    if (m_audioStream && m_state == AUDIO_STATE_PLAYING) {
        m_audioStream->pause();
        m_state = AUDIO_STATE_PAUSED;
    }
}

void UnityAudioPlayer::stop() {
    if (m_audioStream) {
        m_audioStream->stop();
        m_state = AUDIO_STATE_STOPPED;
        m_currentTime = 0.0f;
    }
}

void UnityAudioPlayer::unpause() {
    if (m_audioStream && m_state == AUDIO_STATE_PAUSED) {
        m_audioStream->start();
        m_state = AUDIO_STATE_PLAYING;
    }
}

float UnityAudioPlayer::getCurrentTime() const {
    return m_currentTime;
}

void UnityAudioPlayer::setCurrentTime(const float time) {
    m_currentTime = std::max(0.0f, std::min(time, getMusicLength()));
}

void UnityAudioPlayer::offsetTime(const float offset) {
    setCurrentTime(m_currentTime + offset);
}

void UnityAudioPlayer::resetTime() {
    m_currentTime = 0.0f;
}

void UnityAudioPlayer::restartTime() {
    m_currentTime = 0.0f;
    if (m_state == AUDIO_STATE_PLAYING) {
        stop();
        play();
    }
}

void UnityAudioPlayer::setVolume(const float volume) {
    m_volume = std::max(0.0f, std::min(1.0f, volume));
}

float UnityAudioPlayer::getVolume() const {
    return m_volume;
}

void UnityAudioPlayer::setLoop(const bool loop) {
    m_loop = loop;
}

bool UnityAudioPlayer::getLoop() const {
    return m_loop;
}

bool UnityAudioPlayer::isPlaying() const {
    return m_state == AUDIO_STATE_PLAYING;
}

AudioState UnityAudioPlayer::getState() const {
    return m_state;
}

oboe::DataCallbackResult UnityAudioPlayer::onAudioReady(
    oboe::AudioStream *audioStream,
    void *audioData,
    const int32_t numFrames) {

    if (m_state != AUDIO_STATE_PLAYING || m_interleavedData.empty()) {
        generateSilence(static_cast<float*>(audioData), numFrames, audioStream->getChannelCount());
        return oboe::DataCallbackResult::Continue;
    }

    // 计算每帧的时间增量
    const float frameTime = numFrames / static_cast<float>(audioStream->getSampleRate());

    auto output = static_cast<float*>(audioData);
    const int32_t outputChannels = audioStream->getChannelCount();

    // 计算当前帧位置
    const auto currentFrame = static_cast<int>(m_currentTime * m_sampleRate);
    const int framesRemaining = m_totalFrames - currentFrame;
    const int framesToCopy = std::min(numFrames, framesRemaining);

    if (framesToCopy > 0) {
        // 复制音频数据
        for (auto i = 0; i < framesToCopy; i++) {
            const int srcIndex = (currentFrame + i) * m_channels;

            for (auto c = 0; c < outputChannels; c++) {
                // 如果输出通道多于输入通道，循环使用输入通道
                const int srcChannel = c % m_channels;
                *output++ = m_interleavedData[srcIndex + srcChannel] * m_volume;
            }
        }
    }

    // 如果剩余帧需要填充
    if (framesToCopy < numFrames) {
        if (m_loop) {
            // 循环播放：从开头继续复制
            const int remainingFrames = numFrames - framesToCopy;
            for (auto i = 0; i < remainingFrames; i++) {
                const int srcIndex = (i % m_totalFrames) * m_channels;

                for (auto c = 0; c < outputChannels; c++) {
                    const int srcChannel = c % m_channels;
                    *output++ = m_interleavedData[srcIndex + srcChannel] * m_volume;
                }
            }
            m_currentTime = remainingFrames / static_cast<float>(m_sampleRate);
        } else {
            // 非循环：填充静音
            generateSilence(output, numFrames - framesToCopy, outputChannels);
            m_currentTime += frameTime;

            // 检查是否播放完毕
            if (m_currentTime >= getMusicLength()) {
                m_state = AUDIO_STATE_STOPPED;
                return oboe::DataCallbackResult::Stop;
            }
        }
    } else {
        m_currentTime += frameTime;
    }

    return oboe::DataCallbackResult::Continue;
}

void UnityAudioPlayer::onErrorAfterClose(oboe::AudioStream *audioStream, const oboe::Result error) {
    // 处理音频流错误
    m_state = AUDIO_STATE_STOPPED;
    std::cerr << "Audio stream error: " << oboe::convertToText(error) << std::endl;
}

std::vector<uint8_t> loadFileData(const std::string& filename) {
    std::vector<uint8_t> buffer;
    std::ifstream file(filename, std::ios::binary | std::ios::ate);

    if (!file.is_open()) {
        LOGE("Failed to open file: %s", filename.c_str());
        return buffer;
    }

    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    buffer.resize(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        LOGE("Failed to read file: %s", filename.c_str());
        buffer.clear();
    }

    return buffer;
}

bool UnityAudioPlayer::loadAudioData(const std::string& filePath) {
    try {
        // 确定文件路径是assets中的还是文件系统中的
        const bool isAsset = (filePath.find("assets/") == 0);
        std::vector<uint8_t> fileData;

        if (isAsset) {
            // 从assets加载
            const std::string assetPath = filePath.substr(7); // 去掉"assets/"前缀
            fileData = loadAssetData(assetPath);
        } else {
            // 从文件系统加载
            fileData = loadFileData(filePath);
        }

        if (fileData.empty()) {
            LOGE("Failed to load audio file: %s", filePath.c_str());
            return false;
        }

        m_audioData = std::make_unique<nqr::AudioData>();
        nqr::NyquistIO loader;

        // 从内存加载并解码音频
        loader.Load(m_audioData.get(), filePath, fileData);

        // 获取音频信息
        m_sampleRate = m_audioData->sampleRate;
        m_channels = m_audioData->channelCount;
        m_totalFrames = m_audioData->samples.size() / m_channels;

        // 将音频数据转换为交错格式（如果需要）
        m_interleavedData = std::move(m_audioData->samples);

        LOGI("Loaded audio: %s, SR: %d, Channels: %d, Frames: %d",
             filePath.c_str(), m_sampleRate, m_channels, m_totalFrames);

        return true;
    } catch (const std::exception& e) {
        LOGE("Failed to load audio data: %s", e.what());
        return false;
    }
}

float UnityAudioPlayer::getMusicLength() const {
    return m_totalFrames / static_cast<float>(m_sampleRate);
}

void UnityAudioPlayer::scheduleDelayedPlay() {
    std::unique_lock<std::mutex> lock(m_delayMutex);
    if (m_delayCondition.wait_for(lock,
                                  std::chrono::milliseconds(static_cast<int>(m_delayTime * 1000)),
                                  [this] { return m_delayCancelled.load(); })) {
        // 延迟被取消
        return;
    }

    if (!m_delayCancelled) {
        play();
    }
}

void UnityAudioPlayer::generateSilence(float* buffer, const int32_t numFrames, const int32_t channels) {
    for (auto i = 0; i < numFrames * channels; i++) {
        buffer[i] = 0.0f;
    }
}

void UnityAudioPlayer::generateSineWave(float* buffer, const int32_t numFrames, const int32_t channels, const float frequency) {
    for (auto i = 0; i < numFrames; i++) {
        const float sample = 0.5f * sin(2.0f * M_PI * frequency * (m_currentTime + i / static_cast<float>(m_sampleRate)));
        for (auto c = 0; c < channels; c++) {
            buffer[i * channels + c] = sample * m_volume;
        }
    }
}

// C接口函数实现
void* Create() {
    auto* player = new UnityAudioPlayer();
    const auto handle = reinterpret_cast<void*>(g_nextPlayerId++);
    g_audioPlayers[handle] = player;
    return handle;
}

void Destroy(void* player) {
    const auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        delete it->second;
        g_audioPlayers.erase(it);
    }
}

void Play(void* player) {
    const auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->play();
    }
}

void PlayWithDelay(void* player, const float delay) {
    const auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->playWithDelay(delay);
    }
}

void Pause(void* player) {
    const auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->pause();
    }
}

void Stop(void* player) {
    const auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->stop();
    }
}

void UnPause(void* player) {
    const auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->unpause();
    }
}

float GetCurrentTime(void* player) {
    const auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        return it->second->getCurrentTime();
    }
    return 0.0f;
}

void SetCurrentTime(void* player, const float time) {
    const auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->setCurrentTime(time);
    }
}

void OffsetTime(void* player, const float offset) {
    const auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->offsetTime(offset);
    }
}

void ResetTime(void* player) {
    const auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->resetTime();
    }
}

void RestartTime(void* player) {
    const auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->restartTime();
    }
}

void SetClip(void* player, const char* clipPath) {
    const auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->setClip(clipPath);
    }
}

void SetVolume(void* player, const float volume) {
    const auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->setVolume(volume);
    }
}

float GetVolume(void* player) {
    const auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        return it->second->getVolume();
    }
    return 0.0f;
}

void SetLoop(void* player, const bool loop) {
    const auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        it->second->setLoop(loop);
    }
}

bool GetLoop(void* player) {
    const auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        return it->second->getLoop();
    }
    return false;
}

bool IsPlaying(void* player) {
    const auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        return it->second->isPlaying();
    }
    return false;
}

AudioState GetState(void* player) {
    const auto it = g_audioPlayers.find(player);
    if (it != g_audioPlayers.end()) {
        return it->second->getState();
    }
    return AUDIO_STATE_IDLE;
}
