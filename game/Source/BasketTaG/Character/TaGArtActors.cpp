#include "Character/TaGArtActors.h"
#include "Character/TaGCharacter.h"
#include "Core/TaGArtLib.h"
#include "Core/TaGFx.h"
#include "Core/TaGGameState.h"
#include "Core/TaGTypes.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

// ---------------------------------------------------------------- Wall

ATaGWallActor::ATaGWallActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);
	bAlwaysRelevant = true;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetBoxExtent(FVector(HalfThick, HalfWidth, Height * 0.5f));
	Box->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	Box->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	RootComponent = Box;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Box);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetStaticMesh(TaGArt::EngineCube());
	Mesh->SetRelativeScale3D(FVector(HalfThick * 2.f, HalfWidth * 2.f, Height) / 100.f);
}

void ATaGWallActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATaGWallActor, Team);
	DOREPLIFETIME(ATaGWallActor, Style);
}

void ATaGWallActor::Init(int32 InTeam, float Lifetime, uint8 InStyle)
{
	Team = InTeam;
	Style = InStyle;
	SetLifeSpan(Lifetime);
	OnRep_Team();
}

void ATaGWallActor::OnRep_Team()
{
	const ATaGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATaGGameState>() : nullptr;
	const FLinearColor C = GS ? GS->TeamColorFor(Team) : TaG::AllyColor();
	// Each art keeps its own colour (hull wood, welded light, roof tiles, a shield); the team colour edges it.
	const FLinearColor ArtC = Style != 0 && Style < static_cast<uint8>(ETaGArt::Count) ? TaGHeroes::GetArt(static_cast<ETaGArt>(Style)).Color : FLinearColor(0.35f, 0.3f, 0.25f);
	Mat = TaGArt::PaintSlot(Mesh, NAME_None, ArtC * 0.6f + C * 0.25f, 3.f, true, 0.75f);
	TaGArt::SetGlow(Mat, ArtC * 0.5f + C * 0.5f, 2.5f);
}

void ATaGWallActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Rises out of the floor
	Rise = FMath::Min(1.f, Rise + DeltaSeconds * 6.f);
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, -Height * (1.f - Rise)));
	if (!Mat)
	{
		OnRep_Team();
	}

	if (HasAuthority())
	{
		// Pick and roll: allies who brush past the wall get a burst of speed.
		for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
		{
			if (It->GetTeam() != Team)
			{
				continue;
			}
			const FVector L = GetActorTransform().InverseTransformPosition(It->GetActorLocation());
			if (FMath::Abs(L.Y) < HalfWidth + 60.f && FMath::Abs(L.X) < HalfThick + 110.f)
			{
				It->AddSpeedBoost(1.5f);
			}
		}
	}
}

bool ATaGWallActor::BlocksSegment(const FVector& A, const FVector& B) const
{
	const FTransform Xf = GetActorTransform();
	const FVector LA = Xf.InverseTransformPosition(A);
	const FVector LB = Xf.InverseTransformPosition(B);
	const FBox Local(FVector(-HalfThick, -HalfWidth, -Height * 0.5f), FVector(HalfThick, HalfWidth, Height * 0.5f));
	FVector Hit, Normal;
	float Time;
	return FMath::LineExtentBoxIntersection(Local, LA, LB, FVector::ZeroVector, Hit, Normal, Time);
}

bool ATaGWallActor::BlocksShotFrom(const FVector& FeetPos) const
{
	if (FVector::Dist2D(FeetPos, GetActorLocation()) > 420.f)
	{
		return false;
	}
	return BlocksSegment(FeetPos + FVector(0.f, 0.f, 200.f), TaG::RimCenter());
}

// ---------------------------------------------------------------- Field

ATaGFieldActor::ATaGFieldActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);
	bAlwaysRelevant = true;

	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	RootComponent = Disc;
	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Disc->SetStaticMesh(TaGArt::EngineCylinder());
	Disc->SetCastShadow(false);
}

void ATaGFieldActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATaGFieldActor, Kind);
	DOREPLIFETIME(ATaGFieldActor, Team);
	DOREPLIFETIME(ATaGFieldActor, Radius);
	DOREPLIFETIME(ATaGFieldActor, FollowTarget);
}

void ATaGFieldActor::Init(ETaGFieldKind InKind, int32 InTeam, float InRadius, float Lifetime, ATaGCharacter* Follow)
{
	Kind = InKind;
	Team = InTeam;
	Radius = InRadius;
	FollowTarget = Follow;
	SetLifeSpan(Lifetime);
}

const ATaGFieldActor* ATaGFieldActor::FindAt(const UWorld* World, ETaGFieldKind InKind, int32 OwnerTeam, const FVector& P)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ATaGFieldActor> It(World); It; ++It)
	{
		if (It->Kind == InKind && (OwnerTeam < 0 || It->Team == OwnerTeam) && It->Contains(P))
		{
			return *It;
		}
	}
	return nullptr;
}

namespace
{
	FLinearColor FieldColor(ETaGFieldKind K)
	{
		switch (K)
		{
		case ETaGFieldKind::Slow: return FLinearColor(0.2f, 0.6f, 1.f);
		case ETaGFieldKind::Sonic: return FLinearColor(0.8f, 0.4f, 1.f);
		case ETaGFieldKind::Spore: return FLinearColor(0.6f, 0.95f, 0.35f);
		case ETaGFieldKind::Wind: return FLinearColor(0.7f, 0.92f, 1.f);
		case ETaGFieldKind::Feast: return FLinearColor(1.f, 0.62f, 0.25f);
		case ETaGFieldKind::Mist: return FLinearColor(0.78f, 0.72f, 0.95f);
		case ETaGFieldKind::Ice: return FLinearColor(0.65f, 0.88f, 1.f);
		case ETaGFieldKind::Swarm: return FLinearColor(0.88f, 1.f, 0.4f);
		default: return FLinearColor(0.55f, 0.95f, 0.7f);
		}
	}
}

void ATaGFieldActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (FollowTarget)
	{
		const FVector F = FollowTarget->GetFeetLocation();
		SetActorLocation(FVector(F.X, F.Y, 2.f));
	}
	const FLinearColor KindC = FieldColor(Kind);
	if (!Mat)
	{
		const ATaGGameState* GS = GetWorld()->GetGameState<ATaGGameState>();
		const FLinearColor TeamC = GS ? GS->TeamColorFor(Team) : TaG::AllyColor();
		const float Opacity = Kind == ETaGFieldKind::Mist ? 0.5f : 0.35f;
		Mat = TaGArt::PaintSlot(Disc, NAME_None, KindC * 0.7f + TeamC * 0.3f, 2.f, true, Opacity);
	}
	// Sonic and swarm pulse fast; the others ripple slowly
	const bool bFast = Kind == ETaGFieldKind::Sonic || Kind == ETaGFieldKind::Swarm;
	const float Pulse = bFast ? (0.85f + 0.15f * FMath::Sin(Age * 25.f)) : (0.97f + 0.03f * FMath::Sin(Age * 4.f));
	const float Grow = FMath::Min(1.f, Age * 6.f);
	Disc->SetWorldScale3D(FVector(Radius * 2.f * Pulse * Grow / 100.f, Radius * 2.f * Pulse * Grow / 100.f, Kind == ETaGFieldKind::Mist ? 0.6f : 0.03f));
	TaGArt::SetGlow(Mat, KindC, 1.5f + Pulse);

	// Ambient particles: spores, snow, steam, mist, wind, fireflies over the field
	if (GetNetMode() != NM_DedicatedServer)
	{
		FxT -= DeltaSeconds;
		if (FxT <= 0.f)
		{
			FxT = 0.05f;
			const float A = FMath::FRandRange(0.f, 2.f * UE_PI);
			const float R = Radius * FMath::Sqrt(FMath::FRand());
			const FVector P = GetActorLocation() + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 6.f);
			switch (Kind)
			{
			case ETaGFieldKind::Slow: TaGFx::Burst(this, P, FLinearColor(0.35f, 0.75f, 1.f), 1, 20.f, 0.8f, 6.f, -80.f); break;
			case ETaGFieldKind::Spore: TaGFx::Burst(this, P, FLinearColor(0.7f, 1.f, 0.4f), 1, 25.f, 1.4f, 6.f, -45.f); break;
			case ETaGFieldKind::Wind: TaGFx::Burst(this, P + FVector(0.f, 0.f, 50.f), FLinearColor(0.95f, 1.f, 1.f, 0.8f), 1, 80.f, 0.35f, 4.f, 0.f, ETaGFxShape::Spark, FVector(-260.f, 120.f, 30.f)); break;
			case ETaGFieldKind::Feast: TaGFx::Burst(this, P, FLinearColor(1.f, 0.65f, 0.3f), 1, 25.f, 1.1f, 6.f, -70.f); break;
			case ETaGFieldKind::Mist: TaGFx::Burst(this, P + FVector(0.f, 0.f, 40.f), FLinearColor(0.82f, 0.78f, 0.95f, 0.3f), 1, 30.f, 1.6f, 70.f, -8.f, ETaGFxShape::Puff); break;
			case ETaGFieldKind::Ice: TaGFx::Burst(this, P + FVector(0.f, 0.f, 120.f), FLinearColor(0.92f, 0.97f, 1.f), 1, 15.f, 1.5f, 5.f, 60.f); break;
			case ETaGFieldKind::Swarm: TaGFx::Burst(this, P + FVector(0.f, 0.f, FMath::FRandRange(40.f, 200.f)), FLinearColor(0.9f, 1.f, 0.45f), 1, 60.f, 0.6f, 6.f, 0.f); break;
			case ETaGFieldKind::Root: TaGFx::Burst(this, P, FLinearColor(0.55f, 0.95f, 0.65f), 1, 20.f, 1.f, 6.f, -30.f); break;
			default: break;
			}
		}
	}

	if (HasAuthority())
	{
		for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) > Radius)
			{
				continue;
			}
			const bool bAlly = It->GetTeam() == Team;
			switch (Kind)
			{
			case ETaGFieldKind::Slow:
				if (!bAlly) { It->ApplySlow(0.6f, 0.6f, 0.25f); }
				break;
			case ETaGFieldKind::Sonic:
				if (!bAlly) { It->ApplySlow(0.75f, 0.5f, 0.25f); }
				break;
			case ETaGFieldKind::Ice:
				if (!bAlly) { It->ApplySlow(0.72f, 0.75f, 0.3f); }
				break;
			case ETaGFieldKind::Spore:
				if (!bAlly) { It->ApplyGuardDamage(9.f * DeltaSeconds); }
				break;
			case ETaGFieldKind::Wind:
				if (bAlly) { It->AddSpeedBoost(0.25f); }
				break;
			case ETaGFieldKind::Feast:
				if (bAlly) { It->ApplyArtStatus(ETaGArtFx::FieldFeast, 0.25f, 0.f); It->AddSpeedBoost(0.25f); }
				break;
			default:
				break; // mist, swarm and root act through the shot and pass rules
			}
		}
	}
}

// ---------------------------------------------------------------- Beam

ATaGBeamFx::ATaGBeamFx()
{
	PrimaryActorTick.bCanEverTick = true;
	Line = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Line"));
	RootComponent = Line;
	Line->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Line->SetStaticMesh(TaGArt::EngineCylinder());
	Line->SetCastShadow(false);
}

void ATaGBeamFx::Spawn(UWorld* World, const FVector& A, const FVector& B, const FLinearColor& InColor, float Duration, float InThickness, AActor* InFollowA, AActor* InFollowB)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (ATaGBeamFx* Fx = World->SpawnActor<ATaGBeamFx>(ATaGBeamFx::StaticClass(), FTransform(A)))
	{
		Fx->PA = A;
		Fx->PB = B;
		Fx->Color = InColor;
		Fx->Life = Duration;
		Fx->Thickness = InThickness;
		Fx->FollowA = InFollowA;
		Fx->FollowB = InFollowB;
		Fx->Mat = TaGArt::PaintSlot(Fx->Line, NAME_None, InColor, 6.f, true, 0.8f);
		Fx->Place();
	}
}

void ATaGBeamFx::Place()
{
	FVector A = PA;
	FVector B = PB;
	if (FollowA.IsValid())
	{
		A = FollowA->GetActorLocation() + FVector(0.f, 0.f, 20.f);
	}
	if (FollowB.IsValid())
	{
		B = FollowB->GetActorLocation() + FVector(0.f, 0.f, 20.f);
	}
	const FVector D = B - A;
	const float Len = FMath::Max(D.Size(), 1.f);
	// The engine cylinder is 100 cm tall along Z, centered: orient Z along the segment.
	const FQuat Q = FRotationMatrix::MakeFromZ(D / Len).ToQuat();
	SetActorLocationAndRotation((A + B) * 0.5f, Q);
	SetActorScale3D(FVector(Thickness / 100.f, Thickness / 100.f, Len / 100.f));
}

void ATaGBeamFx::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (Age >= Life || (FollowA.IsValid() == false && FollowA.IsStale()) || (FollowB.IsValid() == false && FollowB.IsStale()))
	{
		Destroy();
		return;
	}
	Place();
	const float Fade = 1.f - FMath::Clamp((Age - Life * 0.7f) / (Life * 0.3f), 0.f, 1.f);
	TaGArt::SetGlow(Mat, Color, 8.f * Fade);
}

// ---------------------------------------------------------------- Ghost

ATaGGhostFx::ATaGGhostFx()
{
	PrimaryActorTick.bCanEverTick = true;
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetStaticMesh(TaGArt::EngineCylinder());
	Body->SetCastShadow(false);
}

void ATaGGhostFx::Spawn(UWorld* World, const FVector& Feet, float Height, float Radius, const FLinearColor& InColor, float Duration)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (ATaGGhostFx* G = World->SpawnActor<ATaGGhostFx>(ATaGGhostFx::StaticClass(), FTransform(Feet + FVector(0.f, 0.f, Height * 0.5f))))
	{
		G->SetActorScale3D(FVector(Radius * 2.f / 100.f, Radius * 2.f / 100.f, Height / 100.f));
		G->Color = InColor;
		G->Life = Duration;
		G->Mat = TaGArt::PaintSlot(G->Body, NAME_None, InColor, 4.f, true, 0.45f);
	}
}

void ATaGGhostFx::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (Age >= Life)
	{
		Destroy();
		return;
	}
	const float K = 1.f - Age / Life;
	TaGArt::SetGlow(Mat, Color, 5.f * K);
	if (Mat)
	{
		Mat->SetScalarParameterValue(TEXT("Opacity"), 0.45f * K);
	}
}
