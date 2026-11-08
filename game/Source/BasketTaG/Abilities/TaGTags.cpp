#include "Abilities/TaGTags.h"

namespace TaGTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Shoot, "Ability.Shoot", "Hold to gather and jump, release at the top to shoot");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Block, "Ability.Block", "Swipe at the ball in the aim direction");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Steal, "Ability.Steal", "Reach for the ball of the handler in front");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_HasBall, "State.HasBall", "Holding the ball");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Charging, "State.Charging", "Shot is being charged");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Airborne, "State.Airborne", "Feet off the floor");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Recovering, "State.Recovering", "Punished after a whiffed steal");
}
