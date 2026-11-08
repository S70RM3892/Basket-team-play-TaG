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
 * Uses the Blender-made meshes under /Game/Art/Court and /Game/Art/City (court, props, districts, sea, toon sky dome
 * and clouds) when they exist; then the scene has no lights, only a post-process volume with manual exposure, because
 * every art material is unlit toon lit by MPC_TaG. Falls back to engine shapes and real lights otherwise.
 * Court lines are always drawn here. Purely visual: the ball's rim and board collision
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
	/** The net stretches and sways as a ball drops through (Rate < 1 plays it slowed, for a replay). */
	void SwishNet(float Rate = 1.f) { NetSwish = 1.f; NetRate = FMath::Max(Rate, 0.1f); }

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
	/** The net hangs from its own pivot at the rim centre (SM_Net) and stretches and sways when a ball drops through. */
	UPROPERTY() TObjectPtr<UStaticMeshComponent> NetMesh;
	float NetSwish = 0.f;
	float NetRate = 1.f;
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
