#include "Core/TaGReplay.h"
#include "Ball/TaGBall.h"
#include "Character/TaGCharacter.h"
#include "Core/TaGGameState.h"
#include "Core/TaGHeroDisplay.h"
#include "Core/TaGAudio.h"
#include "Core/TaGStage.h"
#include "Core/TaGTypes.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Misc/App.h"

namespace
{
	constexpr float SampleDt = 1.f / 30.f;
	constexpr int32 MaxFrames = 240; // 8 seconds
	TWeakObjectPtr<ATaGReplay> GInstance;
}

ATaGReplay::ATaGReplay()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;
	bReplicates = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->SetFieldOfView(55.f);
	Camera->bConstrainAspectRatio = false;
	GhostBall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostBall"));
	GhostBall->SetupAttachment(Root);
	GhostBall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GhostBall->SetCastShadow(false);
	GhostBall->SetVisibility(false);
}

ATaGReplay* ATaGReplay::Get(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}
	if (GInstance.IsValid() && GInstance->GetWorld() == World)
	{
		return GInstance.Get();
	}
	// The first one in the world, if any (Android's clang rejects a for loop that always returns on its first pass)
	TActorIterator<ATaGReplay> Existing(World);
	if (Existing)
	{
		GInstance = *Existing;
		return *Existing;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATaGReplay* R = World->SpawnActor<ATaGReplay>(ATaGReplay::StaticClass(), FTransform::Identity, Params);
	GInstance = R;
	return R;
}

bool ATaGReplay::IsPlaying(const UWorld* World)
{
	return GInstance.IsValid() && GInstance->GetWorld() == World && GInstance->bPlaying;
}

FText ATaGReplay::GetTitle(const UWorld* World)
{
	return GInstance.IsValid() && GInstance->GetWorld() == World ? GInstance->Title : FText::GetEmpty();
}

void ATaGReplay::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bPlaying)
	{
		Finish();
	}
	if (GInstance.Get() == this)
	{
		GInstance = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

// ================================================================ Recording

int32 ATaGReplay::TrackOf(ATaGCharacter* C)
{
	for (int32 i = 0; i < Tracks.Num(); ++i)
	{
		if (Tracks[i].Who.Get() == C && Tracks[i].Hero == C->GetHero())
		{
			return i;
		}
	}
	FTrack T;
	T.Who = C;
	T.Hero = C->GetHero();
	T.Team = C->GetTeam();
	return Tracks.Add(T);
}

void ATaGReplay::Record(float Dt)
{
	SampleT += Dt;
	if (SampleT < SampleDt)
	{
		return;
	}
	SampleT = FMath::Fmod(SampleT, SampleDt);
	const AGameStateBase* GS = GetWorld()->GetGameState();
	if (!GS)
	{
		return;
	}
	FFrame F;
	F.Time = GS->GetServerWorldTimeSeconds();
	for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
	{
		ATaGCharacter* C = *It;
		const UTaGRigComponent* Rig = C->GetRig();
		if (!Rig)
		{
			continue;
		}
		const int32 Ti = TrackOf(C);
		if (F.Chars.Num() <= Ti)
		{
			F.Chars.SetNum(Ti + 1);
		}
		FCharFrame& CF = F.Chars[Ti];
		CF.Pos = Rig->GetComponentLocation();
		CF.Yaw = Rig->GetComponentRotation().Yaw;
		CF.Pose = C->GetLastPose();
		CF.bValid = true;
	}
	// The one ball of the match
	if (TActorIterator<ATaGBall> It(GetWorld()); It)
	{
		F.Ball = It->GetActorLocation();
		F.bBall = true;
		if (!GhostBall->GetStaticMesh())
		{
			if (const UStaticMeshComponent* M = It->GetBallMesh())
			{
				GhostBall->SetStaticMesh(M->GetStaticMesh());
				GhostBall->SetWorldScale3D(M->GetComponentScale());
				for (int32 i = 0; i < M->GetNumMaterials(); ++i)
				{
					GhostBall->SetMaterial(i, M->GetMaterial(i));
				}
			}
		}
	}
	if (Frames.Num() < MaxFrames)
	{
		Frames.Add(MoveTemp(F));
	}
	else
	{
		Frames[Head] = MoveTemp(F);
		Head = (Head + 1) % MaxFrames;
	}
}

// ================================================================ Playback

void ATaGReplay::Play(float InFrom, float InTo, float Delay, float InSpeed, const FText& InTitle, uint8 InKind, ATaGCharacter* Star)
{
	StarWho = Star;
	if (bPlaying)
	{
		Finish();
	}
	From = InFrom;
	To = InTo;
	Speed = FMath::Clamp(InSpeed, 0.1f, 2.f);
	Title = InTitle;
	Kind = InKind;
	PendingDelay = Delay;
	bPending = true;
}

void ATaGReplay::Stop()
{
	bPending = false;
	if (bPlaying)
	{
		Finish();
	}
}

void ATaGReplay::Begin()
{
	bPending = false;
	bHasLastGhost = false;
	Clip.Reset();
	const int32 N = Frames.Num();
	for (int32 k = 0; k < N; ++k)
	{
		const FFrame& F = Frames[N < MaxFrames ? k : (Head + k) % MaxFrames];
		if (F.Time >= From && F.Time <= To)
		{
			Clip.Add(F);
		}
	}
	if (Clip.Num() < 6)
	{
		return; // nothing recorded for that moment (joined late, a long hitch)
	}
	// One stand-in body per player seen in the clip
	TArray<bool> Used;
	Used.SetNumZeroed(Tracks.Num());
	for (const FFrame& F : Clip)
	{
		for (int32 t = 0; t < F.Chars.Num(); ++t)
		{
			if (F.Chars[t].bValid)
			{
				Used[t] = true;
			}
		}
	}
	const ATaGGameState* GS = GetWorld()->GetGameState<ATaGGameState>();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GhostTrack.Reset();
	for (int32 t = 0; t < Tracks.Num(); ++t)
	{
		if (!Used[t])
		{
			continue;
		}
		ATaGHeroDisplay* G = GetWorld()->SpawnActor<ATaGHeroDisplay>(ATaGHeroDisplay::StaticClass(), FTransform::Identity, Params);
		if (!G)
		{
			continue;
		}
		G->ShowHero(Tracks[t].Hero, GS ? GS->TeamColorFor(Tracks[t].Team) : FLinearColor::White, false);
		G->GetCamera()->SetActive(false);
		if (StarWho.IsValid() && Tracks[t].Who.Get() == StarWho.Get())
		{
			StarGhost = Ghosts.Num();
		}
		Ghosts.Add(G);
		GhostTrack.Add(t);
	}
	SetLiveHidden(true);
	GhostBall->SetVisibility(true);
	PlayT = 0.f;
	bPlaying = true;
	FVector Start = Clip[0].Ball;
	StarSide = Start.Y;
	if (Ghosts.IsValidIndex(StarGhost))
	{
		const int32 t = GhostTrack[StarGhost];
		if (Clip[0].Chars.IsValidIndex(t) && Clip[0].Chars[t].bValid)
		{
			Start = Clip[0].Chars[t].Pos + FVector(0.f, 0.f, 150.f);
			StarSide = Clip[0].Chars[t].Pos.Y;
		}
	}
	PlaceCamera(Start, 0.f, true);
}

void ATaGReplay::Finish()
{
	bPlaying = false;
	for (ATaGHeroDisplay* G : Ghosts)
	{
		if (G)
		{
			G->Destroy();
		}
	}
	Ghosts.Reset();
	GhostTrack.Reset();
	StarGhost = INDEX_NONE;
	GhostBall->SetVisibility(false);
	SetLiveHidden(false);
	Clip.Reset();
}

void ATaGReplay::SetLiveHidden(bool bHide)
{
	for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
	{
		It->SetHiddenForReplay(bHide);
	}
	for (TActorIterator<ATaGBall> It(GetWorld()); It; ++It)
	{
		It->SetActorHiddenInGame(bHide);
	}
}

void ATaGReplay::PlaceCamera(const FVector& Focus, float Dt, bool bSnap)
{
	CamFocus = bSnap ? Focus : FMath::VInterpTo(CamFocus, Focus, Dt, 5.f);
	FVector Eye;
	FVector Look;
	if (Kind <= 2)
	{
		// Slams and alley-oops: low beside the rim on the far side from where the scorer comes in, so the run, the
		// leap and the slam come toward the camera with the rim in the frame; a slow push in
		const float Side = StarSide >= 0.f ? -1.f : 1.f;
		Eye = FVector(TaG::RimX + 260.f - 25.f * PlayT, Side * 470.f, 165.f);
		Look = FMath::Lerp(CamFocus, TaG::RimCenter(), 0.4f);
		Camera->SetFieldOfView(54.f);
	}
	else
	{
		// Everything else: the broadcast view from beyond the sideline
		Eye = FVector(CamFocus.X + 160.f, -TaG::CourtHalfWidth - 320.f, 330.f);
		Look = CamFocus + FVector(0.f, 0.f, 60.f);
		Camera->SetFieldOfView(42.f);
	}
	Camera->SetWorldLocationAndRotation(Eye, (Look - Eye).Rotation());
}

// ================================================================ Tick

void ATaGReplay::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bPlaying)
	{
		Record(DeltaSeconds);
	}
	const float RealDt = static_cast<float>(FMath::Min(FApp::GetDeltaTime(), 0.1));
	if (bPending)
	{
		PendingDelay -= RealDt;
		if (PendingDelay <= 0.f)
		{
			Begin();
		}
	}
	if (!bPlaying)
	{
		return;
	}
	// Played in slow motion: the bodies move Speed times as fast, and their gait sees the same scaled time, so the
	// feet still land where they did
	const float Step = RealDt * Speed;
	PlayT += Step;
	const float Tm = Clip[0].Time + PlayT;
	if (Tm >= Clip.Last().Time)
	{
		Finish();
		return;
	}
	int32 K = 0;
	while (K + 1 < Clip.Num() - 1 && Clip[K + 1].Time <= Tm)
	{
		++K;
	}
	const FFrame& A = Clip[K];
	const FFrame& B = Clip[K + 1];
	const float Alpha = FMath::Clamp((Tm - A.Time) / FMath::Max(B.Time - A.Time, 0.001f), 0.f, 1.f);
	for (int32 g = 0; g < Ghosts.Num(); ++g)
	{
		ATaGHeroDisplay* G = Ghosts[g];
		const int32 t = GhostTrack[g];
		const bool bA = A.Chars.IsValidIndex(t) && A.Chars[t].bValid;
		const bool bB = B.Chars.IsValidIndex(t) && B.Chars[t].bValid;
		if (!G || (!bA && !bB))
		{
			continue;
		}
		const FCharFrame& CA = bA ? A.Chars[t] : B.Chars[t];
		const FCharFrame& CB = bB ? B.Chars[t] : CA;
		G->SetActorLocationAndRotation(FMath::Lerp(CA.Pos, CB.Pos, Alpha), FRotator(0.f, CA.Yaw + FMath::FindDeltaAngleDegrees(CA.Yaw, CB.Yaw) * Alpha, 0.f));
		FTaGPoseInput In = Alpha < 0.5f ? CA.Pose : CB.Pose;
		In.Velocity = FMath::Lerp(CA.Pose.Velocity, CB.Pose.Velocity, Alpha);
		In.Dt = Step;
		G->AnimatePose(In);
	}
	const FVector BallAt = FMath::Lerp(A.Ball, B.Ball, Alpha);
	GhostBall->SetWorldLocation(BallAt);
	// The net snaps again when the stand-in ball drops through the rim (slowed like the rest)
	const float NetZ = TaG::RimZ - 4.f;
	if (bHasLastGhost && LastGhostBall.Z >= NetZ && BallAt.Z < NetZ
		&& FVector2D(BallAt.X - TaG::RimX, BallAt.Y).Size() < TaG::RimRadius + 6.f)
	{
		const ATaGGameState* RGS = GetWorld()->GetGameState<ATaGGameState>();
		if (ATaGStage* Stage = RGS ? RGS->GetStage() : nullptr)
		{
			Stage->SwishNet(Speed);
		}
		TaGAudio::PlayAt(this, TEXT("Swish"), TaG::RimCenter(), 0.8f, 0.85f);
	}
	LastGhostBall = BallAt;
	bHasLastGhost = true;
	GhostBall->AddLocalRotation(FRotator(-720.f * Step, 0.f, 0.f));
	// Follow the scorer more than the ball (a lob high in the air would tilt the shot up into the sky)
	FVector Focus = FVector(BallAt.X, BallAt.Y, FMath::Min(BallAt.Z, 340.f));
	if (Ghosts.IsValidIndex(StarGhost) && Ghosts[StarGhost])
	{
		const FVector StarAt = Ghosts[StarGhost]->GetActorLocation() + FVector(0.f, 0.f, 150.f);
		Focus = FMath::Lerp(StarAt, Focus, 0.35f);
		StarSide = Ghosts[StarGhost]->GetActorLocation().Y;
	}
	PlaceCamera(Focus, RealDt, false);
}
