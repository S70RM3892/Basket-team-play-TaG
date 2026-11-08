#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMeshComponent;

/**
 * Small helpers around the Blender-made assets (tools/blender, /Game/Art). Every lookup tolerates missing assets
 * so the game keeps working with engine shapes.
 * Material parameters (contract with tools/import): Color, Emissive, EmissiveStrength, Roughness, Metallic, Opacity.
 */
namespace TaGArt
{
	UStaticMesh* LoadMesh(const FString& PackagePath);       // e.g. "/Game/Art/Heroes/Rei/SM_Rei_Torso"
	UStaticMesh* EngineCube();
	UStaticMesh* EngineCylinder();
	UStaticMesh* EngineSphere();
	UMaterialInterface* BaseMaterial();                       // M_TaGBase or the engine basic shape material
	UMaterialInterface* TranslucentMaterial();                // M_TaGTranslucent or BaseMaterial()

	/** Gives the component a dynamic material on every slot of the given name (or slot 0 when Slot is None). */
	UMaterialInstanceDynamic* PaintSlot(UMeshComponent* C, FName Slot, const FLinearColor& Color, float EmissiveStrength = 0.f, bool bTranslucent = false, float Opacity = 0.5f);

	/** Sets the emissive glow of a dynamic material created by PaintSlot. */
	void SetGlow(UMaterialInstanceDynamic* M, const FLinearColor& Color, float Strength);
}
