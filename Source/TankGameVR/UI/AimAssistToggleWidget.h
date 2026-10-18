#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"
#include "AimAssistToggleWidget.generated.h"

class UButton;
class UTextBlock;

// "Aim Assist: On/Off" button for the in-game menu.
UCLASS()
class TANKGAMEVR_API UAimAssistToggleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim Assist", meta = (ClampMin = "8", ClampMax = "96"))
	int32 FontSize = 24;

	/** Use the style below (to match the menu) instead of the default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim Assist|Style")
	bool bUseCustomStyle = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim Assist|Style", meta = (EditCondition = "bUseCustomStyle"))
	FButtonStyle ButtonStyle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim Assist|Style", meta = (EditCondition = "bUseCustomStyle"))
	FSlateFontInfo LabelFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim Assist|Style", meta = (EditCondition = "bUseCustomStyle"))
	FSlateColor LabelColor = FSlateColor(FLinearColor::White);

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
