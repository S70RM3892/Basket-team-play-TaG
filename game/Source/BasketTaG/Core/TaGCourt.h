#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TaGCourt.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * The FIBA 3x3 half court, the hoop and the lights, built from engine shapes at runtime
 * (no map or mesh assets in the repository during P1). Purely visual: the ball's rim and board
 * collision lives in FTaGBallSim; this actor only provides the floor and walls for the players.
 */
UCLASS()
class BASKETTAG_API ATaGCourt : public AActor
{
	GENERATED_BODY()

public:
	ATaGCourt();

	virtual void BeginPlay() override;

protected:
	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	UStaticMeshComponent* AddBox(const FVector& Center, const FVector& SizeCm, const FLinearColor& Color, bool bCollide, const FRotator& Rot = FRotator::ZeroRotator);
	UStaticMeshComponent* AddCylinder(const FVector& Center, float DiameterCm, float HeightCm, const FLinearColor& Color, const FRotator& Rot = FRotator::ZeroRotator);
	void AddLine(const FVector2D& A, const FVector2D& B, const FLinearColor& Color);
	void BuildFloor();
	void BuildHoop();
	void BuildLights();
};
