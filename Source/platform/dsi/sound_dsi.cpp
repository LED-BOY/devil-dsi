#include "engine/sound.h"
#include "engine/assets.hpp"
#include "engine/sound_defs.hpp"
#include "options.h"
#include "platform/dsi/dsi_audio.h"
#include "platform/dsi/dsi_trace.h"
#include <SDL.h>
#include <utility>
#include <algorithm>

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
namespace {
bool musicEnabled, suspended, muted;
const char *const SpawnMusicTracks[NUM_MUSIC] = {
	"music\\stowne.wav",
	"music\\slvla.wav",
	"music\\slvla.wav",
	"music\\slvla.wav",
	"music\\slvla.wav",
	"music\\dlvlf.wav",
	"music\\dlvle.wav",
	"music\\sintro.wav",
};
/** Maps from track ID to track name. */
const char *const MusicTracks[NUM_MUSIC] = {
	"music\\dtowne.wav",
	"music\\dlvla.wav",
	"music\\dlvlb.wav",
	"music\\dlvlc.wav",
	"music\\dlvld.wav",
	"music\\dlvlf.wav",
	"music\\dlvle.wav",
	"music\\dintro.wav",
};

unsigned MusicVolume()
{
	const int volume = std::clamp(*GetOptions().Audio.musicVolume, VOLUME_MIN, VOLUME_MAX);
	return muted ? 0 : static_cast<unsigned>((volume - VOLUME_MIN) * 1024 / (VOLUME_MAX - VOLUME_MIN));
}

void ResumeTrack()
{
	if (!gbSndInited || !musicEnabled || !gbMusicOn || suspended || sgnMusicTrack >= NUM_MUSIC) return;
	const char *path = HaveFullMusic() ? MusicTracks[sgnMusicTrack] : SpawnMusicTracks[sgnMusicTrack];
	if (!DsiMusicOpen(path, MusicVolume()))
		DsiTrace("music: unable to open or decode %s", path);
}
} // namespace

extern "C" void DsiApplyMusicSetting()
{
	const bool enabled = *GetOptions().Dsi.music;
	if (enabled == musicEnabled) return;
	musicEnabled = enabled;
	gbMusicOn = enabled && *GetOptions().Audio.musicVolume > VOLUME_MIN;
	if (enabled) ResumeTrack(); else DsiMusicClose();
}

void snd_init()
{
	if (*GetOptions().Dsi.soundEffects) DsiAudioInit();
	gbSndInited = true;
	DsiApplyMusicSetting();
}
void snd_deinit() { music_stop(); suspended = muted = false; DsiAudioQuit(); gbSndInited = false; }
void ClearDuplicateSounds() { }
void music_stop()
{
	DsiMusicClose();
	sgnMusicTrack = NUM_MUSIC;
}
void music_start(_music_id track)
{
	music_stop();
	sgnMusicTrack = track;
	ResumeTrack();
}
void sound_disable_music(bool disable)
{
	if (suspended == disable) return;
	suspended = disable;
	if (disable) DsiMusicClose(); else ResumeTrack();
}
void music_mute() { muted = true; DsiMusicVolume(0); }
void music_unmute() { muted = false; DsiMusicVolume(MusicVolume()); }
_music_id GetLevelMusic(dungeon_type dungeonType)
{
	switch (dungeonType) {
	case DTYPE_TOWN:
		return TMUSIC_TOWN;
	case DTYPE_CATHEDRAL:
		return TMUSIC_CATHEDRAL;
	case DTYPE_CATACOMBS:
		return TMUSIC_CATACOMBS;
	case DTYPE_CAVES:
		return TMUSIC_CAVES;
	case DTYPE_HELL:
		return TMUSIC_HELL;
	case DTYPE_NEST:
		return TMUSIC_NEST;
	case DTYPE_CRYPT:
		return TMUSIC_CRYPT;
	default:
		return TMUSIC_INTRO;
	}
}
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
	if (volume != 1) {
		GetOptions().Audio.musicVolume.SetValue(std::clamp(volume, VOLUME_MIN, VOLUME_MAX));
		DsiMusicVolume(MusicVolume());
	}
	return *GetOptions().Audio.musicVolume;
}
} // namespace devilution
