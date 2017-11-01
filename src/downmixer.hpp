#ifndef DOWNMIXER_HPP
#define DOWNMIXER_HPP

#include <map>
#include <vector>

#include "allocore/io/al_AudioIOData.hpp"

class DownMixer {
public:
    DownMixer() {
        mDownMixMap[0] = vector<int>();
        mDownMixMap[1] = vector<int>();
        for (int i = 0; i < 60; i++) {
            mDownMixMap[0].push_back(i);
            mDownMixMap[1].push_back(i);
        }
        mGain = 1/1.0;
    }

    void process(AudioIOData &io) {
        int counter;
        for (auto &entry: mDownMixMap) {
            for (int sourceIndex: entry.second) {
                float *destBuffer = io.outBuffer(entry.first);
                float *srcBuffer = io.outBuffer(sourceIndex);
                counter = io.framesPerBuffer();
                while (counter--) {
                    *destBuffer++ += *srcBuffer++;
                }
            }
            float *destBuffer = io.outBuffer(entry.first);
            counter = io.framesPerBuffer();
            while (counter--) {
                *destBuffer++ *= mGain;
            }
        }
    }

private:
    map<int, vector<int>> mDownMixMap;
    float mGain;
};

#endif // DOWNMIXER_HPP
