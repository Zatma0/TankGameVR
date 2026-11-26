#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "TankPlaceholderEnemyStub.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlaceholderEnemyDied);

/**
 * Stands in for another spawner's own enemy class in the automation tests: a type variable set on spawn and a
 * "died" event dispatcher, the two things UTankLevelSetupSubsystem needs from such a class.
 */
UCLASS(NotPlaceable)
class ATankPlaceholderEnemyStub : public AActor
{
	GENERATED_BODY()

public:
	ATankPlaceholderEnemyStub()
	{
		SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	}

	/** 0 tank, 1 armored car, 2 AT gun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test", meta = (ExposeOnSpawn = "true"))
	uint8 SpawnedEnemyType = 0;

	UPROPERTY(BlueprintAssignable, Category = "Test")
	FOnPlaceholderEnemyDied OnEnemyDied;

	/** How often any stub's OnEnemyDied fired. */
	static inline int32 TotalDied = 0;

	UFUNCTION()
	void CountDied()
	{
		++TotalDied;
	}

protected:
	virtual void BeginPlay() override
	{
		Super::BeginPlay();
		OnEnemyDied.AddDynamic(this, &ATankPlaceholderEnemyStub::CountDied);
	}
};
