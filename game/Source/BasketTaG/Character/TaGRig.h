#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Core/TaGHeroData.h"
#include "Core/TaGRules.h"
#include "TaGRig.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
enum class ETaGAction : uint8;

/** What the rig needs to pick a pose this frame. */
struct FTaGPoseInput
{
	float Speed = 0.f;          // horizontal cm/s
	float ForwardSpeed = 0.f;
	FVector Velocity = FVector::ZeroVector;   // world cm/s (the feet follow it)
	bool bOnFloor = true;
	int32 Role = 0;             // 0 loose, 1 holding the ball, 2 guarding the holder
	ETaGAction Action;
	float ActionT = 0.f;
	ETaGShotType ShotType = ETaGShotType::Jumper;
	float ShotCharge = 0.f;
	float DribbleT = 0.f;
	float DodgeSide = 1.f;
	bool bPretellTackle = false;
	bool bPretellContest = false;
	bool bPretellDrive = false;   // tutorial bot: about to drive (leans forward)
	bool bPretellShot = false;    // tutorial bot: about to shoot (ball up)
	bool bBraced = false;         // shelled or rooted: crouched behind the arms
	float Dt = 0.f;
};

/**
 * The player's body: rigid parts (Blender meshes SM_<Hero>_<Part>, or engine shapes as a fallback) hung on joints
 * and animated procedurally. The upper body is posed per action (ported from the Godot prototype's rig.gd); the legs
 * walk with planted-foot IK: each foot is locked to the floor in world space while it carries weight, the gait phase
 * advances with the distance travelled (so steps match the speed), and the stride, lift, stance width, lean and arm
 * swing change with the direction of travel (run, backpedal, side shuffle). Limbs hang down in the rest pose;
 * positive pitch swings them forward.
 */
UCLASS()
class BASKETTAG_API UTaGRigComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UTaGRigComponent();

	void Build(const FTaGHeroDef& Hero);
	void Animate(const FTaGPoseInput& In);
	void SetOwnerNoSeeAll(bool bNoSee);
	void SetTeamColor(const FLinearColor& Color);
	/** Wrists show the first hand's readiness, ankles the second's (the cooldowns are visible to opponents). */
	void SetGlow(const FLinearColor& Color, float WristStrength, float AnkleStrength);
	FVector GetHandWorld(bool bRight) const;
	/** World position of a foot's sole (0 left, 1 right) and whether it carries weight (for footstep effects). */
	FVector GetFootWorld(int32 Side) const { return Feet[Side & 1].Current; }
	bool IsFootPlanted(int32 Side) const { return Feet[Side & 1].bPlanted; }
	/** Counts touchdowns; effects compare it with their last value to spawn dust at the new footprint. */
	int32 GetStepCount() const { return StepCount; }
	float GetPelvisRestHeight() const { return PelvisRestZ; }
	bool UsesArt() const { return bArt; }
	/** Test: how far the posed sole ended up from where the gait wanted it (IK reach), cm. */
	float GetFootError(int32 Side) const;
	FString DebugFeet() const;

	enum EJoint : uint8 { Pivot, Hips, Spine, Neck, ShoulderL, ShoulderR, ElbowL, ElbowR, HipL, HipR, KneeL, KneeR, AnkleL, AnkleR, JointCount };

protected:
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Joints;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> WristGlow;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> AnkleGlow;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> TeamMats;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> OutlineMats;

	bool bArt = false;
	float PelvisRestZ = 95.f;
	float HeightScale = 1.f;
	float ForearmLen = 30.f;
	FVector HandOffset[2] = { FVector(0.f, 0.f, -30.f), FVector(0.f, 0.f, -30.f) };

	// Current (smoothed) upper-body pose in radians
	float CurHipsY = 0.95f, CurLean = 0.f, CurSpin = 0.f, CurTilt = 0.f, CurSideLean = 0.f;
	float Cur[8] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f }; // sh_l sh_r shz_l shz_r el_l el_r hip_l hip_r (FK legs)
	float CurKnee[2] = { 0.f, 0.f };

	// ---- Legs: planted-foot IK
	struct FFoot
	{
		bool bPlanted = true;
		FVector PlantPos = FVector::ZeroVector;   // world, on the floor
		float PlantYaw = 0.f;                     // world degrees
		FVector SwingFrom = FVector::ZeroVector;
		float SwingFromYaw = 0.f;
		FVector Current = FVector::ZeroVector;    // world sole target this frame (floor height)
		float CurYaw = 0.f;
		float Lift = 0.f;                         // cm above the floor
		float Pitch = 0.f;                        // degrees, toes up > 0
		bool bOnToe = false;                      // heel raised (late stance): pivot about the toe
	};
	FFoot Feet[2];
	bool bFeetValid = false;
	bool bWasOnFloor = true;
	bool bSettling = false;
	bool bHasFeet = false;
	float GaitPhase = 0.f;      // cycles [0, 1)
	float LegIKWeight = 1.f;
	float HipDrop = 0.f;        // cm the pelvis is lowered so the legs reach the planted feet
	float LandSquash = 0.f;
	float IdleT = 0.f;
	int32 StepCount = 0;
	FVector2D MoveDirLocal = FVector2D(1.f, 0.f);
	float ThighLen = 45.f;
	float ShinLen = 45.f;
	FVector HipSock[2];          // hip joints relative to the Hips joint
	FVector KneeSock[2];         // knee relative to the hip joint (thigh part)
	FVector ShinEndSock[2];      // ankle (with feet) or sole (without) relative to the knee joint
	FVector SoleSock[2];         // sole relative to the ankle joint (feet only)
	FVector ToeSock[2];          // toe tip relative to the ankle joint (feet only)
	FVector RestSole[2];         // rest-pose sole under each hip, in rig space (floor at z = 0)

	USceneComponent* MakeJoint(USceneComponent* Parent, const TCHAR* Name, const FVector& Loc);
	UStaticMeshComponent* MakePart(USceneComponent* Joint, UStaticMesh* Mesh, const FTransform& Xf = FTransform::Identity);
	void BuildArt(const FTaGHeroDef& Hero);
	void BuildFallback(const FTaGHeroDef& Hero);
	void FinishLegSetup();
	void Clear();
	/** Gait: advances the phase with the distance travelled, plants and lifts the feet in world space. */
	void UpdateFeet(float Dt, const FVector& Vel, bool bOnFloor, float Duty, float Sweep, float Lift, float Widen, float OutYaw);
	/** -1 (foot behind) .. +1 (foot in front), for the arm swing. */
	float FootForward(int32 Side, float Duty) const;
	/** Where the end of the shin chain must be (ankle with feet, sole without), world space. */
	FVector ChainEndTarget(int32 Side) const;
	void SolveLeg(int32 Side, const FQuat& FKThigh, const FQuat& FKKnee);
};
