//    SAPF - Sound As Pure Form
//    Copyright (C) 2019 James McCartney
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "Play.hpp"
#ifdef __APPLE__
#include <AudioToolbox/AudioToolbox.h>
#else
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#endif
#include <pthread.h>
#include <atomic>

#include "SoundFiles.hpp"

pthread_mutex_t gPlayerMutex = PTHREAD_MUTEX_INITIALIZER;

const int kMaxChannels = 32;

struct AUPlayer* gAllPlayers = nullptr;

#ifdef __APPLE__

struct AUPlayer
{
	AUPlayer(Thread& inThread, int inNumChannels, ExtAudioFileRef inXAFRef = nullptr)
		: th(inThread), count(0), done(false), prev(nullptr), next(gAllPlayers), outputUnit(nullptr), numChannels(inNumChannels), xaf(inXAFRef)
	{ 
		gAllPlayers = this; 
		if (next) next->prev = this; 
	}
	
	~AUPlayer() {
		if (next) next->prev = prev;

		if (prev) prev->next = next;
		else gAllPlayers = next;
		
		if (xaf) {
			ExtAudioFileDispose(xaf);
			char cmd[1100];
			snprintf(cmd, 1100, "open \"%s\"", path.c_str());
			system(cmd);
		}
	}
	
	Thread th;
	int count;
	bool done;
	AUPlayer* prev;
	AUPlayer* next;
	AudioComponentInstance outputUnit;
	int numChannels;
	ZIn in[kMaxChannels];
	ExtAudioFileRef xaf = nullptr;
	std::string path;
	
};

static void stopPlayer(AUPlayer* player)
{
	AudioComponentInstance outputUnit = player->outputUnit;
	player->outputUnit = nullptr;
	if (outputUnit) {
	
		OSStatus err = AudioOutputUnitStop(outputUnit);
		if (err) post("AudioOutputUnitStop err %d\n", (int)err);
		err = AudioComponentInstanceDispose(outputUnit);
		if (err) post("AudioComponentInstanceDispose outputUnit err %d\n", (int)err);
		
	}
	delete player;
}

void stopPlaying()
{
	Locker lock(&gPlayerMutex);

	AUPlayer* player = gAllPlayers;
	while (player) {
		AUPlayer* next = player->next;
		stopPlayer(player);
		player = next;
	}
}

void stopPlayingIfDone()
{
	Locker lock(&gPlayerMutex);
	
	AUPlayer* player = gAllPlayers;
	while (player) {
		AUPlayer* next = player->next;
		if (player->done)
			stopPlayer(player);
		player = next;
	}
}

static void* stopDonePlayers(void* x)
{
	while(1) {
		sleep(1);
		stopPlayingIfDone();
	}
	return nullptr;
}

bool gWatchdogRunning = false;
pthread_t watchdog;

static bool fillBufferList(AUPlayer* player, int inNumberFrames, AudioBufferList* ioData)
{
	if (player->done) {
zeroAll:
		for (int i = 0; i < (int)ioData->mNumberBuffers; ++i) {
			memset((float*)ioData->mBuffers[i].mData, 0, inNumberFrames * sizeof(float));
		}
		return true;
	}
	ZIn* in = player->in;
	bool done = true;
	for (int i = 0; i < (int)ioData->mNumberBuffers; ++i) {
		int n = inNumberFrames;
		if (i >= player->numChannels) {
			memset(ioData->mBuffers[i].mData, 0, ioData->mBuffers[i].mDataByteSize);
		} else {
			try {
				float* buf = (float*)ioData->mBuffers[i].mData;
				bool imdone = in[i].fill(player->th, n, buf, 1);
				if (n < inNumberFrames) {
					memset((float*)ioData->mBuffers[i].mData + n, 0, (inNumberFrames - n) * sizeof(float));
				}
				done = done && imdone;
			} catch (int err) {
				if (err <= -1000 && err > -1000 - kNumErrors) {
					post("\nerror: %s\n", errString[-1000 - err]);
				} else {
					post("\nerror: %d\n", err);
				}
				post("exception in real time. stopping player.\n");
				done = true;
				goto zeroAll;
			} catch (std::bad_alloc& xerr) {
				post("\nnot enough memory\n");
				post("exception in real time. stopping player.\n");
				done = true;
				goto zeroAll;
			} catch (...) {
				post("\nunknown error\n");
				post("exception in real time. stopping player.\n");
				done = true;
				goto zeroAll;
			}
		}
	}
	
	return done;
}

static void recordPlayer(AUPlayer* player, int inNumberFrames, AudioBufferList const* inData)
{
	if (!player->xaf) return;
		
	OSStatus err = ExtAudioFileWriteAsync(player->xaf, inNumberFrames, inData); // initialize async.
	if (err) printf("ExtAudioFileWriteAsync err %d\n", (int)err);
}

static OSStatus inputCallback(	void *							inRefCon,
						AudioUnitRenderActionFlags *	ioActionFlags,
						const AudioTimeStamp *			inTimeStamp,
						UInt32							inBusNumber,
						UInt32							inNumberFrames,
						AudioBufferList *				ioData)
{
	
	AUPlayer* player = (AUPlayer*)inRefCon;
		
	bool done = fillBufferList(player, inNumberFrames, ioData);
	recordPlayer(player, inNumberFrames, ioData);

	if (done) {
		player->done = true;
	}
	return noErr;
}


static AudioComponentInstance openAU(UInt32 inType, UInt32 inSubtype, UInt32 inManuf)
{
    AudioComponentDescription desc;
    desc.componentType = inType;
    desc.componentSubType = inSubtype;
    desc.componentManufacturer = inManuf;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
	if (!comp) {
		return nullptr;
	}

    AudioComponentInstance au = nullptr;
    AudioComponentInstanceNew(comp, &au);
	
	return au;
}

static OSStatus createGraph(AUPlayer* player)
{
    OSStatus err = noErr;
	AudioComponentInstance outputUnit = openAU('auou', 'def ', 'appl');
	if (!outputUnit) {
		post("open output unit failed\n");
		return 'fail';
	}
	
	player->outputUnit = outputUnit;
	
	UInt32 flags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
	AudioStreamBasicDescription fmt = { vm.ar.sampleRate, kAudioFormatLinearPCM, flags, 4, 1, 4, (UInt32)player->numChannels, 32, 0 };
	
	err = AudioUnitSetProperty(outputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &fmt, sizeof(fmt));
	if (err) {
		post("set outputUnit client format failed\n");
		return err;
	}
	
	AURenderCallbackStruct cbs;
		
	cbs.inputProc = inputCallback;
	cbs.inputProcRefCon = player;
	
	err = AudioUnitSetProperty(outputUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cbs, sizeof(cbs));
	if (err) {
		post("set render callback failed\n");
		return err;
	}
	
	err = AudioUnitInitialize(outputUnit);
	if (err) {
		post("initialize output unit failed\n");
		return err;
	}
	
	err = AudioOutputUnitStart(outputUnit);
	if (err) {
		post("start output unit failed\n");
		return err;
	}
	
	post("start output unit OK\n");
	
	return noErr;
}

void playWithAudioUnit(Thread& th, V& v)
{
	if (!v.isList()) wrongType("play : s", "List", v);

	Locker lock(&gPlayerMutex);
	
	AUPlayer *player;
	
	if (v.isZList()) {
		player = new AUPlayer(th, 1);
		player->in[0].set(v);
		player->numChannels = 1;
	} else {
		if (!v.isFinite()) indefiniteOp("play : s", "");
		P<List> s = (List*)v.o();
		s = s->pack(th, kMaxChannels);
		if (!s()) {
			post("Too many channels. Max is %d.\n", kMaxChannels);
			return;
		}
		Array* a = s->mArray();
		
		int asize = (int)a->size();
		
		player = new AUPlayer(th, asize);
		for (int i = 0; i < asize; ++i) {
			player->in[i].set(a->at(i));
		}
		s = nullptr;
		a = nullptr;
	}
	v.o = nullptr; // try to prevent leak.
    
    std::atomic_thread_fence(std::memory_order_seq_cst);
		
	if (!gWatchdogRunning) {
		pthread_create(&watchdog, nullptr, stopDonePlayers, nullptr);
		gWatchdogRunning = true;
	}

	{
		OSStatus err = noErr;
		err = createGraph(player);
		if (err) {
			post("play failed: %d '%4.4s'\n", (int)err, (char*)&err);
			throw errFailed;
		}
	}
}


void recordWithAudioUnit(Thread& th, V& v, Arg filename)
{
	if (!v.isList()) wrongType("play : s", "List", v);

	Locker lock(&gPlayerMutex);
	
	AUPlayer *player;

	char path[1024];
	ExtAudioFileRef xaf = nullptr;
	
	if (v.isZList()) {
		makeRecordingPath(filename, path, 1024);
		xaf = sfcreate(th, path, 1, 0., false);
		if (!xaf) {
			printf("couldn't create recording file \"%s\"\n", path);
			return;
		}

		player = new AUPlayer(th, 1, xaf);
		player->in[0].set(v);
		player->numChannels = 1;
	} else {
		if (!v.isFinite()) indefiniteOp("play : s", "");
		P<List> s = (List*)v.o();
		s = s->pack(th, kMaxChannels);
		if (!s()) {
			post("Too many channels. Max is %d.\n", kMaxChannels);
			return;
		}
		Array* a = s->mArray();
		
		int numChannels = (int)a->size();

		makeRecordingPath(filename, path, 1024);
		xaf = sfcreate(th, path, numChannels, 0., false);
		if (!xaf) {
			printf("couldn't create recording file \"%s\"\n", path);
			return;
		}
		
		player = new AUPlayer(th, numChannels, xaf);
		for (int i = 0; i < numChannels; ++i) {
			player->in[i].set(a->at(i));
		}
		s = nullptr;
		a = nullptr;
	}
	v.o = nullptr; // try to prevent leak.

	player->path = path;

	{
		OSStatus err = ExtAudioFileWriteAsync(xaf, 0, nullptr); // initialize async.
		if (err) printf("init ExtAudioFileWriteAsync err %d\n", (int)err);
    }
	
    std::atomic_thread_fence(std::memory_order_seq_cst);
		
	if (!gWatchdogRunning) {
		pthread_create(&watchdog, nullptr, stopDonePlayers, nullptr);
		gWatchdogRunning = true;
	}

	{
		OSStatus err = noErr;
		err = createGraph(player);
		if (err) {
			post("play failed: %d '%4.4s'\n", (int)err, (char*)&err);
			throw errFailed;
		}
	}
}

#else
// Linux Implementation

struct AUPlayer
{
    AUPlayer(Thread& inThread, int inNumChannels)
        : th(inThread), count(0), done(false), prev(nullptr), next(gAllPlayers), numChannels(inNumChannels)
    { 
        gAllPlayers = this; 
        if (next) next->prev = this; 
        
        memset(&device, 0, sizeof(device));
    }
    
    ~AUPlayer() {
        if (next) next->prev = prev;

        if (prev) prev->next = next;
        else gAllPlayers = next;
        
        ma_device_uninit(&device);
    }
    
    Thread th;
    int count;
    bool done;
    AUPlayer* prev;
    AUPlayer* next;
    ma_device device;
    int numChannels;
    ZIn in[kMaxChannels];
};

static void stopPlayer(AUPlayer* player)
{
    ma_device_stop(&player->device);
    delete player;
}

void stopPlaying()
{
    Locker lock(&gPlayerMutex);

    AUPlayer* player = gAllPlayers;
    while (player) {
        AUPlayer* next = player->next;
        stopPlayer(player);
        player = next;
    }
}

void stopPlayingIfDone()
{
    Locker lock(&gPlayerMutex);
    
    AUPlayer* player = gAllPlayers;
    while (player) {
        AUPlayer* next = player->next;
        if (player->done)
            stopPlayer(player);
        player = next;
    }
}

static void* stopDonePlayers(void* x)
{
    while(1) {
        sleep(1);
        stopPlayingIfDone();
    }
    return nullptr;
}

bool gWatchdogRunning = false;
pthread_t watchdog;

static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    AUPlayer* player = (AUPlayer*)pDevice->pUserData;
    float* out = (float*)pOutput;
    
    if (player->done) {
        memset(out, 0, frameCount * player->numChannels * sizeof(float));
        return;
    }
    
    bool done = true;
    for (int i = 0; i < player->numChannels; ++i) {
        try {
            int n = frameCount;
            bool imdone = player->in[i].fill(player->th, n, out + i, player->numChannels);
            if (n < frameCount) {
                // Zero fill remaining
                for (int j=n; j<frameCount; ++j) {
                    out[j * player->numChannels + i] = 0.0f;
                }
            }
            done = done && imdone;
        } catch(...) {
            done = true;
        }
    }
    
    if (done) player->done = true;
}

void playWithAudioUnit(Thread& th, V& v)
{
    if (!v.isList()) wrongType("play : s", "List", v);

    Locker lock(&gPlayerMutex);
    
    AUPlayer *player;
    
    if (v.isZList()) {
        player = new AUPlayer(th, 1);
        player->in[0].set(v);
        player->numChannels = 1;
    } else {
        if (!v.isFinite()) indefiniteOp("play : s", "");
        P<List> s = (List*)v.o();
        s = s->pack(th, kMaxChannels);
        if (!s()) {
            post("Too many channels. Max is %d.\n", kMaxChannels);
            return;
        }
        Array* a = s->mArray();
        
        int asize = (int)a->size();
        
        player = new AUPlayer(th, asize);
        for (int i = 0; i < asize; ++i) {
            player->in[i].set(a->at(i));
        }
        s = nullptr;
        a = nullptr;
    }
    v.o = nullptr;
    
    std::atomic_thread_fence(std::memory_order_seq_cst);
        
    if (!gWatchdogRunning) {
        pthread_create(&watchdog, nullptr, stopDonePlayers, nullptr);
        gWatchdogRunning = true;
    }

    {
        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.format   = ma_format_f32;
        config.playback.channels = player->numChannels;
        config.sampleRate        = (ma_uint32)vm.ar.sampleRate;
        config.dataCallback      = data_callback;
        config.pUserData         = player;
        
        if (ma_device_init(NULL, &config, &player->device) != MA_SUCCESS) {
            post("Failed to init audio device\n");
            delete player;
            return;
        }
        
        if (ma_device_start(&player->device) != MA_SUCCESS) {
            post("Failed to start audio device\n");
            delete player;
            return;
        }
    }
}

void recordWithAudioUnit(Thread& th, V& v, Arg filename)
{
    post("recordWithAudioUnit not implemented on Linux\n");
}

#endif

