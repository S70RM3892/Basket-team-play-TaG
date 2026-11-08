#include "Core/TaGArtLib.h"
#include "Components/MeshComponent.h"
#include "Engine/StaticMesh.h"
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
}
