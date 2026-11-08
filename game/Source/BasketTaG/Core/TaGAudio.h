#pragma once

#include "CoreMinimal.h"

class UObject;

/**
 * Sound effects live at /Game/Audio/SFX/<Name> (synthesized by tools/audio). Missing sounds are silently skipped,
 * so the game runs before the assets exist.
 */
namespace TaGAudio
{
	void PlayAt(const UObject* WorldContext, FName Sound, const FVector& Location, float Volume = 1.f);
	void Play2D(const UObject* WorldContext, FName Sound, float Volume = 1.f);
	void PlayAmbient(const UObject* WorldContext);
}
