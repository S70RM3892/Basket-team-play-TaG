#include "Abilities/TaGAbilities.h"
#include "Abilities/TaGTags.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Character/TaGCharacter.h"

UTaGGameplayAbility::UTaGGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

ATaGCharacter* UTaGGameplayAbility::GetTaGCharacter(const FGameplayAbilityActorInfo* ActorInfo) const
{
	return ActorInfo ? Cast<ATaGCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
}

// ---------------------------------------------------------------- Shoot

UTaGAbility_Shoot::UTaGAbility_Shoot()
{
	FGameplayTagContainer Tags;
	Tags.AddTag(TaGTags::Ability_Shoot);
	SetAssetTags(Tags);
	ActivationBlockedTags.AddTag(TaGTags::State_Recovering);
}

bool UTaGAbility_Shoot::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const ATaGCharacter* C = GetTaGCharacter(ActorInfo);
	return C && C->CanStartShot() && Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UTaGAbility_Shoot::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (ATaGCharacter* C = GetTaGCharacter(ActorInfo))
	{
		C->BeginShot();
	}
	UAbilityTask_WaitInputRelease* Wait = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	Wait->OnRelease.AddDynamic(this, &UTaGAbility_Shoot::OnInputReleased);
	Wait->ReadyForActivation();
}

void UTaGAbility_Shoot::OnInputReleased(float TimeHeld)
{
	// If the charge already auto-released, ReleaseShot does nothing.
	if (ATaGCharacter* C = GetTaGCharacter(GetCurrentActorInfo()))
	{
		C->ReleaseShot();
	}
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UTaGAbility_Shoot::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bWasCancelled)
	{
		if (ATaGCharacter* C = GetTaGCharacter(ActorInfo))
		{
			C->ReleaseShot();
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------- Block

UTaGAbility_Block::UTaGAbility_Block()
{
	FGameplayTagContainer Tags;
	Tags.AddTag(TaGTags::Ability_Block);
	SetAssetTags(Tags);
	ActivationBlockedTags.AddTag(TaGTags::State_Charging);
	ActivationBlockedTags.AddTag(TaGTags::State_Recovering);
}

bool UTaGAbility_Block::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const ATaGCharacter* C = GetTaGCharacter(ActorInfo);
	return C && C->CanBlock() && Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UTaGAbility_Block::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		if (ATaGCharacter* C = GetTaGCharacter(ActorInfo))
		{
			C->StartBlock();
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// ---------------------------------------------------------------- Steal

UTaGAbility_Steal::UTaGAbility_Steal()
{
	FGameplayTagContainer Tags;
	Tags.AddTag(TaGTags::Ability_Steal);
	SetAssetTags(Tags);
	ActivationBlockedTags.AddTag(TaGTags::State_Charging);
	ActivationBlockedTags.AddTag(TaGTags::State_Recovering);
}

bool UTaGAbility_Steal::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const ATaGCharacter* C = GetTaGCharacter(ActorInfo);
	return C && C->CanSteal() && Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UTaGAbility_Steal::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		if (ATaGCharacter* C = GetTaGCharacter(ActorInfo))
		{
			C->StartSteal();
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
