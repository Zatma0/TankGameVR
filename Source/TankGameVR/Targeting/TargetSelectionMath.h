#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TargetSelectionMath.generated.h"

class UTargetableComponent;

UENUM(BlueprintType)
enum class ETargetRejectReason : uint8
{
	None,
	Invalid,
	TooFar,
	OutsideAngle,
	NotVisible
};

USTRUCT(BlueprintType)
struct TANKGAMEVR_API FTargetCandidate
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	TObjectPtr<UTargetableComponent> Target = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	float Distance = 0.0f;

	/** deg, right + */
	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	float YawOffset = 0.0f;

	/** deg, up + */
	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	float PitchOffset = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	float AngleFromForward = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	float Visibility = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	ETargetRejectReason RejectReason = ETargetRejectReason::None;

	/** Lower is better. */
	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	float Score = 0.0f;

	// Tie-breaker so equal scores don't flip between frames
	uint32 StableId = 0;

	bool IsEligible() const { return RejectReason == ETargetRejectReason::None; }
};

USTRUCT(BlueprintType)
struct TANKGAMEVR_API FTargetSelectionTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "100.0", Units = "Centimeters"))
	float MaximumTargetDistance = 15000.0f;

	/** Half-angle (deg) around the hull's forward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float MaximumTargetAngle = 60.0f;

	/** Fraction of line-of-sight rays that must be clear. 0 = ignore. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumTargetVisibility = 0.34f;

	/** Min yaw (deg) for a candidate to count as being on the requested side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float MinimumSwitchSeparation = 1.0f;

	/** Weight of vertical offset in the score. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float PitchWeight = 0.5f;

	/** Score added per metre (prefers nearer targets). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float DistanceWeightPerMeter = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	bool bWrapAround = false;
};

UCLASS()
class TANKGAMEVR_API UTargetSelectionMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Fills angles, distance and range/angle eligibility. */
	static void EvaluateCandidate(FTargetCandidate& Candidate, const FVector& Origin, const FVector& ReferenceDirection,
		const FVector& SightForward, const FTargetSelectionTuning& Tuning);

	/** Next target on the given side (Direction > 0 = right), INDEX_NONE if none. */
	static int32 SelectSwitchTarget(const TArray<FTargetCandidate>& Candidates, int32 Direction, const UTargetableComponent* Current,
		const FTargetSelectionTuning& Tuning);

	/** Eligible candidate closest to the reference direction. */
	static int32 SelectNearestToReference(const TArray<FTargetCandidate>& Candidates, const FTargetSelectionTuning& Tuning);

	/** Yaw (right +) and pitch (up +) of Direction relative to Reference. */
	static void RelativeYawPitch(const FVector& Reference, const FVector& Direction, float& OutYaw, float& OutPitch);
};

UCLASS()
class TANKGAMEVR_API UTankSightMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Projects a world point onto the display. OutUV: (0,0) top-left to (1,1) bottom-right; off-screen points are
	 * clamped to the edge. Returns false if the point is behind the sight.
	 */
	UFUNCTION(BlueprintPure, Category = "Tank|Sight")
	static bool ProjectToSight(const FTransform& SightTransform, float HorizontalFOVDegrees, float AspectRatio, const FVector& WorldLocation,
		FVector2D& OutUV, bool& bOnScreen);
};
