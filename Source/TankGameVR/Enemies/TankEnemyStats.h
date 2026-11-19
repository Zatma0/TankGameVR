#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TankEnemyStats.generated.h"

class UDataTable;
class UStaticMesh;

/** Same order as Ugurkan's E_EnemyTypes. */
UENUM(BlueprintType)
enum class ETankEnemyType : uint8
{
	Tank,
	ArmoredCar,
	AntiTank
};

/** One row of DT_EnemyFeatures, converted to engine units. */
USTRUCT(BlueprintType)
struct TANKGAMEVR_API FTankEnemyStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	float Health = 50.0f;

	/** Per shot (the armored car splits it over a burst). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	float Damage = 20.0f;

	/** cm/s (the table is in km/h). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy", meta = (Units = "CentimetersPerSecond"))
	float Speed = 280.0f;

	/** cm (the table is in metres). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy", meta = (Units = "Centimeters"))
	float Range = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	TObjectPtr<UStaticMesh> Model;
};

UCLASS()
class TANKGAMEVR_API UTankEnemyStatsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Row name in DT_EnemyFeatures for a type (Tank_Row, ArmoredCar_Row, AntiTank_Row). */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	static FName GetDefaultRowName(ETankEnemyType Type);

	/**
	 * Reads E_HP, E_Damage, E_Speed (km/h), E_Range (m) and E_Model from a row. Columns are found by their
	 * display name, so the table can be a Blueprint struct. Missing columns keep the values already in OutStats.
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy")
	static bool ReadEnemyStats(const UDataTable* Table, FName RowName, UPARAM(ref) FTankEnemyStats& OutStats);
};
