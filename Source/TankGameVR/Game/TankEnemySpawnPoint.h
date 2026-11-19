#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TankEnemySpawnPoint.generated.h"

class UArrowComponent;
class UBillboardComponent;

/** Drop these around the arena (behind cover, at the edges). The arena director spawns enemies here. */
UCLASS(Blueprintable)
class TANKGAMEVR_API ATankEnemySpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	ATankEnemySpawnPoint();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	bool bAllowTanks = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	bool bAllowArmoredCars = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	bool bAllowAntiTankGuns = true;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;

	UPROPERTY()
	TObjectPtr<UArrowComponent> Arrow;
#endif
};
