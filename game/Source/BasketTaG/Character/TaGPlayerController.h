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
 * Local input, menus and the network path for lobby choices. The host (listen server or offline) controls the
 * lobby settings and starts the match; everyone picks their own hero.
 */
UCLASS(Config = Game)
class BASKETTAG_API ATaGPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATaGPlayerController();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	const FTaGInputActions& GetInputActions() const { return Actions; }
	float GetLookSensitivity() const { return LookSensitivity; }
	void SetLookSensitivity(float Value);
	bool IsHost() const;
	bool IsPauseMenuOpen() const { return bPauseMenu; }
	void TogglePauseMenu();
	void SetUIMode(bool bUI);

	// --- Title
	void PlayOffline();
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

	UPROPERTY(Config) float LookSensitivity = 1.f;
	UPROPERTY(Config) FString LastJoinAddress = TEXT("127.0.0.1");

protected:
	UPROPERTY() FTaGInputActions Actions;
	UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY() TObjectPtr<ACameraActor> OrbitCamera;

	bool bPauseMenu = false;
	float NetLogTimer = 0.f;
	float NetTestTime = 0.f;
	float InputTestT = 0.f;
	int32 InputTestStep = 0;

	void TickInputTest(float Dt);
	bool bUIMode = false;
	float OrbitAngle = 0.f;

	void UpdateSpectatorCamera(float Dt);
};
