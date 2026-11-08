#include "Abilities/TaGAbilities.h"
#include "Abilities/TaGTags.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemComponent.h"
#include "Character/TaGCharacter.h"

UTaGInputAbility::UTaGInputAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	FGameplayTagContainer Tags;
	Tags.AddTag(TaGTags::Ability_Input);
	SetAssetTags(Tags);
}

ATaGCharacter* UTaGInputAbility::GetTaGCharacter(const FGameplayAbilityActorInfo* ActorInfo) const
{
	return ActorInfo ? Cast<ATaGCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
}

uint8 UTaGInputAbility::InputOf(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		if (const FGameplayAbilitySpec* Spec = ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle))
		{
			return static_cast<uint8>(Spec->InputID);
		}
	}
	return 0;
}

bool UTaGInputAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const ATaGCharacter* C = GetTaGCharacter(ActorInfo);
	const ETaGInput Input = static_cast<ETaGInput>(InputOf(Handle, ActorInfo));
	return C && C->CanUseInput(Input) && Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UTaGInputAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ActiveInput = InputOf(Handle, ActorInfo);
	const ETaGInput Input = static_cast<ETaGInput>(ActiveInput);
	ATaGCharacter* C = GetTaGCharacter(ActorInfo);
	if (C)
	{
		C->InputPressed(Input);
	}
	if (Input == ETaGInput::Shoot || Input == ETaGInput::Secondary)
	{
		UAbilityTask_WaitInputRelease* Wait = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
		Wait->OnRelease.AddDynamic(this, &UTaGInputAbility::OnInputReleased);
		Wait->ReadyForActivation();
		return;
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UTaGInputAbility::OnInputReleased(float TimeHeld)
{
	if (ATaGCharacter* C = GetTaGCharacter(GetCurrentActorInfo()))
	{
		C->InputReleased(static_cast<ETaGInput>(ActiveInput));
	}
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}
