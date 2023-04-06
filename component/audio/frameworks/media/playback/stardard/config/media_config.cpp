/*
 * Copyright (c) 2021 Realtek, LLC.
 * All rights reserved.
 *
 * Licensed under the Realtek License, Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License from Realtek
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstddef>

#include "media/config/media_config.h"

// ----------------------------------------------------------------------
//Defines
void* GetWAVExtractorType();

void* GetMP3ExtractorType();

void* GetAACExtractorType();

void* GetMPEG4ExtractorType();

void* GetFLACExtractorType();

void *CreatePCMDec(
        const char *name, const void *callbacks,
        void *appData, void **component);

void *CreateMP3Dec(
        const char *name, const void *callbacks,
        void *appData, void **component);

void *CreateHAACDec(
        const char *name, const void *callbacks,
        void *appData, void **component);

// ----------------------------------------------------------------------
//MediaConfig
MediaConfig kMediaConfigs[] = {
#if defined(MEDIA_DEMUX_WAV) && defined(MEDIA_CODEC_PCM)
    { "wav", GetWAVExtractorType(), CreatePCMDec },
#endif
#if defined(MEDIA_DEMUX_MP3) && defined(MEDIA_CODEC_MP3)
    { "mp3", GetMP3ExtractorType(), CreateMP3Dec },
#endif
#if defined(MEDIA_DEMUX_AAC) && defined(MEDIA_CODEC_HAAC)
    { "aac", GetAACExtractorType(), CreateHAACDec },
#endif
#if defined(MEDIA_DEMUX_MP4) && defined(MEDIA_CODEC_HAAC)
    { "mp4", GetMPEG4ExtractorType(), CreateHAACDec },
#endif
#if defined(MEDIA_DEMUX_FLAC) && defined(MEDIA_CODEC_PCM)
    { "flac", GetFLACExtractorType(), CreatePCMDec },
#endif
};

size_t kNumMediaConfigs =
    sizeof(kMediaConfigs) / sizeof(kMediaConfigs[0]);
