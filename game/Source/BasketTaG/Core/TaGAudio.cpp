#include "Core/TaGAudio.h"
#include "Components/AudioComponent.h"
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

	USoundBase* Find(FName Name)
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
		const FString Path = FString::Printf(TEXT("/Game/Audio/SFX/%s.%s"), *N, *N);
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
}

namespace TaGAudio
{
	void PlayAt(const UObject* WorldContext, FName Sound, const FVector& Location, float Volume)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		if (!World || World->GetNetMode() == NM_DedicatedServer)
		{
			return;
		}
		if (USoundBase* S = Find(Sound))
		{
			UGameplayStatics::PlaySoundAtLocation(WorldContext, S, Location, FRotator::ZeroRotator, Volume, 1.f, 0.f, Attenuation());
		}
	}

	void Play2D(const UObject* WorldContext, FName Sound, float Volume)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		if (!World || World->GetNetMode() == NM_DedicatedServer)
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
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		if (!World || World->GetNetMode() == NM_DedicatedServer)
		{
			return;
		}
		if (USoundBase* S = Find(TEXT("AmbientLoop")))
		{
			if (USoundWave* W = Cast<USoundWave>(S))
			{
				W->bLooping = true;
			}
			UGameplayStatics::SpawnSound2D(WorldContext, S, 0.35f, 1.f, 0.f, nullptr, true, false);
		}
	}
}
