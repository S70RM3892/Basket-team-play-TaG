#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TaGStage.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UPointLightComponent;

/**
 * The rooftop court in Nagisa (Tetsuba district) at dusk, plus the two districts around it.
 * Uses the Blender-made meshes under /Game/Art/Court and /Game/Art/City when they exist, and falls back to
 * engine shapes otherwise. Court lines are always drawn here. Purely visual: the ball's rim and board collision
 * lives in FTaGBallSim; this actor only provides the floor and the walls for the players.
 * World rule shown on screen: the district of the team that scores gets its lights back (WORLD.md section 1).
 */
UCLASS()
class BASKETTAG_API ATaGStage : public AActor
{
	GENERATED_BODY()

public:
	ATaGStage();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Lights up the districts in proportion to each team's score. Team 0 owns district A. */
	void SetCityLights(int32 Score0, int32 Score1, bool bAnimate);
	void OnBasket(int32 Team, int32 NewScore);

	bool HasArt() const { return bHasArt; }

protected:
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> ShapeMaterial;

	struct FDistrictBlock
	{
		TArray<TObjectPtr<UMaterialInstanceDynamic>> Windows;
		float Level = 0.f;       // current brightness 0..1
		float Target = 0.f;
		float FlickerT = 0.f;
	};
	TArray<FDistrictBlock> DistrictA;
	TArray<FDistrictBlock> DistrictB;
	TArray<TObjectPtr<UMaterialInstanceDynamic>> RimGlow;
	float RimFlash = 0.f;
	bool bHasArt = false;

	UStaticMeshComponent* AddMesh(UStaticMesh* Mesh, const FTransform& Xf, bool bCollide);
	UStaticMeshComponent* AddBox(const FVector& Center, const FVector& SizeCm, const FLinearColor& Color, bool bCollide, const FRotator& Rot = FRotator::ZeroRotator);
	void AddLine(const FVector2D& A, const FVector2D& B);
	void BuildFromArt();
	void BuildFallback();
	void BuildLines();
	void BuildWalls();
	void BuildLights();
	void UpdateDistrict(TArray<FDistrictBlock>& D, float Dt);
};
