#include "Core/TaGStage.h"
#include "BasketTaG.h"
#include "Core/TaGAudio.h"
#include "Core/TaGTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float LineWidth = 5.f;
	constexpr float LineHeight = 0.4f;
	const FLinearColor LineWhite(0.92f, 0.92f, 0.9f);
	const FLinearColor WindowWarm(1.f, 0.72f, 0.38f);

	int32 SlotIndex(const UStaticMeshComponent* C, FName Slot)
	{
		return C ? C->GetMaterialIndex(Slot) : INDEX_NONE;
	}
}

ATaGStage::ATaGStage()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = Cube.Object;
	CylinderMesh = Cylinder.Object;
	ShapeMaterial = Mat.Object;
}

void ATaGStage::BeginPlay()
{
	Super::BeginPlay();
	BuildFromArt();
	if (!bHasArt)
	{
		BuildFallback();
	}
	BuildLines();
	BuildWalls();
	BuildLights();
	SetCityLights(0, 0, false);
}

UStaticMeshComponent* ATaGStage::AddMesh(UStaticMesh* Mesh, const FTransform& Xf, bool bCollide)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(RootComponent);
	C->SetStaticMesh(Mesh);
	C->SetRelativeTransform(Xf);
	C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (bCollide)
	{
		C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	C->RegisterComponent();
	return C;
}

UStaticMeshComponent* ATaGStage::AddBox(const FVector& Center, const FVector& SizeCm, const FLinearColor& Color, bool bCollide, const FRotator& Rot)
{
	UStaticMeshComponent* C = AddMesh(CubeMesh, FTransform(Rot, Center, SizeCm / 100.f), bCollide);
	C->SetMaterial(0, ShapeMaterial);
	if (UMaterialInstanceDynamic* M = C->CreateAndSetMaterialInstanceDynamic(0))
	{
		M->SetVectorParameterValue(TEXT("Color"), Color);
	}
	return C;
}

void ATaGStage::AddLine(const FVector2D& A, const FVector2D& B)
{
	const FVector2D D = B - A;
	const float Len = D.Size();
	if (Len < 0.5f)
	{
		return;
	}
	const FVector2D Mid = (A + B) * 0.5f;
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
	UStaticMeshComponent* C = AddBox(FVector(Mid.X, Mid.Y, LineHeight * 0.5f), FVector(Len + LineWidth * 0.5f, LineWidth, LineHeight), LineWhite, false, FRotator(0.f, Yaw, 0.f));
	C->SetCastShadow(false);
}

void ATaGStage::BuildFromArt()
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(TEXT("/Game/Art/Court"), Assets, true);
	TArray<FAssetData> City;
	Registry.GetAssetsByPath(TEXT("/Game/Art/City"), City, true);
	Assets.Append(City);

	TMap<int32, FDistrictBlock> BlocksA, BlocksB;
	for (const FAssetData& Data : Assets)
	{
		if (Data.AssetClassPath != UStaticMesh::StaticClass()->GetClassPathName())
		{
			continue;
		}
		UStaticMesh* Mesh = Cast<UStaticMesh>(Data.GetAsset());
		if (!Mesh)
		{
			continue;
		}
		const FString Name = Data.AssetName.ToString();
		const bool bFloor = Name.Contains(TEXT("Rooftop")) && !Name.Contains(TEXT("Props"));
		UStaticMeshComponent* C = AddMesh(Mesh, FTransform::Identity, bFloor);
		bHasArt = true;

		// District windows: SM_CityA_07 -> district A block 7
		const int32 WindowSlot = SlotIndex(C, TEXT("Window"));
		if (WindowSlot != INDEX_NONE && (Name.Contains(TEXT("CityA")) || Name.Contains(TEXT("CityB"))))
		{
			FString Digits;
			for (const TCHAR Ch : Name)
			{
				if (FChar::IsDigit(Ch))
				{
					Digits.AppendChar(Ch);
				}
			}
			const int32 Index = Digits.IsEmpty() ? 0 : FCString::Atoi(*Digits);
			FDistrictBlock& Block = Name.Contains(TEXT("CityA")) ? BlocksA.FindOrAdd(Index) : BlocksB.FindOrAdd(Index);
			if (UMaterialInstanceDynamic* M = C->CreateAndSetMaterialInstanceDynamic(WindowSlot))
			{
				Block.Windows.Add(M);
			}
		}
		if (Name.Contains(TEXT("Hoop")))
		{
			for (const FName Slot : { FName(TEXT("Glow")), FName(TEXT("TaG")) })
			{
				const int32 S = SlotIndex(C, Slot);
				if (S != INDEX_NONE)
				{
					if (UMaterialInstanceDynamic* M = C->CreateAndSetMaterialInstanceDynamic(S))
					{
						RimGlow.Add(M);
					}
				}
			}
		}
	}
	BlocksA.KeySort(TLess<int32>());
	BlocksB.KeySort(TLess<int32>());
	for (auto& Pair : BlocksA)
	{
		DistrictA.Add(Pair.Value);
	}
	for (auto& Pair : BlocksB)
	{
		DistrictB.Add(Pair.Value);
	}
	if (bHasArt)
	{
		UE_LOG(LogTaG, Log, TEXT("Stage: %d art meshes, district A %d blocks, district B %d blocks"), Assets.Num(), DistrictA.Num(), DistrictB.Num());
	}
}

void ATaGStage::BuildFallback()
{
	const FLinearColor Wood(0.55f, 0.32f, 0.14f);
	const FLinearColor Paint(0.05f, 0.25f, 0.3f);
	const FLinearColor Apron(0.07f, 0.08f, 0.1f);
	const FLinearColor RimOrange(1.f, 0.3f, 0.02f);
	const FLinearColor Steel(0.15f, 0.16f, 0.18f);

	AddBox(FVector(TaG::CourtDepth * 0.5f, 0.f, -50.f), FVector(TaG::CourtDepth + 1200.f, TaG::CourtHalfWidth * 2.f + 1200.f, 100.f), Apron, true);
	AddBox(FVector(TaG::CourtDepth * 0.5f, 0.f, 0.05f), FVector(TaG::CourtDepth, TaG::CourtHalfWidth * 2.f, 0.1f), Wood, false);
	AddBox(FVector(TaG::FreeThrowX * 0.5f, 0.f, 0.15f), FVector(TaG::FreeThrowX, TaG::LaneHalfWidth * 2.f, 0.1f), Paint, false);

	const float BoardMidZ = (TaG::BoardBottomZ + TaG::BoardTopZ) * 0.5f;
	AddBox(FVector(TaG::BoardFrontX - TaG::BoardThickness * 0.5f, 0.f, BoardMidZ), FVector(TaG::BoardThickness, TaG::BoardHalfWidth * 2.f, TaG::BoardTopZ - TaG::BoardBottomZ), FLinearColor(0.85f, 0.88f, 0.9f), false);
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
	AddBox(FVector((TaG::BoardFrontX + TaG::RimX - TaG::RimRadius) * 0.5f, 0.f, TaG::RimZ - 2.f), FVector(TaG::RimX - TaG::RimRadius - TaG::BoardFrontX + 2.f, 10.f, 4.f), RimOrange, false);
	for (int32 i = 0; i < 12; ++i)
	{
		const float A = 2.f * UE_PI * i / 12;
		const FVector Top(TaG::RimX + FMath::Cos(A) * TaG::RimRadius, FMath::Sin(A) * TaG::RimRadius, TaG::RimZ);
		const FVector Bottom(TaG::RimX + FMath::Cos(A) * 14.f, FMath::Sin(A) * 14.f, TaG::RimZ - 40.f);
		const FVector D = Bottom - Top;
		AddBox((Top + Bottom) * 0.5f, FVector(D.Size(), 0.6f, 0.6f), FLinearColor(0.95f, 0.95f, 0.95f), false, D.Rotation());
	}
	const float PoleX = -120.f;
	AddBox(FVector(PoleX, 0.f, 175.f), FVector(20.f, 20.f, 350.f), Steel, true);
	AddBox(FVector((PoleX + TaG::BoardFrontX - TaG::BoardThickness) * 0.5f, 0.f, BoardMidZ), FVector(TaG::BoardFrontX - TaG::BoardThickness - PoleX, 12.f, 12.f), Steel, false);
	AddBox(FVector(PoleX, 0.f, 90.f), FVector(50.f, 60.f, 180.f), FLinearColor(0.1f, 0.25f, 0.6f), true);
}

void ATaGStage::BuildLines()
{
	const float W = TaG::CourtHalfWidth;
	AddLine(FVector2D(0.f, -W), FVector2D(0.f, W));
	AddLine(FVector2D(TaG::CourtDepth, -W), FVector2D(TaG::CourtDepth, W));
	AddLine(FVector2D(0.f, -W), FVector2D(TaG::CourtDepth, -W));
	AddLine(FVector2D(0.f, W), FVector2D(TaG::CourtDepth, W));
	AddLine(FVector2D(0.f, -TaG::LaneHalfWidth), FVector2D(TaG::FreeThrowX, -TaG::LaneHalfWidth));
	AddLine(FVector2D(0.f, TaG::LaneHalfWidth), FVector2D(TaG::FreeThrowX, TaG::LaneHalfWidth));
	AddLine(FVector2D(TaG::FreeThrowX, -TaG::LaneHalfWidth), FVector2D(TaG::FreeThrowX, TaG::LaneHalfWidth));

	const float CornerX = TaG::RimX + FMath::Sqrt(FMath::Square(TaG::ArcRadius) - FMath::Square(TaG::CornerLineY));
	AddLine(FVector2D(0.f, -TaG::CornerLineY), FVector2D(CornerX, -TaG::CornerLineY));
	AddLine(FVector2D(0.f, TaG::CornerLineY), FVector2D(CornerX, TaG::CornerLineY));
	const float StartA = FMath::Asin(TaG::CornerLineY / TaG::ArcRadius);
	constexpr int32 Segments = 40;
	FVector2D Prev(TaG::RimX + FMath::Cos(-StartA) * TaG::ArcRadius, FMath::Sin(-StartA) * TaG::ArcRadius);
	for (int32 i = 1; i <= Segments; ++i)
	{
		const float A = FMath::Lerp(-StartA, StartA, float(i) / Segments);
		const FVector2D P(TaG::RimX + FMath::Cos(A) * TaG::ArcRadius, FMath::Sin(A) * TaG::ArcRadius);
		AddLine(Prev, P);
		Prev = P;
	}
	// Free-throw semicircle (decorative)
	Prev = FVector2D(TaG::FreeThrowX, -180.f);
	for (int32 i = 1; i <= 16; ++i)
	{
		const float A = FMath::Lerp(-UE_HALF_PI, UE_HALF_PI, float(i) / 16);
		const FVector2D P(TaG::FreeThrowX + FMath::Cos(A) * 180.f, FMath::Sin(A) * 180.f);
		AddLine(Prev, P);
		Prev = P;
	}
}

void ATaGStage::BuildWalls()
{
	// Invisible walls a few meters out so nobody walks off the roof. The floor box also guarantees a walkable surface.
	const float W = TaG::CourtHalfWidth;
	const float Out = 450.f;
	const FLinearColor C(0.f, 0.f, 0.f);
	AddBox(FVector(TaG::CourtDepth * 0.5f, 0.f, -60.f), FVector(TaG::CourtDepth + 1500.f, W * 2.f + 1500.f, 100.f), C, true)->SetVisibility(false);
	AddBox(FVector(-Out, 0.f, 200.f), FVector(20.f, W * 2.f + Out * 2.f, 600.f), C, true)->SetVisibility(false);
	AddBox(FVector(TaG::CourtDepth + Out, 0.f, 200.f), FVector(20.f, W * 2.f + Out * 2.f, 600.f), C, true)->SetVisibility(false);
	AddBox(FVector(TaG::CourtDepth * 0.5f, -W - Out, 200.f), FVector(TaG::CourtDepth + Out * 2.f, 20.f, 600.f), C, true)->SetVisibility(false);
	AddBox(FVector(TaG::CourtDepth * 0.5f, W + Out, 200.f), FVector(TaG::CourtDepth + Out * 2.f, 20.f, 600.f), C, true)->SetVisibility(false);
}

void ATaGStage::BuildLights()
{
	FActorSpawnParameters Params;
	Params.Owner = this;

	// Evening sun, low and warm: the city's windows and the TaG glow carry the scene.
	if (ADirectionalLight* Sun = GetWorld()->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector(0.f, 0.f, 1000.f), FRotator(-14.f, 140.f, 0.f), Params))
	{
		Sun->SetMobility(EComponentMobility::Movable);
		if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			L->SetIntensity(4.f);
			L->SetLightColor(FLinearColor(1.f, 0.72f, 0.48f));
			L->SetAtmosphereSunLight(true);
			L->SetDynamicShadowDistanceMovableLight(5000.f);
		}
	}
	if (ASkyLight* Sky = GetWorld()->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FVector(0.f, 0.f, 800.f), FRotator::ZeroRotator, Params))
	{
		if (USkyLightComponent* S = Sky->GetLightComponent())
		{
			S->SetMobility(EComponentMobility::Movable);
			S->bRealTimeCapture = true;
			S->SetIntensity(1.2f);
			S->RecaptureSky();
		}
	}
	GetWorld()->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FTransform::Identity, Params);
	if (AExponentialHeightFog* Fog = GetWorld()->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FTransform(FVector(0.f, 0.f, -3000.f)), Params))
	{
		if (UExponentialHeightFogComponent* F = Fog->GetComponent())
		{
			F->SetFogDensity(0.012f);
			F->SetFogHeightFalloff(0.08f);
			F->SetFogInscatteringColor(FLinearColor(0.25f, 0.32f, 0.45f));
		}
	}

	// Court flood lights on four towers
	const FVector Towers[] = {
		FVector(-250.f, -1050.f, 1100.f), FVector(-250.f, 1050.f, 1100.f),
		FVector(1350.f, -1050.f, 1100.f), FVector(1350.f, 1050.f, 1100.f) };
	for (const FVector& P : Towers)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetupAttachment(RootComponent);
		L->SetRelativeLocation(P);
		L->SetIntensity(90000.f);
		L->SetAttenuationRadius(3200.f);
		L->SetLightColor(FLinearColor(1.f, 0.93f, 0.82f));
		L->SetCastShadows(false);
		L->RegisterComponent();
	}
}

void ATaGStage::SetCityLights(int32 Score0, int32 Score1, bool bAnimate)
{
	auto Apply = [bAnimate](TArray<FDistrictBlock>& D, int32 Score)
	{
		// A few windows are always lit; the rest follow the score (first to 11 lights the whole district).
		const int32 N = D.Num();
		const int32 Lit = FMath::Clamp(FMath::CeilToInt((float(Score) / TaG::TargetScore) * N), 0, N);
		for (int32 i = 0; i < N; ++i)
		{
			const float Target = i < Lit ? 1.f : 0.06f;
			if (!bAnimate)
			{
				D[i].Level = Target;
			}
			else if (Target > D[i].Target)
			{
				D[i].FlickerT = 0.6f + 0.05f * i;
			}
			D[i].Target = Target;
		}
	};
	Apply(DistrictA, Score0);
	Apply(DistrictB, Score1);
	if (!bAnimate)
	{
		UpdateDistrict(DistrictA, 0.f);
		UpdateDistrict(DistrictB, 0.f);
	}
}

void ATaGStage::OnBasket(int32 Team, int32 NewScore)
{
	RimFlash = 1.f;
	const FVector Where = Team == 0 ? FVector(400.f, -6000.f, -1500.f) : FVector(400.f, 6000.f, -1500.f);
	TaGAudio::Play2D(this, TEXT("CityPowerUp"), 0.7f);
	TaGAudio::PlayAt(this, TEXT("Score"), TaG::RimCenter(), 1.f);
	(void)Where;
	(void)NewScore;
}

void ATaGStage::UpdateDistrict(TArray<FDistrictBlock>& D, float Dt)
{
	const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	for (FDistrictBlock& B : D)
	{
		if (B.FlickerT > 0.f)
		{
			// Power coming back: a short flicker before the block settles on.
			B.FlickerT -= Dt;
			B.Level = (FMath::Frac(Time * 9.f + B.FlickerT * 3.f) > 0.45f) ? B.Target : B.Target * 0.2f;
			if (B.FlickerT <= 0.f)
			{
				B.Level = B.Target;
			}
		}
		else
		{
			B.Level = FMath::FInterpTo(B.Level, B.Target, Dt, 3.f);
		}
		for (UMaterialInstanceDynamic* M : B.Windows)
		{
			if (M)
			{
				M->SetVectorParameterValue(TEXT("Emissive"), WindowWarm);
				M->SetScalarParameterValue(TEXT("EmissiveStrength"), 12.f * B.Level);
			}
		}
	}
}

void ATaGStage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateDistrict(DistrictA, DeltaSeconds);
	UpdateDistrict(DistrictB, DeltaSeconds);
	if (RimFlash > 0.f || RimGlow.Num() > 0)
	{
		RimFlash = FMath::Max(0.f, RimFlash - DeltaSeconds * 1.2f);
		for (UMaterialInstanceDynamic* M : RimGlow)
		{
			if (M)
			{
				M->SetVectorParameterValue(TEXT("Emissive"), FLinearColor(1.f, 0.55f, 0.15f));
				M->SetScalarParameterValue(TEXT("EmissiveStrength"), 2.f + 30.f * RimFlash);
			}
		}
	}
}
