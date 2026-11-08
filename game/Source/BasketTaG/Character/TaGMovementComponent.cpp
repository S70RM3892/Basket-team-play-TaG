#include "Character/TaGMovementComponent.h"
#include "Character/TaGCharacter.h"

float UTaGMovementComponent::GetMaxSpeed() const
{
	if (const ATaGCharacter* C = Cast<ATaGCharacter>(CharacterOwner))
	{
		if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking || MovementMode == MOVE_Falling)
		{
			return C->GetDesiredMaxSpeed();
		}
	}
	return Super::GetMaxSpeed();
}

float UTaGMovementComponent::GetMaxAcceleration() const
{
	if (const ATaGCharacter* C = Cast<ATaGCharacter>(CharacterOwner))
	{
		return C->GetDesiredAcceleration();
	}
	return Super::GetMaxAcceleration();
}

float UTaGMovementComponent::GetMaxBrakingDeceleration() const
{
	if (const ATaGCharacter* C = Cast<ATaGCharacter>(CharacterOwner))
	{
		if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking)
		{
			return C->GetDesiredBraking();
		}
	}
	return Super::GetMaxBrakingDeceleration();
}
