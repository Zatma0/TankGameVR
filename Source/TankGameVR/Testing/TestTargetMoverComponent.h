#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TestTargetMoverComponent.generated.h"

// Moves the owner sideways back and forth (for testing lock-on on a moving target).
UCLASS(ClassGroup = (Testing), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UTestTargetMoverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTestTargetMoverComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Mover", meta = (Units = "Centimeters"))
	float Amplitude = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Mover", meta = (ClampMin = "0.0"))
	float CyclesPerMinute = 4.0f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	FVector StartLocation = FVector::ZeroVector;
	float Phase = 0.0f;
	float Elapsed = 0.0f;
};
