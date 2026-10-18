#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TankSettingsSubsystem.generated.h"

/** Persisted player settings (SaveGame slot). Add new options here. */
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

/**
 * Player settings for the tank game. Loaded from disk when the game instance starts, saved on every change,
 * and broadcast so live systems (targeting, UI) react immediately.
 */
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

	/** Reloads from the save slot (used by tests and after external changes). */
	UFUNCTION(BlueprintCallable, Category = "Tank|Settings")
	void LoadSettings();

	UFUNCTION(BlueprintCallable, Category = "Tank|Settings")
	bool SaveSettings() const;

	/** Lets tests use their own slot instead of the player's real settings. */
	void SetSaveSlotForTesting(const FString& InSlotName) { SlotName = InSlotName; }

	static const TCHAR* DefaultSlotName;

private:
	FString SlotName = DefaultSlotName;
	bool bAimAssistEnabled = true;
};
