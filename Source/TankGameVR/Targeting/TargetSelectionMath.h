#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TargetSelectionMath.generated.h"

class UTargetableComponent;

/** Why a candidate is not eligible (shown in targeting debug). */
UENUM(BlueprintType)
enum class ETargetRejectReason : uint8
{
	None,
	Invalid,
	TooFar,
	OutsideAngle,
	NotVisible
};

/** One evaluated candidate. Angles are relative to the reference direction (current target or current aim). */
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

	/** Signed yaw (deg) from the reference direction, positive = to the right. */
	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	float YawOffset = 0.0f;

	/** Signed pitch (deg) from the reference direction, positive = up. */
	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	float PitchOffset = 0.0f;

	/** Angle (deg) between the hull/sight forward and the candidate. */
	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	float AngleFromForward = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	float Visibility = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	ETargetRejectReason RejectReason = ETargetRejectReason::None;

	/** Lower is better. Only meaningful for eligible candidates. */
	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	float Score = 0.0f;

	/** Stable tie-breaker (unique object id) so equal scores never flip between frames. */
	uint32 StableId = 0;

	bool IsEligible() const { return RejectReason == ETargetRejectReason::None; }
};

USTRUCT(BlueprintType)
struct TANKGAMEVR_API FTargetSelectionTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "100.0", Units = "Centimeters"))
	float MaximumTargetDistance = 15000.0f;

	/** Half-angle (deg) around the sight forward inside which targets can be locked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float MaximumTargetAngle = 60.0f;

	/** 0 = ignore line of sight; otherwise the fraction of sample rays (centre and sides) that must be unobstructed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumTargetVisibility = 0.34f;

	/** A switch candidate must be at least this far (deg) to the requested side to count as "different". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float MinimumSwitchSeparation = 1.0f;

	/** How much vertical offset (deg) counts against a candidate relative to horizontal offset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float PitchWeight = 0.5f;

	/** Score added per metre of distance (prefers nearer targets when directions are similar). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float DistanceWeightPerMeter = 0.05f;

	/** Cycle from the last target on one side back to the first on the other. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	bool bWrapAround = false;
};

/** Pure target scoring and selection; no world access, fully deterministic. */
UCLASS()
class TANKGAMEVR_API UTargetSelectionMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Fills the angular and distance fields and the distance/angle eligibility of a candidate. */
	static void EvaluateCandidate(FTargetCandidate& Candidate, const FVector& Origin, const FVector& ReferenceDirection,
		const FVector& SightForward, const FTargetSelectionTuning& Tuning);

	/** Best eligible candidate on the requested side (Direction > 0 = right). INDEX_NONE if none. Excludes Current. */
	static int32 SelectSwitchTarget(const TArray<FTargetCandidate>& Candidates, int32 Direction, const UTargetableComponent* Current,
		const FTargetSelectionTuning& Tuning);

	/** Best eligible candidate closest to the reference direction (used for first lock and re-acquire). */
	static int32 SelectNearestToReference(const TArray<FTargetCandidate>& Candidates, const FTargetSelectionTuning& Tuning);

	/** Signed yaw (deg, right positive) and pitch (deg, up positive) of Direction relative to Reference (world up = Z). */
	static void RelativeYawPitch(const FVector& Reference, const FVector& Direction, float& OutYaw, float& OutPitch);
};

/** Projection of world positions onto a cockpit display that represents a camera-like sight. */
UCLASS()
class TANKGAMEVR_API UTankSightMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Perspective projection through a virtual sight. SightTransform: X forward, Y right, Z up.
	 * OutUV is (0,0) top-left .. (1,1) bottom-right, always finite. Points behind the sight or outside the view are
	 * clamped to the display edge (in the target's direction) and reported via the return value / bOnScreen.
	 * @return true if the point is in front of the sight.
	 */
	UFUNCTION(BlueprintPure, Category = "Tank|Sight")
	static bool ProjectToSight(const FTransform& SightTransform, float HorizontalFOVDegrees, float AspectRatio, const FVector& WorldLocation,
		FVector2D& OutUV, bool& bOnScreen);
};
