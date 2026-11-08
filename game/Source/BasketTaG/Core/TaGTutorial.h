#pragma once

#include "CoreMinimal.h"

/**
 * The tutorial: a 1on1 against a scripted bot that walks through the basics and the rock-paper-scissors reads one by
 * one (RULES_V1 section 3). Each lesson is learned by doing it once or twice, not by reading a manual: the bot shows a
 * clear colored tell (red = contest, orange = tackle, cyan = drive, gold = shot) and the player answers it.
 * The game mode runs the lessons (ATaGGameMode::StartTutorial); the HUD shows the current one.
 */
namespace TaGTutorial
{
	enum EStep : int32
	{
		Move,
		Shoot,
		Perfect,
		Drive,
		DriveVsContest,   // drive > contest
		DodgeVsTackle,    // dodge > tackle
		TackleVsDrive,    // tackle > drive (defense)
		ContestVsShot,    // contest > shot (defense)
		Art,
		Done,
		Count
	};

	struct FLesson
	{
		const TCHAR* Title;
		const TCHAR* Text;        // what to do (keyboard / gamepad names)
		int32 Goal;
		const TCHAR* Unit;        // shown after the progress ("回", "m")
		bool bPlayerAttacks;      // who gets the ball in this lesson
	};

	const FLesson& Get(int32 Step);
	/** Number of real lessons (Done excluded), for "3 / 9". */
	inline int32 NumLessons() { return static_cast<int32>(Done); }
}
