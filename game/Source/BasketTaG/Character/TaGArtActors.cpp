#include "Character/TaGArtActors.h"
#include "Character/TaGCharacter.h"
#include "Core/TaGArtLib.h"
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
}

void ATaGWallActor::Init(int32 InTeam, float Lifetime)
{
	Team = InTeam;
	SetLifeSpan(Lifetime);
	OnRep_Team();
}

void ATaGWallActor::OnRep_Team()
{
	const ATaGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATaGGameState>() : nullptr;
	const FLinearColor C = GS ? GS->TeamColorFor(Team) : TaG::AllyColor();
	Mat = TaGArt::PaintSlot(Mesh, NAME_None, FLinearColor(0.35f, 0.3f, 0.25f) * 0.6f + C * 0.25f, 3.f, true, 0.75f);
	TaGArt::SetGlow(Mat, C, 2.5f);
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

void ATaGFieldActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (FollowTarget)
	{
		const FVector F = FollowTarget->GetFeetLocation();
		SetActorLocation(FVector(F.X, F.Y, 2.f));
	}
	if (!Mat)
	{
		const ATaGGameState* GS = GetWorld()->GetGameState<ATaGGameState>();
		const FLinearColor Base = Kind == ETaGFieldKind::Slow ? FLinearColor(0.1f, 0.45f, 0.9f) : FLinearColor(0.7f, 0.35f, 1.f);
		const FLinearColor TeamC = GS ? GS->TeamColorFor(Team) : TaG::AllyColor();
		Mat = TaGArt::PaintSlot(Disc, NAME_None, Base * 0.7f + TeamC * 0.3f, 2.f, true, 0.35f);
	}
	// Sonic pulses; the slow field ripples
	const float Pulse = Kind == ETaGFieldKind::Sonic ? (0.85f + 0.15f * FMath::Sin(Age * 25.f)) : (0.97f + 0.03f * FMath::Sin(Age * 4.f));
	Disc->SetWorldScale3D(FVector(Radius * 2.f * Pulse / 100.f, Radius * 2.f * Pulse / 100.f, 0.03f));
	TaGArt::SetGlow(Mat, Kind == ETaGFieldKind::Slow ? FLinearColor(0.2f, 0.6f, 1.f) : FLinearColor(0.8f, 0.4f, 1.f), 1.5f + Pulse);

	if (HasAuthority())
	{
		for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
		{
			if (It->GetTeam() == Team || FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) > Radius)
			{
				continue;
			}
			It->ApplySlow(Kind == ETaGFieldKind::Slow ? 0.6f : 0.75f, Kind == ETaGFieldKind::Slow ? 0.6f : 0.5f, 0.25f);
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
