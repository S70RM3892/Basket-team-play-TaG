#include "Core/TaGFx.h"
#include "Character/TaGCharacter.h"
#include "Core/TaGArtLib.h"
#include "Core/TaGHeroData.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"

namespace
{
	constexpr int32 MaxParticles = 1800;

	UMaterialInterface* LoadMat(const TCHAR* Path, bool bTranslucentFallback)
	{
		UMaterialInterface* M = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
		return M ? M : (bTranslucentFallback ? TaGArt::TranslucentMaterial() : TaGArt::BaseMaterial());
	}

	UInstancedStaticMeshComponent* MakeIsm(AActor* Owner, const TCHAR* Name, UStaticMesh* Mesh, UMaterialInterface* Mat)
	{
		UInstancedStaticMeshComponent* C = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		C->SetStaticMesh(Mesh);
		C->SetMaterial(0, Mat);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetNumCustomDataFloats(4);
		C->SetMobility(EComponentMobility::Movable);
		C->bUseAsOccluder = false;
		return C;
	}

	FVector RandDir()
	{
		return FMath::VRand();
	}
}

ATaGFxManager::ATaGFxManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetReplicates(false);
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Orbs = MakeIsm(this, TEXT("Orbs"), TaGArt::EngineSphere(), LoadMat(TEXT("/Game/Art/Materials/M_TaGParticle.M_TaGParticle"), true));
	Sparks = MakeIsm(this, TEXT("Sparks"), TaGArt::EngineCube(), LoadMat(TEXT("/Game/Art/Materials/M_TaGParticle.M_TaGParticle"), true));
	Puffs = MakeIsm(this, TEXT("Puffs"), TaGArt::EngineSphere(), LoadMat(TEXT("/Game/Art/Materials/M_TaGParticleAlpha.M_TaGParticleAlpha"), true));
	for (UInstancedStaticMeshComponent* C : { Orbs.Get(), Sparks.Get(), Puffs.Get() })
	{
		C->SetupAttachment(Root);
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		C->SetUsingAbsoluteScale(true);
	}
}

ATaGFxManager* ATaGFxManager::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}
	if (TActorIterator<ATaGFxManager> It(World); It)
	{
		return *It;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<ATaGFxManager>(ATaGFxManager::StaticClass(), FTransform::Identity, Params);
}

void ATaGFxManager::Add(const FTaGParticle& P)
{
	if (Live.Num() >= MaxParticles)
	{
		Live.RemoveAtSwap(0); // the oldest-ish goes first
	}
	Live.Add(P);
}

void ATaGFxManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	for (int32 i = Live.Num() - 1; i >= 0; --i)
	{
		FTaGParticle& P = Live[i];
		P.Age += Dt;
		if (P.Age >= P.Life)
		{
			Live.RemoveAtSwap(i);
			continue;
		}
		P.Vel.Z -= P.Gravity * Dt;
		P.Vel *= FMath::Max(0.f, 1.f - P.Drag * Dt);
		P.Pos += P.Vel * Dt;
		if (P.Pos.Z < 1.f && P.Gravity > 0.f)
		{
			P.Pos.Z = 1.f;
			P.Vel.Z = FMath::Abs(P.Vel.Z) * 0.3f; // sparks bounce once off the floor
			P.Vel.X *= 0.6f;
			P.Vel.Y *= 0.6f;
		}
	}
	Upload(Orbs, ETaGFxShape::Orb, 0);
	Upload(Sparks, ETaGFxShape::Spark, 1);
	Upload(Puffs, ETaGFxShape::Puff, 2);
}

void ATaGFxManager::Upload(UInstancedStaticMeshComponent* C, ETaGFxShape Shape, int32 Slot)
{
	if (!C)
	{
		return;
	}
	TArray<FTransform> Xf;
	TArray<FLinearColor> Col;
	for (const FTaGParticle& P : Live)
	{
		if (P.Shape != Shape)
		{
			continue;
		}
		const float T = P.Age / P.Life;
		const float Size = FMath::Lerp(P.Size0, P.Size1, T);
		const FLinearColor Color = FMath::Lerp(P.Color0, P.Color1, T);
		if (Shape == ETaGFxShape::Spark)
		{
			const float Speed = P.Vel.Size();
			const FVector Dir = Speed > 1.f ? P.Vel / Speed : FVector::UpVector;
			const float Len = FMath::Max(Size, Speed * P.Stretch * Size * 0.1f);
			Xf.Add(FTransform(Dir.Rotation(), P.Pos, FVector(Len, Size * 0.35f, Size * 0.35f) / 100.f));
		}
		else
		{
			Xf.Add(FTransform(FQuat::Identity, P.Pos, FVector(Size / 100.f)));
		}
		Col.Add(Color);
	}
	const int32 N = Xf.Num();
	const int32 Have = C->GetInstanceCount();
	if (Have < N)
	{
		TArray<FTransform> More;
		More.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), N - Have);
		C->AddInstances(More, false, true);
	}
	// Unused instances shrink to nothing (cheaper than removing and re-adding every frame)
	for (int32 i = N; i < FMath::Min(Shown[Slot], C->GetInstanceCount()); ++i)
	{
		Xf.Add(FTransform(FQuat::Identity, FVector(0.f, 0.f, -10000.f), FVector::ZeroVector));
		Col.Add(FLinearColor::Transparent);
	}
	if (Xf.Num() > 0)
	{
		C->BatchUpdateInstancesTransforms(0, Xf, true, false, true);
		for (int32 i = 0; i < Col.Num(); ++i)
		{
			const float Data[4] = { Col[i].R, Col[i].G, Col[i].B, Col[i].A };
			C->SetCustomData(i, MakeArrayView(Data, 4), false);
		}
		C->MarkRenderStateDirty();
	}
	Shown[Slot] = N;
}

// ================================================================ Recipes

namespace TaGFx
{
	void Burst(const UObject* Ctx, const FVector& At, const FLinearColor& Color, int32 Count, float Speed, float Life, float Size,
		float Gravity, ETaGFxShape Shape, const FVector& Bias)
	{
		ATaGFxManager* M = ATaGFxManager::Get(Ctx);
		if (!M)
		{
			return;
		}
		for (int32 i = 0; i < Count; ++i)
		{
			FTaGParticle P;
			P.Pos = At + RandDir() * Size * 0.3f;
			P.Vel = RandDir() * Speed * FMath::FRandRange(0.4f, 1.f) + Bias;
			P.Gravity = Gravity;
			P.Drag = 1.5f;
			P.Size0 = Size * FMath::FRandRange(0.6f, 1.2f);
			P.Size1 = 0.f;
			P.Color0 = Color;
			P.Color1 = FLinearColor(Color.R, Color.G, Color.B, 0.f);
			P.Life = Life * FMath::FRandRange(0.7f, 1.2f);
			P.Shape = Shape;
			M->Add(P);
		}
	}

	void Ring(const UObject* Ctx, const FVector& At, const FLinearColor& Color, float Radius, float Life, int32 Count, float Size)
	{
		ATaGFxManager* M = ATaGFxManager::Get(Ctx);
		if (!M)
		{
			return;
		}
		for (int32 i = 0; i < Count; ++i)
		{
			const float A = 2.f * UE_PI * i / Count;
			const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
			FTaGParticle P;
			P.Pos = At + Dir * Radius * 0.15f;
			P.Vel = Dir * (Radius / Life) * 1.6f;
			P.Drag = 2.2f;
			P.Size0 = Size;
			P.Size1 = Size * 0.3f;
			P.Color0 = Color;
			P.Color1 = FLinearColor(Color.R, Color.G, Color.B, 0.f);
			P.Life = Life;
			M->Add(P);
		}
	}

	void Line(const UObject* Ctx, const FVector& A, const FVector& B, const FLinearColor& Color, int32 Count, float Life, float Size)
	{
		ATaGFxManager* M = ATaGFxManager::Get(Ctx);
		if (!M)
		{
			return;
		}
		for (int32 i = 0; i < Count; ++i)
		{
			const float T = (i + 0.5f) / Count;
			FTaGParticle P;
			P.Pos = FMath::Lerp(A, B, T) + RandDir() * 4.f;
			P.Vel = RandDir() * 30.f + FVector(0.f, 0.f, 20.f);
			P.Drag = 1.f;
			P.Size0 = Size;
			P.Size1 = 0.f;
			P.Color0 = Color;
			P.Color1 = FLinearColor(Color.R, Color.G, Color.B, 0.f);
			P.Life = Life * FMath::FRandRange(0.6f, 1.f) * (0.5f + 0.5f * T);
			M->Add(P);
		}
	}

	void Dust(const UObject* Ctx, const FVector& At, float Strength)
	{
		ATaGFxManager* M = ATaGFxManager::Get(Ctx);
		if (!M || Strength <= 0.f)
		{
			return;
		}
		const int32 N = FMath::Clamp(FMath::RoundToInt(4.f * Strength), 1, 14);
		for (int32 i = 0; i < N; ++i)
		{
			FVector D = RandDir();
			D.Z = FMath::Abs(D.Z) * 0.4f;
			FTaGParticle P;
			P.Pos = At + FVector(0.f, 0.f, 4.f);
			P.Vel = D * 110.f * Strength;
			P.Gravity = -18.f;
			P.Drag = 3.f;
			P.Size0 = 9.f * Strength;
			P.Size1 = 26.f * Strength;
			P.Color0 = FLinearColor(0.62f, 0.52f, 0.42f, 0.35f);
			P.Color1 = FLinearColor(0.62f, 0.52f, 0.42f, 0.f);
			P.Life = FMath::FRandRange(0.35f, 0.6f);
			P.Shape = ETaGFxShape::Puff;
			M->Add(P);
		}
	}

	void Impact(const UObject* Ctx, const FVector& At, const FLinearColor& Color, float Strength)
	{
		Burst(Ctx, At, Color, FMath::RoundToInt(16 * Strength), 520.f * Strength, 0.45f, 9.f, 900.f, ETaGFxShape::Spark);
		Burst(Ctx, At, FLinearColor(1.f, 0.95f, 0.8f), 6, 120.f, 0.18f, 26.f * Strength);
		Ring(Ctx, At - FVector(0.f, 0.f, 60.f), Color, 140.f * Strength, 0.35f, 18, 10.f);
	}

	void ReadWin(const UObject* Ctx, const FVector& At, const FLinearColor& Color)
	{
		Ring(Ctx, At - FVector(0.f, 0.f, 80.f), Color, 260.f, 0.5f, 32, 14.f);
		Burst(Ctx, At, Color, 26, 420.f, 0.6f, 12.f, 0.f, ETaGFxShape::Spark);
		Burst(Ctx, At, FLinearColor::White, 10, 160.f, 0.35f, 18.f, -200.f);
	}

	void Sparkle(const UObject* Ctx, const FVector& At, const FLinearColor& Color, int32 Count)
	{
		Burst(Ctx, At, Color, Count, 150.f, 0.7f, 8.f, -60.f, ETaGFxShape::Orb);
	}

	void Fireworks(const UObject* Ctx, const FVector& At, const FLinearColor& Color)
	{
		for (int32 k = 0; k < 3; ++k)
		{
			const FVector C = At + FVector(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-220.f, 220.f), FMath::FRandRange(120.f, 260.f));
			Burst(Ctx, C, k == 1 ? FLinearColor(1.f, 0.85f, 0.35f) : Color, 34, 520.f, 1.1f, 11.f, 260.f, ETaGFxShape::Spark);
			Burst(Ctx, C, FLinearColor::White, 6, 80.f, 0.3f, 30.f);
		}
	}

	void Aura(const UObject* Ctx, const FVector& Feet, float Height, float Radius, const FLinearColor& Color, int32 Count)
	{
		ATaGFxManager* M = ATaGFxManager::Get(Ctx);
		if (!M)
		{
			return;
		}
		for (int32 i = 0; i < Count; ++i)
		{
			const float A = FMath::FRandRange(0.f, 2.f * UE_PI);
			FTaGParticle P;
			P.Pos = Feet + FVector(FMath::Cos(A) * Radius, FMath::Sin(A) * Radius, FMath::FRandRange(0.f, Height * 0.4f));
			P.Vel = FVector(0.f, 0.f, FMath::FRandRange(90.f, 180.f));
			P.Size0 = FMath::FRandRange(5.f, 9.f);
			P.Size1 = 0.f;
			P.Color0 = Color;
			P.Color1 = FLinearColor(Color.R, Color.G, Color.B, 0.f);
			P.Life = FMath::FRandRange(0.5f, 0.9f);
			M->Add(P);
		}
	}

	void Art(const ATaGCharacter* User, const FTaGArtDef& Def, uint8 Stage, const FVector& A, const FVector& B)
	{
		if (!User)
		{
			return;
		}
		const FLinearColor C = Def.Color;
		const FVector Feet = User->GetFeetLocation();
		if (Stage == 2)
		{
			// Bounced off a shell, or read: a pale ring and a flash where it was answered
			Ring(User, B - FVector(0.f, 0.f, 60.f), FLinearColor(0.85f, 1.f, 1.f), 160.f, 0.4f, 20, 10.f);
			Burst(User, B, FLinearColor::White, 12, 300.f, 0.3f, 10.f, 0.f, ETaGFxShape::Spark);
			return;
		}
		if (Stage == 1)
		{
			switch (Def.Kind)
			{
			case ETaGArtKind::Bolt:
				Line(User, A, B, C, 26, 0.45f, 9.f);
				Impact(User, B, C, 0.9f);
				break;
			case ETaGArtKind::TeamAura:
			case ETaGArtKind::TeamOne:
			case ETaGArtKind::ContestAt:
				Line(User, A, B, C, 18, 0.55f, 7.f);
				Aura(User, B - FVector(0.f, 0.f, 80.f), 180.f, 35.f, C, 14);
				break;
			case ETaGArtKind::Decoy:
				Line(User, A, B, C, 10, 0.4f, 6.f);
				Sparkle(User, B, C, 8);
				break;
			default:
				Impact(User, B, C, 0.8f);
				break;
			}
			return;
		}
		// Stage 0: the cast, shaped by what the art is
		switch (Def.Kind)
		{
		case ETaGArtKind::Dash:
		case ETaGArtKind::Leap:
			Burst(User, Feet + FVector(0.f, 0.f, 30.f), C, 26, 360.f, 0.5f, 10.f, 300.f, ETaGFxShape::Spark, (A - B).GetSafeNormal() * 200.f);
			Dust(User, Feet, 1.6f);
			break;
		case ETaGArtKind::Wall:
		case ETaGArtKind::ShieldFollow:
			Burst(User, B, C, 24, 260.f, 0.55f, 11.f, 500.f, ETaGFxShape::Spark);
			Dust(User, FVector(B.X, B.Y, Feet.Z), 1.8f);
			break;
		case ETaGArtKind::Field:
		case ETaGArtKind::Root:
			Ring(User, Feet + FVector(0.f, 0.f, 8.f), C, FMath::Max(Def.Radius, 150.f), 0.7f, 36, 12.f);
			Aura(User, Feet, 200.f, 60.f, C, 18);
			break;
		case ETaGArtKind::Aura:
			Ring(User, Feet + FVector(0.f, 0.f, 40.f), C, FMath::Max(Def.Radius, 150.f), 0.4f, 30, 14.f);
			Burst(User, A, C, 22, 420.f, 0.4f, 9.f, 0.f, ETaGFxShape::Spark);
			break;
		case ETaGArtKind::Cone:
			Line(User, A, B, C, 22, 0.5f, 11.f);
			Burst(User, B, C, 16, 300.f, 0.5f, 9.f, 300.f, ETaGFxShape::Spark);
			break;
		case ETaGArtKind::Shell:
		case ETaGArtKind::Counter:
			Aura(User, Feet, 220.f, 55.f, C, 22);
			Ring(User, Feet + FVector(0.f, 0.f, 10.f), C, 120.f, 0.5f, 20, 9.f);
			break;
		case ETaGArtKind::Phase:
		case ETaGArtKind::Decoy:
			Burst(User, A, C.CopyWithNewOpacity(0.7f), 14, 120.f, 0.7f, 26.f, -40.f, ETaGFxShape::Puff);
			Sparkle(User, A, C, 12);
			break;
		case ETaGArtKind::ShotBuff:
			Sparkle(User, A + FVector(0.f, 0.f, 40.f), C, 18);
			Ring(User, A + FVector(0.f, 0.f, 40.f), C, 90.f, 0.4f, 16, 7.f);
			break;
		case ETaGArtKind::TeamAura:
		case ETaGArtKind::TeamOne:
			Ring(User, Feet + FVector(0.f, 0.f, 10.f), C, FMath::Min(FMath::Max(Def.Radius, Def.Range), 500.f), 0.6f, 32, 11.f);
			Aura(User, Feet, 220.f, 45.f, C, 16);
			break;
		case ETaGArtKind::ContestStance:
		case ETaGArtKind::ContestAt:
		case ETaGArtKind::Tackle:
		case ETaGArtKind::Bolt:
		case ETaGArtKind::Pass:
		default:
			Burst(User, A, C, 16, 240.f, 0.4f, 9.f, 0.f, ETaGFxShape::Orb);
			break;
		}
	}
}
