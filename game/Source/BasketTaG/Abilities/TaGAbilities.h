#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "TaGAbilities.generated.h"

class ATaGCharacter;

/** Base for TaG abilities: locally predicted, one instance per player. */
UCLASS(Abstract)
class BASKETTAG_API UTaGGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UTaGGameplayAbility();

protected:
	ATaGCharacter* GetTaGCharacter(const FGameplayAbilityActorInfo* ActorInfo) const;
};

/** GA_Shoot: hold to gather and jump (automatic), release near the top of the jump. */
UCLASS()
class BASKETTAG_API UTaGAbility_Shoot : public UTaGGameplayAbility
{
	GENERATED_BODY()

public:
	UTaGAbility_Shoot();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UFUNCTION()
	void OnInputReleased(float TimeHeld);
};

/** GA_Block: a short swipe in the aim direction; the server checks it against the rising ball. */
UCLASS()
class BASKETTAG_API UTaGAbility_Block : public UTaGGameplayAbility
{
	GENERATED_BODY()

public:
	UTaGAbility_Block();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** GA_Steal: reach for the ball of the handler in front of you; a whiff leaves you slow for a moment. */
UCLASS()
class BASKETTAG_API UTaGAbility_Steal : public UTaGGameplayAbility
{
	GENERATED_BODY()

public:
	UTaGAbility_Steal();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
