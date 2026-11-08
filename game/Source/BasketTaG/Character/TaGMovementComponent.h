#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TaGMovementComponent.generated.h"

/**
 * Speed and acceleration come from the character's action state (drive, dodge, tackle, step, shot motions...),
 * which runs the same way on the owning client and on the server, so the moves stay predicted.
 */
UCLASS()
class BASKETTAG_API UTaGMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;

	/**
	 * Free running only (no action): the push fades as speed builds in its direction (strong first step, soft arrival),
	 * and sharp turns brake and push off again instead of swinging the full speed around in a few frames.
	 * Depends only on velocity and the move's acceleration, so the owning client and the server compute the same.
	 */
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
};
