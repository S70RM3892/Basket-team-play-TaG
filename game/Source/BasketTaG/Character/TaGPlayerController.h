#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Core/TaGHeroData.h"
#include "TaGPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class ACameraActor;

/** Input actions built in C++ (no input assets in the repository). */
USTRUCT()
struct FTaGInputActions
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<UInputAction> Move;
	UPROPERTY() TObjectPtr<UInputAction> LookMouse;
	UPROPERTY() TObjectPtr<UInputAction> LookStick;
	UPROPERTY() TObjectPtr<UInputAction> Jump;
	UPROPERTY() TObjectPtr<UInputAction> Primary;
	UPROPERTY() TObjectPtr<UInputAction> Secondary;
	UPROPERTY() TObjectPtr<UInputAction> Step;
	UPROPERTY() TObjectPtr<UInputAction> PassSteal;
	UPROPERTY() TObjectPtr<UInputAction> Shoot;
	UPROPERTY() TObjectPtr<UInputAction> Art1;
	UPROPERTY() TObjectPtr<UInputAction> Art2;
	UPROPERTY() TObjectPtr<UInputAction> Pause;
	UPROPERTY() TObjectPtr<UInputAction> CameraToggle;
};

/**
 * Local input, menus, settings and the network path for lobby choices. The host (listen server or offline) controls
 * the lobby settings and starts the match; everyone picks their own hero.
 * Settings are saved per machine in Saved/Config/<Platform>/Game.ini.
 */
UCLASS(Config = Game)
class BASKETTAG_API ATaGPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATaGPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	const FTaGInputActions& GetInputActions() const { return Actions; }
	bool IsHost() const;
	bool IsPauseMenuOpen() const { return bPauseMenu; }
	void TogglePauseMenu();
	void SetUIMode(bool bUI);
	/** Test scenes that place their own camera (no orbiting spectator view). */
	bool bExternalView = false;

	// --- Settings (menu "設定")
	float GetLookSensitivity() const { return LookSensitivity; }
	void SetLookSensitivity(float Value);
	bool IsSettingsOpen() const { return bSettingsOpen; }
	void SetSettingsOpen(bool bOpen);
	/** Applies and saves every setting (called by the settings screen after each change). */
	void ApplySettings();
	/** Short rumble (gamepad) / vibration (phone), if enabled in the settings. */
	void Feedback(float Intensity, float Duration, float CameraKick);

	// --- Title
	void PlayOffline();
	void QuickPlay();
	void StartTutorial();
	void HostOnline();
	void JoinOnline(const FString& Address);
	void QuitGame();
	void LeaveToTitle();

	// --- Lobby / results (server RPCs; the host's options are ignored for guests)
	UFUNCTION(Server, Reliable) void ServerSetLobbyOptions(int32 TeamSize, int32 BotSkill);
	UFUNCTION(Server, Reliable) void ServerSetHero(ETaGHero Hero);
	UFUNCTION(Server, Reliable) void ServerSetTeam(int32 Team);
	UFUNCTION(Server, Reliable) void ServerStartMatch();
	UFUNCTION(Server, Reliable) void ServerRematch();
	UFUNCTION(Server, Reliable) void ServerBackToLobby();
	UFUNCTION(Server, Reliable) void ServerEnterLobby(int32 TeamSize, int32 BotSkill);
	UFUNCTION(Server, Reliable) void ServerQuickPlay();
	UFUNCTION(Server, Reliable) void ServerStartTutorial();
	UFUNCTION(Server, Reliable) void ServerTutorialSkip();

	UPROPERTY(Config) float LookSensitivity = 1.f;
	/**
	 * Right stick vertical look. The engine reports stick-up as +Y (GameInput / Android), which looked up; the first
	 * playtest found that backwards, so the default is now push up = look down ("リバース"). The settings switch it back.
	 */
	UPROPERTY(Config) bool bInvertStickY = true;
	UPROPERTY(Config) bool bInvertMouseY = false;
	UPROPERTY(Config) float FieldOfView = 95.f;
	/** First-person head motion with the steps (0 = off, 1 = as the body moves, 1.5 = stronger). */
	UPROPERTY(Config) float ViewBob = 1.f;
	/** Slow-motion replays of slams and alley-oops during the dead ball (the host's choice counts online). */
	UPROPERTY(Config) bool bReplays = true;
	UPROPERTY(Config) float MasterVolume = 1.f;
	UPROPERTY(Config) float MusicVolume = 0.6f;
	UPROPERTY(Config) bool bVibration = true;
	bool bThreatCued = false;
	UPROPERTY(Config) bool bTutorialDone = false;
	UPROPERTY(Config) FString LastJoinAddress = TEXT("127.0.0.1");

protected:
	UPROPERTY() FTaGInputActions Actions;
	UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY() TObjectPtr<ACameraActor> OrbitCamera;
	/** Hero select: the chosen hero standing on the court in front of its own camera. */
	UPROPERTY() TObjectPtr<class ATaGHeroDisplay> LobbyHero;

	bool bPauseMenu = false;
	bool bSettingsOpen = false;
	float NetLogTimer = 0.f;
	float NetTestTime = 0.f;
	float InputTestT = 0.f;
	int32 InputTestStep = 0;

	void TickInputTest(float Dt);

	/** -TaGTutorialTest: plays the tutorial through the real input path, answering the bot's tells. */
	void TickTutorialTest(float Dt);
	int32 TutTestStep = -1;
	float TutTestClock = 0.f;
	float TutTestT = 0.f;
	float TutTestNext = 0.f;
	float TutTestHeldSince = 0.f;
	bool bTutTestShootHeld = false;
	bool bTutTestContestHeld = false;

	bool bUIMode = false;
	float OrbitAngle = 0.f;

	void UpdateSpectatorCamera(float Dt);
};
