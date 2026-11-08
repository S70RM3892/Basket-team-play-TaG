#pragma once

#include "CoreMinimal.h"

class UObject;

/**
 * Sound effects live at /Game/Audio/SFX/<Name>, music at /Game/Audio/Music/<Name> (synthesized by tools/audio).
 * Missing sounds are silently skipped, so the game runs before the assets exist.
 */
namespace TaGAudio
{
	void PlayAt(const UObject* WorldContext, FName Sound, const FVector& Location, float Volume = 1.f);
	void Play2D(const UObject* WorldContext, FName Sound, float Volume = 1.f);
	void PlayAmbient(const UObject* WorldContext);

	/** Cross-fades to a looping music track (MusicMenu / MusicMatch). Same name again is a no-op. */
	void PlayMusic(const UObject* WorldContext, FName Track);
	void SetMusicVolume(float Volume);
	/** Overall game volume (0..1) applied to the main audio device. */
	void SetMasterVolume(const UObject* WorldContext, float Volume);
}
