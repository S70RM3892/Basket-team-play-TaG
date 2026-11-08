#include "Core/TaGArtLib.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	template <typename T>
	T* LoadQuiet(const FString& ObjectPath)
	{
		return Cast<T>(StaticLoadObject(T::StaticClass(), nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet));
	}

	FString ObjectPathFor(const FString& PackagePath)
	{
		FString Name;
		PackagePath.Split(TEXT("/"), nullptr, &Name, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		return PackagePath + TEXT(".") + Name;
	}
}

namespace TaGArt
{
	UStaticMesh* LoadMesh(const FString& PackagePath)
	{
		static TMap<FString, TWeakObjectPtr<UStaticMesh>> Cache;
		if (TWeakObjectPtr<UStaticMesh>* Found = Cache.Find(PackagePath))
		{
			if (Found->IsValid())
			{
				return Found->Get();
			}
		}
		UStaticMesh* M = LoadQuiet<UStaticMesh>(ObjectPathFor(PackagePath));
		Cache.Add(PackagePath, M);
		return M;
	}

	UStaticMesh* EngineCube() { return LoadQuiet<UStaticMesh>(TEXT("/Engine/BasicShapes/Cube.Cube")); }
	UStaticMesh* EngineCylinder() { return LoadQuiet<UStaticMesh>(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")); }
	UStaticMesh* EngineSphere() { return LoadQuiet<UStaticMesh>(TEXT("/Engine/BasicShapes/Sphere.Sphere")); }

	UMaterialInterface* BaseMaterial()
	{
		static TWeakObjectPtr<UMaterialInterface> M;
		if (!M.IsValid())
		{
			UMaterialInterface* Found = LoadQuiet<UMaterialInterface>(TEXT("/Game/Art/Materials/M_TaGBase.M_TaGBase"));
			if (!Found)
			{
				Found = LoadQuiet<UMaterialInterface>(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
			}
			M = Found;
		}
		return M.Get();
	}

	UMaterialInterface* TranslucentMaterial()
	{
		static TWeakObjectPtr<UMaterialInterface> M;
		if (!M.IsValid())
		{
			UMaterialInterface* Found = LoadQuiet<UMaterialInterface>(TEXT("/Game/Art/Materials/M_TaGTranslucent.M_TaGTranslucent"));
			M = Found ? Found : BaseMaterial();
		}
		return M.Get();
	}

	UMaterialInstanceDynamic* PaintSlot(UMeshComponent* C, FName Slot, const FLinearColor& Color, float EmissiveStrength, bool bTranslucent, float Opacity)
	{
		if (!C)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Result = nullptr;
		const int32 Num = C->GetNumMaterials();
		for (int32 i = 0; i < FMath::Max(Num, 1); ++i)
		{
			if (!Slot.IsNone())
			{
				const TArray<FName> Names = C->GetMaterialSlotNames();
				if (!Names.IsValidIndex(i) || Names[i] != Slot)
				{
					continue;
				}
			}
			UMaterialInterface* Parent = bTranslucent ? TranslucentMaterial() : C->GetMaterial(i);
			// Painting a slot twice (the hero is set up again) must not stack a dynamic material on another one:
			// only materials and constant instances are valid parents
			while (UMaterialInstanceDynamic* Old = Cast<UMaterialInstanceDynamic>(Parent))
			{
				Parent = Old->Parent;
			}
			if (!Parent || Slot.IsNone())
			{
				Parent = bTranslucent ? TranslucentMaterial() : BaseMaterial();
			}
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Parent, C);
			M->SetVectorParameterValue(TEXT("Color"), Color);
			M->SetVectorParameterValue(TEXT("Emissive"), Color);
			M->SetScalarParameterValue(TEXT("EmissiveStrength"), EmissiveStrength);
			M->SetScalarParameterValue(TEXT("Opacity"), Opacity);
			C->SetMaterial(i, M);
			Result = M;
			if (Slot.IsNone())
			{
				break;
			}
		}
		return Result;
	}

	void SetGlow(UMaterialInstanceDynamic* M, const FLinearColor& Color, float Strength)
	{
		if (M)
		{
			M->SetVectorParameterValue(TEXT("Emissive"), Color);
			M->SetScalarParameterValue(TEXT("EmissiveStrength"), Strength);
		}
	}

	UStaticMeshComponent* MakeBlobShadow(AActor* Owner)
	{
		if (!Owner)
		{
			return nullptr;
		}
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetupAttachment(Owner->GetRootComponent());
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		C->SetUsingAbsoluteScale(true);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetCastShadow(false);
		C->SetStaticMesh(EngineCylinder());
		C->RegisterComponent();
		// Black in the toon material is black whatever the light, so only the opacity matters
		PaintSlot(C, NAME_None, FLinearColor::Black, 0.f, true, 0.35f);
		return C;
	}

	void PlaceBlobShadow(UStaticMeshComponent* Shadow, const FVector& At, float Diameter, float Height, float Opacity)
	{
		if (!Shadow)
		{
			return;
		}
		const float Up = FMath::Clamp(Height / 400.f, 0.f, 1.f);
		const float D = Diameter * FMath::Lerp(1.f, 0.55f, Up);
		// 0.5 cm thin, floating just above the court lines (0.4 cm)
		Shadow->SetWorldLocationAndRotation(FVector(At.X, At.Y, 0.7f), FRotator::ZeroRotator);
		Shadow->SetWorldScale3D(FVector(D / 100.f, D / 100.f, 0.005f));
		if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(Shadow->GetMaterial(0)))
		{
			M->SetScalarParameterValue(TEXT("Opacity"), Opacity * FMath::Lerp(1.f, 0.3f, Up));
		}
	}
}
