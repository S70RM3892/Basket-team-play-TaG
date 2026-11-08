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
	float Dt = 0.f;
};

/**
 * The player's body: rigid parts (Blender meshes SM_<Hero>_<Part>, or engine shapes as a fallback) hung on joints
 * and animated procedurally, ported from the Godot prototype's rig.gd (run, dribble, defensive stance, each shot type,
 * contest, tackle, spin, stagger, down). Limbs hang down in the rest pose; positive pitch swings them forward.
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
	float GetPelvisRestHeight() const { return PelvisRestZ; }
	bool UsesArt() const { return bArt; }

	enum EJoint : uint8 { Pivot, Hips, Spine, Neck, ShoulderL, ShoulderR, ElbowL, ElbowR, HipL, HipR, KneeL, KneeR, JointCount };

protected:
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Joints;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> WristGlow;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> AnkleGlow;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> TeamMats;

	bool bArt = false;
	float PelvisRestZ = 95.f;
	float HeightScale = 1.f;
	float ForearmLen = 30.f;
	FVector HandOffset[2] = { FVector(0.f, 0.f, -30.f), FVector(0.f, 0.f, -30.f) };
	float Phase = 0.f;

	// Current (smoothed) pose in radians
	float CurHipsY = 0.95f, CurLean = 0.f, CurSpin = 0.f, CurTilt = 0.f;
	float Cur[8] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f }; // sh_l sh_r shz_l shz_r el_l el_r hip_l hip_r
	float CurKnee[2] = { 0.f, 0.f };

	USceneComponent* MakeJoint(USceneComponent* Parent, const TCHAR* Name, const FVector& Loc);
	UStaticMeshComponent* MakePart(USceneComponent* Joint, UStaticMesh* Mesh, const FTransform& Xf = FTransform::Identity);
	void BuildArt(const FTaGHeroDef& Hero);
	void BuildFallback(const FTaGHeroDef& Hero);
	void Clear();
};
