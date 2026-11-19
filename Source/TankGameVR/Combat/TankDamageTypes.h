#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "TankDamageTypes.generated.h"

UENUM(BlueprintType)
enum class ETankRoundType : uint8
{
	/** Armour piercing: one hard point hit. Best against tanks. */
	AP,
	/** High explosive: smaller hit plus blast. Best against light vehicles and guns. */
	HE
};

UCLASS()
class TANKGAMEVR_API UTankDamageType_AP : public UDamageType
{
	GENERATED_BODY()
};

UCLASS()
class TANKGAMEVR_API UTankDamageType_HE : public UDamageType
{
	GENERATED_BODY()
};

/** Armored car autocannon. */
UCLASS()
class TANKGAMEVR_API UTankDamageType_Autocannon : public UDamageType
{
	GENERATED_BODY()
};

/** Burning inside the tank. */
UCLASS()
class TANKGAMEVR_API UTankDamageType_Fire : public UDamageType
{
	GENERATED_BODY()
};
