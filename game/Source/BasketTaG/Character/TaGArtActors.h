#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TaGArtActors.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class ATaGCharacter;

/** Gantetsu's iron wall: blocks bodies and passes, contests shots taken from behind it, speeds up allies who rub past it. */
UCLASS()
class BASKETTAG_API ATaGWallActor : public AActor
{
	GENERATED_BODY()

public:
	ATaGWallActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Style = the art that raised it (0 = the iron wall): picks the colour; the scale picks the size. */
	void Init(int32 InTeam, float Lifetime, uint8 InStyle = 0);
	int32 GetTeam() const { return Team; }

	/** True when the wall stands between a shooter at FeetPos and the rim, close to the shooter. */
	bool BlocksShotFrom(const FVector& FeetPos) const;
	/** True when the segment A-B passes through the wall. */
	bool BlocksSegment(const FVector& A, const FVector& B) const;

	static constexpr float HalfWidth = 150.f;
	static constexpr float HalfThick = 20.f;
	static constexpr float Height = 250.f;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Box;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(ReplicatedUsing = OnRep_Team) int32 Team = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Team) uint8 Style = 0;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Mat;
	float Rise = 0.f;

	UFUNCTION() void OnRep_Team();
};

UENUM()
enum class ETaGFieldKind : uint8
{
	Slow,   // Minamo: water current, opponents slowed
	Sonic,  // Hibiki: sonic pulse around her, drives slowed (the shot contest itself is her Contest state)
	Spore,  // Kokemaru: opponents inside lose guard and do not recover
	Wind,   // Kazami: teammates inside are faster and shoot better from range
	Feast,  // Yatai: teammates inside recover guard fast and are a little faster
	Mist,   // Oboro: opponents inside contest at half strength; passes through it cannot be cut
	Ice,    // Tsurara: opponents inside slip (slow, shorter drives and dodges)
	Swarm,  // Hotaru: fireflies around the opponent ball handler: their shots are contested
	Root    // Kokemaru rooted: roots on the floor around him (the contest itself is in the shot rules)
};

/** A circular field on the floor (Minamo's slow field, Hibiki's sonic wave). Server applies the effect. */
UCLASS()
class BASKETTAG_API ATaGFieldActor : public AActor
{
	GENERATED_BODY()

public:
	ATaGFieldActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void Init(ETaGFieldKind InKind, int32 InTeam, float InRadius, float Lifetime, ATaGCharacter* Follow);
	ETaGFieldKind GetKind() const { return Kind; }
	int32 GetTeam() const { return Team; }
	float GetRadius() const { return Radius; }
	bool Contains(const FVector& P) const { return FVector::Dist2D(P, GetActorLocation()) <= Radius; }
	/** The field of this kind owned by Team (or any team with -1) that covers P, if any. */
	static const ATaGFieldActor* FindAt(const UWorld* World, ETaGFieldKind Kind, int32 OwnerTeam, const FVector& P);

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Disc;
	UPROPERTY(Replicated) ETaGFieldKind Kind = ETaGFieldKind::Slow;
	UPROPERTY(Replicated) int32 Team = 0;
	UPROPERTY(Replicated) float Radius = 400.f;
	UPROPERTY(Replicated) TObjectPtr<ATaGCharacter> FollowTarget;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Mat;
	float Age = 0.f;
	float FxT = 0.f;
};

/** Local cosmetic: a glowing line between two points or two actors (water beam, thread, tether). */
UCLASS()
class BASKETTAG_API ATaGBeamFx : public AActor
{
	GENERATED_BODY()

public:
	ATaGBeamFx();
	virtual void Tick(float DeltaSeconds) override;

	static void Spawn(UWorld* World, const FVector& A, const FVector& B, const FLinearColor& Color, float Duration, float Thickness,
		AActor* FollowA = nullptr, AActor* FollowB = nullptr);

protected:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Line;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Mat;
	TWeakObjectPtr<AActor> FollowA;
	TWeakObjectPtr<AActor> FollowB;
	FVector PA = FVector::ZeroVector;
	FVector PB = FVector::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	float Life = 0.5f;
	float Age = 0.f;
	float Thickness = 3.f;
	void Place();
};

/** Local cosmetic: a fading silhouette (Rei's afterimage, blink origin). */
UCLASS()
class BASKETTAG_API ATaGGhostFx : public AActor
{
	GENERATED_BODY()

public:
	ATaGGhostFx();
	virtual void Tick(float DeltaSeconds) override;

	static void Spawn(UWorld* World, const FVector& Feet, float Height, float Radius, const FLinearColor& Color, float Duration);

protected:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Mat;
	FLinearColor Color = FLinearColor::White;
	float Life = 0.5f;
	float Age = 0.f;
};
