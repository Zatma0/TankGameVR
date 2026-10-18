#include "Settings/TankSettingsSubsystem.h"
#include "TankGameVR.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

const TCHAR* UTankSettingsSubsystem::DefaultSlotName = TEXT("TankPlayerSettings");

UTankSettingsSubsystem* UTankSettingsSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UTankSettingsSubsystem>() : nullptr;
}

void UTankSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadSettings();
}

void UTankSettingsSubsystem::LoadSettings()
{
	const UTankSettingsSaveGame* Save = UGameplayStatics::DoesSaveGameExist(SlotName, 0)
		? Cast<UTankSettingsSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0))
		: nullptr;
	// No save yet: defaults.
	bAimAssistEnabled = Save ? Save->bAimAssistEnabled : true;
}

bool UTankSettingsSubsystem::SaveSettings() const
{
	UTankSettingsSaveGame* Save = Cast<UTankSettingsSaveGame>(UGameplayStatics::CreateSaveGameObject(UTankSettingsSaveGame::StaticClass()));
	Save->bAimAssistEnabled = bAimAssistEnabled;
	const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
	if (!bSaved)
	{
		UE_LOG(LogTankGame, Warning, TEXT("Could not save tank settings to slot %s"), *SlotName);
	}
	return bSaved;
}

void UTankSettingsSubsystem::SetAimAssistEnabled(bool bEnabled)
{
	if (bAimAssistEnabled == bEnabled)
	{
		return;
	}
	bAimAssistEnabled = bEnabled;
	SaveSettings();
	OnAimAssistChanged.Broadcast(bAimAssistEnabled);
}
