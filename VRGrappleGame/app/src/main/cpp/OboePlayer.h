//#ifndef OBOEPLAYER_H
#define OBOEPLAYER_H
#define DR_MP3_IMPLEMENTATION

#include <math.h>
#include <algorithm>
#include <oboe/Oboe.h>
#include "dr_mp3.h"


#define NOTE_NUM   12 * 4

struct Mp3Sound {
    std::vector<float> samples;
    uint32_t sampleRate = 0;
    uint32_t channels = 1;
    double durationSeconds = 0.0;

};


Mp3Sound loadMp3File(const char* filename) {
    Mp3Sound result;

    drmp3 mp3;
    if (!drmp3_init_file(&mp3, filename, NULL)) {
        LOGE("Failed to load MP3");
        return result;  // Empty
    }

    drmp3_uint64 frameCount = drmp3_get_pcm_frame_count(&mp3);
    result.samples.resize(frameCount * mp3.channels);

    drmp3_read_pcm_frames_f32(&mp3, frameCount, result.samples.data());

    result.sampleRate = mp3.sampleRate;
    result.channels = mp3.channels;
    result.durationSeconds = static_cast<double>(frameCount) / static_cast<double>(mp3.sampleRate);


    drmp3_uninit(&mp3);

    return result;
}


class OboeMp3Player : public oboe::AudioStreamCallback {
public:
    OboeMp3Player(const std::vector<float>& samples, uint32_t sampleRate, uint32_t channels)
            : mSamples(samples), mChannelCount(channels)
    {
        oboe::AudioStreamBuilder builder;
        builder.setPerformanceMode(oboe::PerformanceMode::LowLatency)
                ->setSharingMode(oboe::SharingMode::Exclusive)
                ->setFormat(oboe::AudioFormat::Float)
                ->setSampleRate(sampleRate)
                ->setChannelCount(channels)
                ->setCallback(this);

        builder.openManagedStream(mStream);
        mStream->requestStart();
    }

    void play() {
        mReadIndex = 0;
        mPlaying = true;
        Loop = false;
    }
    void loop() {
        mReadIndex = 0;
        mPlaying = true;
        Loop = true;
    }

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream*, void* audioData, int32_t numFrames) override {
        float* out = static_cast<float*>(audioData);
        float accumulatedEnergy = 0.0f;
        int sampleCount = 0;

        for (int i = 0; i < numFrames * mChannelCount; ++i) {
            if (mPlaying) {
                if (mReadIndex >= mSamples.size()) {
                    if (Loop) {
                        mReadIndex = 0;  // Safe: loop to beginning
                    } else {
                        mPlaying = false;
                        out[i] = 0.0f;
                        continue;  // skip sample write
                    }
                }
                float sample = mSamples[mReadIndex++];
                out[i] = sample;
                //accumulate the sound energy of the song to average it. So you don't get seizure inducing pulsing.
                accumulatedEnergy += fabsf(sample); // Use abs value
                sampleCount++;
            } else {
                out[i] = 0.0f;
            }
        }

        if (sampleCount > 0) {
            float targetBrightness = accumulatedEnergy / (float)sampleCount;

            const float smoothing = 0.9f;
            MusicBrightness = smoothing * MusicBrightness + (1.0f - smoothing) * targetBrightness;
        } else {
            MusicBrightness = 0.0f;
        }
        return oboe::DataCallbackResult::Continue;
    }

public:
    float MusicBrightness;
private:
    oboe::ManagedStream mStream;
    std::vector<float> mSamples;
    uint32_t mChannelCount;
    size_t mReadIndex = 0;
    bool mPlaying = false;
    bool Loop = false;
};
