#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Character/TaGRig.h"
#include "Core/TaGHeroData.h"
#include "TaGReplay.generated.h"

class ATaGCharacter;
class ATaGHeroDisplay;
class UCameraComponent;
class UStaticMeshComponent;

/**
 * Highlight replays. Every machine records what it sees (each body's position, facing and pose, and the ball) 30 times
 * a second for the last 8 seconds. When the server calls a highlight (a slam or an alley-oop that scored), each machine
 * plays the moment back while the ball is dead: stand-in bodies driven by the recorded poses, the real players and ball
 * hidden, slowed down, from a broadcast angle. Purely cosmetic and local; the server holds the next possession for it.
 */
UCLASS(NotPlaceable)
class BASKETTAG_API ATaGReplay : public AActor
{
	GENERATED_BODY()

public:
	ATaGReplay();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The recorder of this world, spawned on first use (never on a dedicated server). */
	static ATaGReplay* Get(UWorld* World);
	static bool IsPlaying(const UWorld* World);
	static FText GetTitle(const UWorld* World);

	/** Plays the recorded window [From, To] (server time) after Delay seconds of real time, at Speed. */
	void Play(float From, float To, float Delay, float Speed, const FText& Title, uint8 Kind, ATaGCharacter* Star = nullptr);
	void Stop();
	bool IsActive() const { return bPlaying; }

	/** Seconds a highlight of this length keeps the ball dead (delay, slowed playback, a beat after). */
	static float HoldSeconds(float Window, float Delay, float Speed) { return Delay + Window / Speed + 0.4f; }

protected:
	struct FCharFrame
	{
		FVector Pos = FVector::ZeroVector;   // the drawn body's floor point (rig origin)
		float Yaw = 0.f;                     // the drawn body's facing
		FTaGPoseInput Pose;
		bool bValid = false;
	};
	struct FFrame
	{
		float Time = 0.f;
		TArray<FCharFrame> Chars;            // indexed by track
		FVector Ball = FVector::ZeroVector;
		bool bBall = false;
	};
	struct FTrack
	{
		TWeakObjectPtr<ATaGCharacter> Who;
		ETaGHero Hero = ETaGHero::Rei;
		int32 Team = 0;
	};

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> GhostBall;
	UPROPERTY() TArray<TObjectPtr<ATaGHeroDisplay>> Ghosts;

	TArray<FTrack> Tracks;
	TArray<FFrame> Frames;       // ring buffer, oldest at Head once full
	int32 Head = 0;
	float SampleT = 0.f;

	bool bPending = false;
	bool bPlaying = false;
	float PendingDelay = 0.f;
	float From = 0.f;
	float To = 0.f;
	float Speed = 0.6f;
	float PlayT = 0.f;
	FText Title;
	uint8 Kind = 0;
	FVector CamFocus = FVector::ZeroVector;
	float StarSide = 0.f;        // which side of the court the scorer comes from (the camera takes the other)
	FVector LastGhostBall = FVector::ZeroVector;
	bool bHasLastGhost = false;
	TArray<FFrame> Clip;         // the window being played, in time order
	TArray<int32> GhostTrack;    // track played by each ghost
	TWeakObjectPtr<ATaGCharacter> StarWho;
	int32 StarGhost = INDEX_NONE; // the stand-in of the player the highlight is about

	void Record(float Dt);
	int32 TrackOf(ATaGCharacter* C);
	void Begin();
	void Finish();
	void SetLiveHidden(bool bHide);
	void PlaceCamera(const FVector& Focus, float Dt, bool bSnap);
};
