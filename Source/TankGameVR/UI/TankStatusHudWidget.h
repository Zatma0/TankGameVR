#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TankStatusHudWidget.generated.h"

class ATankVehicle;
class UTextBlock;

// Status panel for the overview camera (speed, Aim Assist, target).
UCLASS()
class TANKGAMEVR_API UTankStatusHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status")
	FLinearColor TextColor = FLinearColor(0.85f, 0.95f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status")
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.55f);

	UFUNCTION(BlueprintCallable, Category = "Status")
	void SetTank(ATankVehicle* InTank);

	UFUNCTION(BlueprintPure, Category = "Status")
	FString GetStatusString() const { return LastStatus; }

	UFUNCTION(BlueprintCallable, Category = "Status")
	void UpdateStatus();

	virtual bool Initialize() override;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category = "Status", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	void BuildDefaultLayout();

	TWeakObjectPtr<ATankVehicle> Tank;
	FString LastStatus;
};
