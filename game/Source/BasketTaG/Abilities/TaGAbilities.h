#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "TaGAbilities.generated.h"

class ATaGCharacter;

/**
 * One locally predicted ability per input channel (granted once per channel; the spec's InputID says which).
 * The ability is the network path for the press: it runs on the owning client right away (prediction) and on the
 * server (authority). Held inputs (shoot, contest) wait for the release, which GAS replicates to the server.
 * What the press means (drive or tackle, pass or steal...) depends on the situation and is decided by ATaGCharacter.
 */
UCLASS()
class BASKETTAG_API UTaGInputAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UTaGInputAbility();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	ATaGCharacter* GetTaGCharacter(const FGameplayAbilityActorInfo* ActorInfo) const;
	uint8 InputOf(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const;
	uint8 ActiveInput = 0;
};
