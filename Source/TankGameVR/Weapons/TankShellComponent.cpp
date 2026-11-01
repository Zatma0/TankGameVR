#include "Weapons/TankShellComponent.h"

UTankShellComponent::UTankShellComponent()
{
}

FVector UTankShellComponent::GetNoseLocation() const
{
	return GetComponentTransform().TransformPositionNoScale(FVector(NoseOffset, 0.0, 0.0));
}

void UTankShellComponent::Consume()
{
	SetAway(true);
	RefillRemaining = RefillDelay > 0.0f ? RefillDelay : -1.0f;
}

void UTankShellComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (RefillRemaining > 0.0f)
	{
		RefillRemaining -= DeltaTime;
		if (RefillRemaining <= 0.0f)
		{
			RefillRemaining = -1.0f;
			SetAway(false);
		}
	}
}
