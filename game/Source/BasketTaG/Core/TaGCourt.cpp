#include "Core/TaGCourt.h"
#include "Core/TaGTypes.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor Wood(0.55f, 0.32f, 0.14f);
	const FLinearColor Paint(0.05f, 0.18f, 0.32f);
	const FLinearColor Apron(0.06f, 0.07f, 0.09f);
	const FLinearColor LineWhite(0.9f, 0.9f, 0.88f);
	const FLinearColor RimOrange(1.f, 0.3f, 0.02f);
	const FLinearColor Steel(0.15f, 0.16f, 0.18f);
	constexpr float LineWidth = 5.f;
	constexpr float LineHeight = 0.4f;
}

ATaGCourt::ATaGCourt()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = Cube.Object;
	CylinderMesh = Cylinder.Object;
	ShapeMaterial = Mat.Object;
}

void ATaGCourt::BeginPlay()
{
	Super::BeginPlay();
	BuildFloor();
	BuildHoop();
	BuildLights();
}

UStaticMeshComponent* ATaGCourt::AddBox(const FVector& Center, const FVector& SizeCm, const FLinearColor& Color, bool bCollide, const FRotator& Rot)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(RootComponent);
	C->SetStaticMesh(CubeMesh);
	C->SetMaterial(0, ShapeMaterial);
	C->SetRelativeLocationAndRotation(Center, Rot);
	C->SetRelativeScale3D(SizeCm / 100.f);
	C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (bCollide)
	{
		C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	C->RegisterComponent();
	if (UMaterialInstanceDynamic* M = C->CreateAndSetMaterialInstanceDynamic(0))
	{
		M->SetVectorParameterValue(TEXT("Color"), Color);
	}
	return C;
}

UStaticMeshComponent* ATaGCourt::AddCylinder(const FVector& Center, float DiameterCm, float HeightCm, const FLinearColor& Color, const FRotator& Rot)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(RootComponent);
	C->SetStaticMesh(CylinderMesh);
	C->SetMaterial(0, ShapeMaterial);
	C->SetRelativeLocationAndRotation(Center, Rot);
	C->SetRelativeScale3D(FVector(DiameterCm / 100.f, DiameterCm / 100.f, HeightCm / 100.f));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->RegisterComponent();
	if (UMaterialInstanceDynamic* M = C->CreateAndSetMaterialInstanceDynamic(0))
	{
		M->SetVectorParameterValue(TEXT("Color"), Color);
	}
	return C;
}

void ATaGCourt::AddLine(const FVector2D& A, const FVector2D& B, const FLinearColor& Color)
{
	const FVector2D D = B - A;
	const float Len = D.Size();
	if (Len < 0.5f)
	{
		return;
	}
	const FVector2D Mid = (A + B) * 0.5f;
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
	AddBox(FVector(Mid.X, Mid.Y, LineHeight * 0.5f), FVector(Len + LineWidth * 0.5f, LineWidth, LineHeight), Color, false, FRotator(0.f, Yaw, 0.f));
}

void ATaGCourt::BuildFloor()
{
	// Apron around the court (also the walkable floor beyond the lines), then the wooden court on top.
	AddBox(FVector(TaG::CourtDepth * 0.5f, 0.f, -50.f), FVector(TaG::CourtDepth + 1200.f, TaG::CourtHalfWidth * 2.f + 1200.f, 100.f), Apron, true);
	AddBox(FVector(TaG::CourtDepth * 0.5f, 0.f, 0.05f), FVector(TaG::CourtDepth, TaG::CourtHalfWidth * 2.f, 0.1f), Wood, false);
	// Paint (the lane)
	AddBox(FVector(TaG::FreeThrowX * 0.5f, 0.f, 0.15f), FVector(TaG::FreeThrowX, TaG::LaneHalfWidth * 2.f, 0.1f), Paint, false);

	// Boundary lines
	const float W = TaG::CourtHalfWidth;
	AddLine(FVector2D(0.f, -W), FVector2D(0.f, W), LineWhite);
	AddLine(FVector2D(TaG::CourtDepth, -W), FVector2D(TaG::CourtDepth, W), LineWhite);
	AddLine(FVector2D(0.f, -W), FVector2D(TaG::CourtDepth, -W), LineWhite);
	AddLine(FVector2D(0.f, W), FVector2D(TaG::CourtDepth, W), LineWhite);

	// Lane and free-throw line
	AddLine(FVector2D(0.f, -TaG::LaneHalfWidth), FVector2D(TaG::FreeThrowX, -TaG::LaneHalfWidth), LineWhite);
	AddLine(FVector2D(0.f, TaG::LaneHalfWidth), FVector2D(TaG::FreeThrowX, TaG::LaneHalfWidth), LineWhite);
	AddLine(FVector2D(TaG::FreeThrowX, -TaG::LaneHalfWidth), FVector2D(TaG::FreeThrowX, TaG::LaneHalfWidth), LineWhite);

	// Two-point arc: straight corner lines, then the arc
	const float CornerX = TaG::RimX + FMath::Sqrt(FMath::Square(TaG::ArcRadius) - FMath::Square(TaG::CornerLineY));
	AddLine(FVector2D(0.f, -TaG::CornerLineY), FVector2D(CornerX, -TaG::CornerLineY), LineWhite);
	AddLine(FVector2D(0.f, TaG::CornerLineY), FVector2D(CornerX, TaG::CornerLineY), LineWhite);
	const float StartA = FMath::Asin(TaG::CornerLineY / TaG::ArcRadius);
	constexpr int32 Segments = 40;
	FVector2D Prev(TaG::RimX + FMath::Cos(-StartA) * TaG::ArcRadius, FMath::Sin(-StartA) * TaG::ArcRadius);
	for (int32 i = 1; i <= Segments; ++i)
	{
		const float A = FMath::Lerp(-StartA, StartA, float(i) / Segments);
		const FVector2D P(TaG::RimX + FMath::Cos(A) * TaG::ArcRadius, FMath::Sin(A) * TaG::ArcRadius);
		AddLine(Prev, P, LineWhite);
		Prev = P;
	}

	// Invisible walls a few meters out so nobody walks off into the void.
	const float Out = 500.f;
	AddBox(FVector(-Out, 0.f, 200.f), FVector(20.f, W * 2.f + Out * 2.f, 400.f), Apron, true)->SetVisibility(false);
	AddBox(FVector(TaG::CourtDepth + Out, 0.f, 200.f), FVector(20.f, W * 2.f + Out * 2.f, 400.f), Apron, true)->SetVisibility(false);
	AddBox(FVector(TaG::CourtDepth * 0.5f, -W - Out, 200.f), FVector(TaG::CourtDepth + Out * 2.f, 20.f, 400.f), Apron, true)->SetVisibility(false);
	AddBox(FVector(TaG::CourtDepth * 0.5f, W + Out, 200.f), FVector(TaG::CourtDepth + Out * 2.f, 20.f, 400.f), Apron, true)->SetVisibility(false);
}

void ATaGCourt::BuildHoop()
{
	// Backboard with a target square
	const float BoardMidZ = (TaG::BoardBottomZ + TaG::BoardTopZ) * 0.5f;
	const float BoardH = TaG::BoardTopZ - TaG::BoardBottomZ;
	AddBox(FVector(TaG::BoardFrontX - TaG::BoardThickness * 0.5f, 0.f, BoardMidZ), FVector(TaG::BoardThickness, TaG::BoardHalfWidth * 2.f, BoardH), FLinearColor(0.85f, 0.88f, 0.9f), false);
	const float SqZ0 = TaG::RimZ + 15.f;
	const float SqZ1 = SqZ0 + 45.f;
	const float SqY = 29.5f;
	const float Fx = TaG::BoardFrontX + 0.2f;
	auto BoardLine = [&](float Y0, float Z0, float Y1, float Z1)
	{
		const FVector Mid(Fx, (Y0 + Y1) * 0.5f, (Z0 + Z1) * 0.5f);
		AddBox(Mid, FVector(0.4f, FMath::Max(FMath::Abs(Y1 - Y0), 4.f), FMath::Max(FMath::Abs(Z1 - Z0), 4.f)), RimOrange, false);
	};
	BoardLine(-SqY, SqZ0, SqY, SqZ0);
	BoardLine(-SqY, SqZ1, SqY, SqZ1);
	BoardLine(-SqY, SqZ0, -SqY, SqZ1);
	BoardLine(SqY, SqZ0, SqY, SqZ1);

	// Rim as a ring of short bars (matches the torus in FTaGBallSim)
	constexpr int32 RimSegments = 24;
	for (int32 i = 0; i < RimSegments; ++i)
	{
		const float A0 = 2.f * UE_PI * i / RimSegments;
		const float A1 = 2.f * UE_PI * (i + 1) / RimSegments;
		const FVector P0(TaG::RimX + FMath::Cos(A0) * TaG::RimRadius, FMath::Sin(A0) * TaG::RimRadius, TaG::RimZ);
		const FVector P1(TaG::RimX + FMath::Cos(A1) * TaG::RimRadius, FMath::Sin(A1) * TaG::RimRadius, TaG::RimZ);
		const FVector D = P1 - P0;
		AddBox((P0 + P1) * 0.5f, FVector(D.Size() + 0.6f, TaG::RimTube * 2.f, TaG::RimTube * 2.f), RimOrange, false, D.Rotation());
	}
	// Bracket from the board to the rim
	AddBox(FVector((TaG::BoardFrontX + TaG::RimX - TaG::RimRadius) * 0.5f, 0.f, TaG::RimZ - 2.f), FVector(TaG::RimX - TaG::RimRadius - TaG::BoardFrontX + 2.f, 10.f, 4.f), RimOrange, false);

	// Net hint: a faint cone of thin bars below the rim
	constexpr int32 NetStrands = 12;
	for (int32 i = 0; i < NetStrands; ++i)
	{
		const float A = 2.f * UE_PI * i / NetStrands;
		const FVector Top(TaG::RimX + FMath::Cos(A) * TaG::RimRadius, FMath::Sin(A) * TaG::RimRadius, TaG::RimZ);
		const FVector Bottom(TaG::RimX + FMath::Cos(A) * 14.f, FMath::Sin(A) * 14.f, TaG::RimZ - 40.f);
		const FVector D = Bottom - Top;
		AddBox((Top + Bottom) * 0.5f, FVector(D.Size(), 0.6f, 0.6f), FLinearColor(0.95f, 0.95f, 0.95f), false, D.Rotation());
	}

	// Support: pole behind the baseline and an arm to the board (with collision so players can't walk through)
	const float PoleX = -120.f;
	AddBox(FVector(PoleX, 0.f, 175.f), FVector(20.f, 20.f, 350.f), Steel, true);
	AddBox(FVector((PoleX + TaG::BoardFrontX - TaG::BoardThickness) * 0.5f, 0.f, BoardMidZ), FVector(TaG::BoardFrontX - TaG::BoardThickness - PoleX, 12.f, 12.f), Steel, false);
	// Padding at the pole base
	AddBox(FVector(PoleX, 0.f, 90.f), FVector(50.f, 60.f, 180.f), FLinearColor(0.1f, 0.25f, 0.6f), true);
}

void ATaGCourt::BuildLights()
{
	FActorSpawnParameters Params;
	Params.Owner = this;

	if (ADirectionalLight* Sun = GetWorld()->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector(0.f, 0.f, 1000.f), FRotator(-50.f, 35.f, 0.f), Params))
	{
		Sun->SetMobility(EComponentMobility::Movable);
		if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			L->SetIntensity(6.f);
			L->SetAtmosphereSunLight(true);
			L->SetDynamicShadowDistanceMovableLight(4000.f);
		}
	}
	if (ASkyLight* Sky = GetWorld()->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FVector(0.f, 0.f, 800.f), FRotator::ZeroRotator, Params))
	{
		if (USkyLightComponent* S = Sky->GetLightComponent())
		{
			S->SetMobility(EComponentMobility::Movable);
			S->bRealTimeCapture = true;
			S->SetIntensity(1.f);
			S->RecaptureSky();
		}
	}
	GetWorld()->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FTransform::Identity, Params);
	GetWorld()->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FTransform::Identity, Params);
}
