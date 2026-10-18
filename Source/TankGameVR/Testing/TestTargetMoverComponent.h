#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TestTargetMoverComponent.generated.h"

/** Test helper: slides the owner back and forth sideways so lock-on and crosshair tracking can be seen moving. */
UCLASS(ClassGroup = (Testing), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UTestTargetMoverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTestTargetMoverComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Mover", meta = (Units = "Centimeters"))
	float Amplitude = 400.0f;

	/** Full back-and-forth cycles per minute. */
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
