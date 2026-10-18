#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TankSettingsSubsystem.generated.h"

// Saved player settings.
UCLASS()
class TANKGAMEVR_API UTankSettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	bool bAimAssistEnabled = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAimAssistSettingChanged, bool, bEnabled);

// Loads the settings at startup, saves on every change and notifies listeners.
UCLASS()
class TANKGAMEVR_API UTankSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UTankSettingsSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Tank|Settings")
	bool IsAimAssistEnabled() const { return bAimAssistEnabled; }

	UFUNCTION(BlueprintCallable, Category = "Tank|Settings")
	void SetAimAssistEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Tank|Settings")
	void ToggleAimAssist() { SetAimAssistEnabled(!bAimAssistEnabled); }

	UPROPERTY(BlueprintAssignable, Category = "Tank|Settings")
	FOnAimAssistSettingChanged OnAimAssistChanged;

		UFUNCTION(BlueprintCallable, Category = "Tank|Settings")
	void LoadSettings();

	UFUNCTION(BlueprintCallable, Category = "Tank|Settings")
	bool SaveSettings() const;

		void SetSaveSlotForTesting(const FString& InSlotName) { SlotName = InSlotName; }

	static const TCHAR* DefaultSlotName;

private:
	FString SlotName = DefaultSlotName;
	bool bAimAssistEnabled = true;
};
