#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/TaGHeroData.h"
#include "Character/TaGRig.h"
#include "TaGHeroDisplay.generated.h"

class UTaGRigComponent;
class UStaticMeshComponent;
class UCameraComponent;

/**
 * A hero's body without a player: the rig on its own, local to one machine (never replicated). The hero select stands
 * the chosen hero on the court in front of its own camera; the highlight replay drives one per player from recorded
 * poses. Left alone it idles (breathing, a slow sway toward the camera) and cheers for a moment when shown anew.
 */
UCLASS(NotPlaceable)
class BASKETTAG_API ATaGHeroDisplay : public AActor
{
	GENERATED_BODY()

public:
	ATaGHeroDisplay();

	virtual void Tick(float DeltaSeconds) override;

	/** Builds the hero's body (only when it changes) and tints its team lights. */
	void ShowHero(ETaGHero InHero, const FLinearColor& TeamColor, bool bCheer = true);
	bool HasHero() const { return bBuilt; }
	ETaGHero GetHero() const { return Hero; }
	UTaGRigComponent* GetRig() const { return Rig; }
	UCameraComponent* GetCamera() const { return Camera; }

	/** Replay: pose the body as recorded this frame (velocity, action, shot...) with the replay's own time step. */
	void AnimatePose(const FTaGPoseInput& In);
	void SetPose(const FTaGPoseInput& In);
	void ClearPose() { bPose = false; }

	/** Hero select: the camera frames the body standing at the actor's origin, a little off to the side. */
	void FrameForSelect(const FVector& Floor, float FacingYaw);

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTaGRigComponent> Rig;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Shadow;

	ETaGHero Hero = ETaGHero::Rei;
	bool bBuilt = false;
	bool bPose = false;
	FTaGPoseInput Pose;
	FLinearColor Team = FLinearColor::White;
	float IdleT = 0.f;
	float CheerT = 0.f;
	float BaseYaw = 0.f;
};
