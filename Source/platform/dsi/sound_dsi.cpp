#include "engine/sound.h"
#include "engine/assets.hpp"
#include "engine/sound_defs.hpp"
#include "options.h"
#include "platform/dsi/dsi_audio.h"
#include "platform/dsi/dsi_trace.h"
#include <SDL.h>
#include <utility>

extern "C" SDL_RWops *DsiOpenAudioAsset(const char *path)
{
	auto ref = devilution::FindAsset(path);
	if (!ref.ok()) return nullptr;
	auto handle = devilution::OpenAsset(std::move(ref));
	if (!handle.ok()) return nullptr;
	return std::move(handle).release();
}

namespace devilution {
bool gbSndInited;
bool gbSoundOn = true;
bool gbMusicOn = false;
_music_id sgnMusicTrack = NUM_MUSIC;
TSnd::~TSnd() = default;

std::expected<std::unique_ptr<TSnd>, std::string> SoundFileLoadWithStatus(const char *path, bool stream)
{
	(void)stream;
	auto sound = std::make_unique<TSnd>();
	sound->start_tc = 0;
	sound->DSB.SetPath(path);
	return sound;
}
std::unique_ptr<TSnd> sound_file_load(const char *path, bool stream)
{
	return std::move(*SoundFileLoadWithStatus(path, stream));
}
void snd_play_snd(TSnd *sound, int volume, int pan, int userVolume)
{
	if (!gbSndInited || !gbSoundOn || !sound) return;
	const auto now = SDL_GetTicks();
	if (now - sound->start_tc < 80) return;
	if (sound->DSB.PlayWithVolumeAndPan(volume, userVolume, pan)) sound->start_tc = now;
}
void snd_init()
{
	if (*GetOptions().Dsi.soundEffects) DsiAudioInit();
	gbSndInited = true;
	DsiTrace("experimental hardware SFX: 16 PCM voices, 1 MiB cache; music disabled");
}
void snd_deinit() { DsiAudioQuit(); gbSndInited = false; }
void ClearDuplicateSounds() { }
void music_stop() { sgnMusicTrack = NUM_MUSIC; }
void music_start(_music_id track) { (void)track; }
void sound_disable_music(bool disable) { (void)disable; }
void music_mute() { }
void music_unmute() { }
_music_id GetLevelMusic(dungeon_type type) { (void)type; return TMUSIC_TOWN; }
int sound_get_or_set_sound_volume(int volume)
{
	if (volume != 1) GetOptions().Audio.soundVolume.SetValue(volume);
	return *GetOptions().Audio.soundVolume;
}
int SoundGetOrSetAudioCuesVolume(int volume)
{
	if (volume != 1) GetOptions().Audio.audioCuesVolume.SetValue(volume);
	return *GetOptions().Audio.audioCuesVolume;
}
int sound_get_or_set_music_volume(int volume)
{
	(void)volume;
	return VOLUME_MIN; // Music is deliberately unavailable in this effects-only backend.
}
} // namespace devilution
