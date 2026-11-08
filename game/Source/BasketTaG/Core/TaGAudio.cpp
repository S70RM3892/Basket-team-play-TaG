#include "Core/TaGAudio.h"
#include "BasketTaG.h"
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"

namespace
{
	TMap<FName, TWeakObjectPtr<USoundBase>>& Cache()
	{
		static TMap<FName, TWeakObjectPtr<USoundBase>> C;
		return C;
	}

	USoundBase* Find(FName Name, const TCHAR* Folder = TEXT("SFX"))
	{
		if (Name.IsNone())
		{
			return nullptr;
		}
		if (TWeakObjectPtr<USoundBase>* Found = Cache().Find(Name))
		{
			if (Found->IsValid())
			{
				return Found->Get();
			}
		}
		const FString N = Name.ToString();
		const FString Path = FString::Printf(TEXT("/Game/Audio/%s/%s.%s"), Folder, *N, *N);
		USoundBase* S = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
		Cache().Add(Name, S);
		return S;
	}

	USoundAttenuation* Attenuation()
	{
		static TWeakObjectPtr<USoundAttenuation> A;
		if (!A.IsValid())
		{
			USoundAttenuation* New = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("TaGAttenuation"));
			New->AddToRoot();
			New->Attenuation.bAttenuate = true;
			New->Attenuation.bSpatialize = true;
			New->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
			New->Attenuation.AttenuationShapeExtents = FVector(400.f, 0.f, 0.f);
			New->Attenuation.FalloffDistance = 3600.f;
			A = New;
		}
		return A.Get();
	}

	struct FMusicState
	{
		TWeakObjectPtr<UAudioComponent> Comp;
		FName Track;
		float Volume = 0.6f;
	};

	FMusicState& Music()
	{
		static FMusicState M;
		return M;
	}

	bool CanPlay(const UObject* WorldContext)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		return World && World->GetNetMode() != NM_DedicatedServer;
	}
}

namespace TaGAudio
{
	void PlayAt(const UObject* WorldContext, FName Sound, const FVector& Location, float Volume, float Pitch)
	{
		if (!CanPlay(WorldContext))
		{
			return;
		}
		if (USoundBase* S = Find(Sound))
		{
			UGameplayStatics::PlaySoundAtLocation(WorldContext, S, Location, FRotator::ZeroRotator, Volume, Pitch, 0.f, Attenuation());
		}
	}

	void Play2D(const UObject* WorldContext, FName Sound, float Volume)
	{
		if (!CanPlay(WorldContext))
		{
			return;
		}
		if (USoundBase* S = Find(Sound))
		{
			UGameplayStatics::PlaySound2D(WorldContext, S, Volume);
		}
	}

	void PlayAmbient(const UObject* WorldContext)
	{
		if (!CanPlay(WorldContext))
		{
			return;
		}
		if (USoundBase* S = Find(TEXT("AmbientLoop")))
		{
			if (USoundWave* W = Cast<USoundWave>(S))
			{
				W->bLooping = true;
			}
			UGameplayStatics::SpawnSound2D(WorldContext, S, 0.3f, 1.f, 0.f, nullptr, true, false);
		}
	}

	void PlayMusic(const UObject* WorldContext, FName Track)
	{
		FMusicState& M = Music();
		if (!CanPlay(WorldContext) || (M.Track == Track && M.Comp.IsValid()))
		{
			return;
		}
		if (M.Comp.IsValid())
		{
			M.Comp->FadeOut(1.2f, 0.f);
		}
		M.Track = Track;
		USoundBase* S = Find(Track, TEXT("Music"));
		UE_LOG(LogTaG, Display, TEXT("Music -> %s (%s)"), *Track.ToString(), S ? TEXT("playing") : TEXT("missing"));
		if (!S)
		{
			M.Comp = nullptr;
			return;
		}
		if (USoundWave* W = Cast<USoundWave>(S))
		{
			W->bLooping = true;
		}
		UAudioComponent* C = UGameplayStatics::SpawnSound2D(WorldContext, S, M.Volume * 0.55f, 1.f, 0.f, nullptr, false, true);
		if (C)
		{
			C->FadeIn(1.2f, M.Volume * 0.55f);
		}
		M.Comp = C;
	}

	void SetMusicVolume(float Volume)
	{
		FMusicState& M = Music();
		M.Volume = FMath::Clamp(Volume, 0.f, 1.f);
		if (M.Comp.IsValid())
		{
			M.Comp->SetVolumeMultiplier(M.Volume * 0.55f);
		}
	}

	void SetMasterVolume(const UObject* WorldContext, float Volume)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		if (!World)
		{
			return;
		}
		if (FAudioDeviceHandle Device = World->GetAudioDevice())
		{
			Device->SetTransientPrimaryVolume(FMath::Clamp(Volume, 0.f, 1.f));
		}
	}
}
