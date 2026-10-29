#pragma once
#include <string>
#include "platform/dsi/dsi_audio.h"
namespace devilution {
class SoundSample final {
public:
	SoundSample() = default;
	SoundSample(const SoundSample &) = delete;
	SoundSample &operator=(const SoundSample &) = delete;
	~SoundSample() { Stop(); }
	void SetPath(const char *path) { path_ = path; }
	bool IsLoaded() const { return !path_.empty(); }
	bool IsPlaying() { return DsiAudioIsPlaying(this) != 0; }
	bool PlayWithVolumeAndPan(int attenuation, int userVolume, int pan) { return DsiAudioPlay(this, path_.c_str(), attenuation, pan, userVolume) != 0; }
	void Stop() { DsiAudioStop(this); }
	int GetLength() const { return DsiAudioLength(path_.c_str()); }
private:
	std::string path_;
};
} // namespace devilution
