#include "Abilities/TaGTags.h"

namespace TaGTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Input, "Ability.Input", "An input-channel ability (primary, secondary, step, pass/steal, shoot, arts)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hand_Drive, "Hand.Drive", "RPS: drive (beats contest, loses to tackle)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hand_Shot, "Hand.Shot", "RPS: shot (beats spacing, loses to contest)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hand_Evade, "Hand.Evade", "RPS: evade (beats tackle, loses to spacing)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hand_Tackle, "Hand.Tackle", "RPS: tackle (beats drive, loses to evade)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hand_Contest, "Hand.Contest", "RPS: contest (beats shot, loses to drive)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hand_Spacing, "Hand.Spacing", "RPS: spacing (beats evade, loses to shot)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hand_Teamplay, "Hand.Teamplay", "Outside the 1on1 RPS: creates 2on1");
}
