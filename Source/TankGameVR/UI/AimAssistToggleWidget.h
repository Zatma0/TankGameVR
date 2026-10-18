#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AimAssistToggleWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * "Aim Assist: On / Off" entry for the existing in-game menu. Drop it into any menu layout; it reads and writes
 * the persisted setting and updates its own label (works with the VR template's widget interaction laser).
 */
UCLASS()
class TANKGAMEVR_API UAimAssistToggleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim Assist", meta = (ClampMin = "8", ClampMax = "96"))
	int32 FontSize = 24;

	UFUNCTION(BlueprintCallable, Category = "Aim Assist")
	void Toggle();

	UFUNCTION(BlueprintPure, Category = "Aim Assist")
	FText GetLabel() const;

	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "Aim Assist", meta = (BindWidgetOptional))
	TObjectPtr<UButton> ToggleButton;

	UPROPERTY(BlueprintReadOnly, Category = "Aim Assist", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Label;

private:
	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleSettingChanged(bool bEnabled);

	void RefreshLabel();
};
