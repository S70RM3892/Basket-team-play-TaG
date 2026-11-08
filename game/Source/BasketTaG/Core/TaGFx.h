#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TaGFx.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class ATaGCharacter;
struct FTaGArtDef;

enum class ETaGFxShape : uint8
{
	Orb,     // additive glowing sphere
	Spark,   // additive streak stretched along its velocity
	Puff     // alpha-blended soft sphere (dust, mist, smoke)
};

struct FTaGParticle
{
	FVector Pos = FVector::ZeroVector;
	FVector Vel = FVector::ZeroVector;
	float Gravity = 0.f;      // cm/s^2 downward (negative floats up)
	float Drag = 0.f;         // per second
	float Size0 = 10.f;       // cm diameter at birth
	float Size1 = 0.f;        // at death
	FLinearColor Color0 = FLinearColor::White;
	FLinearColor Color1 = FLinearColor(1.f, 1.f, 1.f, 0.f);
	float Life = 1.f;
	float Age = 0.f;
	float Stretch = 0.012f;   // sparks: length per (cm/s) of speed
	ETaGFxShape Shape = ETaGFxShape::Orb;
};

/**
 * Client-side particles without particle assets: three instanced meshes (additive orbs and sparks, alpha puffs)
 * whose instances are the particles, coloured through per-instance custom data (M_TaGParticle / M_TaGParticleAlpha,
 * made by tools/import). One manager per world, spawned on first use.
 */
UCLASS()
class BASKETTAG_API ATaGFxManager : public AActor
{
	GENERATED_BODY()

public:
	ATaGFxManager();
	virtual void Tick(float DeltaSeconds) override;

	static ATaGFxManager* Get(const UObject* WorldContext);
	void Add(const FTaGParticle& P);

protected:
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Orbs;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Sparks;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Puffs;
	TArray<FTaGParticle> Live;
	int32 Shown[3] = { 0, 0, 0 };

	void Upload(UInstancedStaticMeshComponent* C, ETaGFxShape Shape, int32 Slot);
};

/** Effect recipes built on the particle manager (all local, cosmetic). */
namespace TaGFx
{
	BASKETTAG_API void Burst(const UObject* Ctx, const FVector& At, const FLinearColor& Color, int32 Count, float Speed, float Life, float Size,
		float Gravity = 0.f, ETaGFxShape Shape = ETaGFxShape::Orb, const FVector& Bias = FVector::ZeroVector);
	BASKETTAG_API void Ring(const UObject* Ctx, const FVector& At, const FLinearColor& Color, float Radius, float Life, int32 Count = 24, float Size = 12.f);
	BASKETTAG_API void Line(const UObject* Ctx, const FVector& A, const FVector& B, const FLinearColor& Color, int32 Count, float Life, float Size);
	BASKETTAG_API void Dust(const UObject* Ctx, const FVector& At, float Strength);
	BASKETTAG_API void Impact(const UObject* Ctx, const FVector& At, const FLinearColor& Color, float Strength);
	BASKETTAG_API void ReadWin(const UObject* Ctx, const FVector& At, const FLinearColor& Color);
	BASKETTAG_API void Sparkle(const UObject* Ctx, const FVector& At, const FLinearColor& Color, int32 Count = 14);
	BASKETTAG_API void Fireworks(const UObject* Ctx, const FVector& At, const FLinearColor& Color);
	BASKETTAG_API void Aura(const UObject* Ctx, const FVector& Feet, float Height, float Radius, const FLinearColor& Color, int32 Count);
	/** A hero art: Stage 0 cast (A = user, B = ahead), 1 landed (B = target), 2 bounced or read (B = target). */
	BASKETTAG_API void Art(const ATaGCharacter* User, const FTaGArtDef& Def, uint8 Stage, const FVector& A, const FVector& B);
}
