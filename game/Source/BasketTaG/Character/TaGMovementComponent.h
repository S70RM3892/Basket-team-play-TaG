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
};
