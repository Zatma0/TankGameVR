#include "Targeting/TargetSelectionMath.h"
#include "Targeting/TargetableComponent.h"

void UTargetSelectionMath::RelativeYawPitch(const FVector& Reference, const FVector& Direction, float& OutYaw, float& OutPitch)
{
	const FRotator RefRot = Reference.GetSafeNormal().Rotation();
	const FRotator DirRot = Direction.GetSafeNormal().Rotation();
	OutYaw = FRotator::NormalizeAxis(DirRot.Yaw - RefRot.Yaw);
	OutPitch = FRotator::NormalizeAxis(DirRot.Pitch - RefRot.Pitch);
}

void UTargetSelectionMath::EvaluateCandidate(FTargetCandidate& Candidate, const FVector& Origin, const FVector& ReferenceDirection,
	const FVector& SightForward, const FTargetSelectionTuning& Tuning)
{
	if (!Candidate.Target || !Candidate.Target->IsValidTarget())
	{
		Candidate.RejectReason = ETargetRejectReason::Invalid;
		return;
	}
	const FVector ToTarget = Candidate.Location - Origin;
	Candidate.Distance = ToTarget.Size();
	RelativeYawPitch(ReferenceDirection, ToTarget, Candidate.YawOffset, Candidate.PitchOffset);
	Candidate.AngleFromForward = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(SightForward.GetSafeNormal(), ToTarget.GetSafeNormal()), -1.0, 1.0)));

	if (Candidate.Distance > Tuning.MaximumTargetDistance)
	{
		Candidate.RejectReason = ETargetRejectReason::TooFar;
	}
	else if (Candidate.AngleFromForward > Tuning.MaximumTargetAngle)
	{
		Candidate.RejectReason = ETargetRejectReason::OutsideAngle;
	}
	else if (Tuning.MinimumTargetVisibility > 0.0f && Candidate.Visibility + KINDA_SMALL_NUMBER < Tuning.MinimumTargetVisibility)
	{
		Candidate.RejectReason = ETargetRejectReason::NotVisible;
	}
	else
	{
		Candidate.RejectReason = ETargetRejectReason::None;
	}
}

namespace
{
	/** Angular closeness to the reference, penalising vertical offset and distance. Lower is better. */
	float ClosenessScore(const FTargetCandidate& C, const FTargetSelectionTuning& Tuning)
	{
		return FMath::Abs(C.YawOffset) + Tuning.PitchWeight * FMath::Abs(C.PitchOffset) + Tuning.DistanceWeightPerMeter * C.Distance / 100.0f;
	}

	/** Index of the lowest score among candidates accepted by Filter; ties resolved by stable id (deterministic). */
	template <typename FilterType, typename ScoreType>
	int32 PickBest(const TArray<FTargetCandidate>& Candidates, FilterType Filter, ScoreType ScoreOf)
	{
		int32 Best = INDEX_NONE;
		float BestScore = 0.0f;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			const FTargetCandidate& C = Candidates[Index];
			if (!C.IsEligible() || !Filter(C))
			{
				continue;
			}
			const float Score = ScoreOf(C);
			const bool bBetter = Best == INDEX_NONE || Score < BestScore - 1e-3f
				|| (FMath::IsNearlyEqual(Score, BestScore, 1e-3f) && C.StableId < Candidates[Best].StableId);
			if (bBetter)
			{
				Best = Index;
				BestScore = Score;
			}
		}
		return Best;
	}
}

int32 UTargetSelectionMath::SelectSwitchTarget(const TArray<FTargetCandidate>& Candidates, int32 Direction, const UTargetableComponent* Current,
	const FTargetSelectionTuning& Tuning)
{
	const float Side = Direction >= 0 ? 1.0f : -1.0f;
	// Nearest step to the requested side.
	int32 Best = PickBest(Candidates,
		[&](const FTargetCandidate& C) { return C.Target != Current && C.YawOffset * Side >= Tuning.MinimumSwitchSeparation; },
		[&](const FTargetCandidate& C) { return ClosenessScore(C, Tuning); });

	if (Best == INDEX_NONE && Tuning.bWrapAround)
	{
		// Wrap around: the farthest candidate on the opposite side.
		Best = PickBest(Candidates,
			[&](const FTargetCandidate& C) { return C.Target != Current && C.YawOffset * Side <= -Tuning.MinimumSwitchSeparation; },
			[&](const FTargetCandidate& C) { return -FMath::Abs(C.YawOffset); });
	}
	return Best;
}

int32 UTargetSelectionMath::SelectNearestToReference(const TArray<FTargetCandidate>& Candidates, const FTargetSelectionTuning& Tuning)
{
	return PickBest(Candidates,
		[](const FTargetCandidate&) { return true; },
		[&](const FTargetCandidate& C) { return ClosenessScore(C, Tuning); });
}

bool UTankSightMath::ProjectToSight(const FTransform& SightTransform, float HorizontalFOVDegrees, float AspectRatio, const FVector& WorldLocation,
	FVector2D& OutUV, bool& bOnScreen)
{
	OutUV = FVector2D(0.5, 0.5);
	bOnScreen = false;
	const FVector Local = SightTransform.InverseTransformPositionNoScale(WorldLocation);
	if (Local.ContainsNaN() || Local.SizeSquared() < UE_SMALL_NUMBER)
	{
		return false;
	}

	const double TanHalfH = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(HorizontalFOVDegrees, 1.0f, 170.0f) * 0.5));
	const double TanHalfV = TanHalfH / FMath::Max(AspectRatio, 0.1f);
	const bool bInFront = Local.X > 1.0;

	FVector2D Ndc;
	if (bInFront)
	{
		Ndc = FVector2D((Local.Y / Local.X) / TanHalfH, (Local.Z / Local.X) / TanHalfV);
	}
	else
	{
		// Behind or beside the sight: no valid projection. Point toward the target from the centre instead.
		Ndc = FVector2D(Local.Y, Local.Z).GetSafeNormal() * 10.0;
		if (Ndc.IsNearlyZero())
		{
			Ndc = FVector2D(0.0, -10.0);
		}
	}

	bOnScreen = bInFront && FMath::Abs(Ndc.X) <= 1.0 && FMath::Abs(Ndc.Y) <= 1.0;
	if (!bOnScreen)
	{
		// Clamp to the edge while keeping the direction (the marker sits on the border, pointing at the target).
		const double Scale = 1.0 / FMath::Max(FMath::Abs(Ndc.X), FMath::Abs(Ndc.Y));
		Ndc *= FMath::Min(Scale, 1.0);
	}
	if (!FMath::IsFinite(Ndc.X) || !FMath::IsFinite(Ndc.Y))
	{
		Ndc = FVector2D::ZeroVector;
		bOnScreen = false;
	}
	OutUV = FVector2D(0.5 + 0.5 * Ndc.X, 0.5 - 0.5 * Ndc.Y);
	return bInFront;
}
