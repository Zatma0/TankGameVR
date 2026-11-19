#include "Enemies/TankEnemyStats.h"
#include "TankGameVR.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr float KmhToCms = 100000.0f / 3600.0f;

	bool ReadNumber(const FProperty* Property, const void* Value, float& Out)
	{
		if (const FNumericProperty* Numeric = CastField<FNumericProperty>(Property))
		{
			Out = Numeric->IsFloatingPoint() ? static_cast<float>(Numeric->GetFloatingPointPropertyValue(Value))
				: static_cast<float>(Numeric->GetSignedIntPropertyValue(Value));
			return true;
		}
		return false;
	}
}

FName UTankEnemyStatsLibrary::GetDefaultRowName(ETankEnemyType Type)
{
	switch (Type)
	{
	case ETankEnemyType::ArmoredCar:
		return TEXT("ArmoredCar_Row");
	case ETankEnemyType::AntiTank:
		return TEXT("AntiTank_Row");
	default:
		return TEXT("Tank_Row");
	}
}

bool UTankEnemyStatsLibrary::ReadEnemyStats(const UDataTable* Table, FName RowName, FTankEnemyStats& OutStats)
{
	if (!Table || !Table->GetRowStruct())
	{
		return false;
	}
	const uint8* Row = Table->FindRowUnchecked(RowName);
	if (!Row)
	{
		UE_LOG(LogTankGame, Warning, TEXT("%s has no row %s, using defaults"), *Table->GetName(), *RowName.ToString());
		return false;
	}

	for (TFieldIterator<FProperty> It(Table->GetRowStruct()); It; ++It)
	{
		const FProperty* Property = *It;
		// Blueprint struct members are stored as E_HP_11_<guid>; the authored name is just E_HP.
		const FString Name = Property->GetAuthoredName();
		const void* Value = Property->ContainerPtrToValuePtr<void>(Row);
		float Number = 0.0f;
		if (Name == TEXT("E_HP") && ReadNumber(Property, Value, Number))
		{
			OutStats.Health = FMath::Max(1.0f, Number);
		}
		else if (Name == TEXT("E_Damage") && ReadNumber(Property, Value, Number))
		{
			OutStats.Damage = FMath::Max(0.0f, Number);
		}
		else if (Name == TEXT("E_Speed") && ReadNumber(Property, Value, Number))
		{
			OutStats.Speed = FMath::Max(0.0f, Number) * KmhToCms;
		}
		else if (Name == TEXT("E_Range") && ReadNumber(Property, Value, Number))
		{
			OutStats.Range = FMath::Max(1.0f, Number) * 100.0f;
		}
		else if (Name == TEXT("E_Model"))
		{
			// Soft first: a soft property is also an object property.
			if (const FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property))
			{
				OutStats.Model = Cast<UStaticMesh>(Soft->GetPropertyValue(Value).LoadSynchronous());
			}
			else if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
			{
				OutStats.Model = Cast<UStaticMesh>(Object->GetObjectPropertyValue(Value));
			}
		}
	}
	return true;
}
