#pragma once

#include "NativeGameplayTags.h"

namespace TaGTags
{
	BASKETTAG_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Shoot);
	BASKETTAG_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Block);
	BASKETTAG_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Steal);
	BASKETTAG_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_HasBall);
	BASKETTAG_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Charging);
	BASKETTAG_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Airborne);
	BASKETTAG_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Recovering);
}

/** Input IDs used when granting abilities (AbilityLocalInputPressed / Released). */
enum class ETaGAbilityInput : int32
{
	None = 0,
	Shoot = 1,
	Block = 2,
	Steal = 3,
};
