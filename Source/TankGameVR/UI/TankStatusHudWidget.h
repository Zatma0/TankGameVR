#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TankStatusHudWidget.generated.h"

class ATankVehicle;
class UTextBlock;

/**
 * Small on-screen status panel for the overview camera: speed, turn rate, Aim Assist, current target and how many
 * targets are around. Builds a placeholder layout itself; a Blueprint subclass may provide its own text block named
 * StatusText instead.
 */
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

	/** Text shown after the last update. */
	UFUNCTION(BlueprintPure, Category = "Status")
	FString GetStatusString() const { return LastStatus; }

	/** Runs one update immediately (normally done every tick). */
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
