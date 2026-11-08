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

void UTaGMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	const ATaGCharacter* C = Cast<ATaGCharacter>(CharacterOwner);
	const FVector Accel2(Acceleration.X, Acceleration.Y, 0.f);
	if (!C || C->GetAction() != ETaGAction::None || !IsMovingOnGround() || Accel2.IsNearlyZero())
	{
		Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
		return;
	}
	const FVector Dir = Accel2.GetSafeNormal();
	const FVector Vel2(Velocity.X, Velocity.Y, 0.f);
	const float MaxSp = FMath::Max(GetMaxSpeed(), 1.f);
	// A sprint start follows a force-velocity line: the push is strongest from a standstill and fades as the speed in
	// its direction builds (Samozino et al. 2016; Morin & Samozino 2016). Moving against the push (a reversal) gets it all.
	const float Along = FVector::DotProduct(Vel2, Dir);
	const float Push = FMath::Lerp(1.f, 0.2f, FMath::Clamp(Along / MaxSp, 0.f, 1.f));
	// The friction term swings the velocity toward the stick while keeping its size, so a full-speed body could flip
	// around in a few frames. Past about 45 degrees most of that goes: the body brakes and pushes off the other way.
	const float Sp = Vel2.Size();
	const float Cos = Sp > 10.f ? FVector::DotProduct(Vel2 / Sp, Dir) : 1.f;
	const float Sharp = FMath::Clamp((0.7f - Cos) / 1.7f, 0.f, 1.f);
	const FVector Saved = Acceleration;
	Acceleration *= Push;
	Super::CalcVelocity(DeltaTime, Friction * FMath::Lerp(1.f, 0.1f, Sharp), bFluid, BrakingDeceleration);
	Acceleration = Saved;
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
