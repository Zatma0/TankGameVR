#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TankWaveHudWidget.generated.h"

class ATankArenaDirector;
class UTextBlock;

/**
 * Wave panel for the monitor: wave, time to the next wave and the enemies still out by type.
 * Reads the arena director, so it works with our waves and with a level spawner that reports to it.
 * Builds its own texts when the Widget Blueprint has none; a designed WBP can bind the Get...Text functions instead.
 */
UCLASS()
class TANKGAMEVR_API UTankWaveHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave HUD")
	FLinearColor TextColor = FLinearColor(0.85f, 0.95f, 0.85f);

	/** Timer colour in the last seconds before a wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave HUD")
	FLinearColor SoonColor = FLinearColor(1.0f, 0.55f, 0.15f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave HUD")
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.55f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave HUD", meta = (Units = "Seconds"))
	float SoonTime = 5.0f;

	/** "WAVE 3", or "GET READY" before the first one. */
	UFUNCTION(BlueprintPure, Category = "Wave HUD")
	FText GetWaveText() const { return FText::FromString(WaveLine); }

	/** "NEXT WAVE 0:24", empty when there is no timer. */
	UFUNCTION(BlueprintPure, Category = "Wave HUD")
	FText GetTimerText() const { return FText::FromString(TimerLine); }

	/** "TANKS 1   CARS 2   AT GUNS 0   SCORE 150", or the game over line. */
	UFUNCTION(BlueprintPure, Category = "Wave HUD")
	FText GetEnemiesText() const { return FText::FromString(EnemiesLine); }

	UFUNCTION(BlueprintCallable, Category = "Wave HUD")
	void UpdateTexts();

	/** Creates the panel and puts it on the screen (once per player). */
	UFUNCTION(BlueprintCallable, Category = "Wave HUD", meta = (WorldContext = "WorldContext", DeterminesOutputType = "WidgetClass"))
	static UTankWaveHudWidget* ShowWaveHud(const UObject* WorldContext, TSubclassOf<UTankWaveHudWidget> WidgetClass);

	virtual bool Initialize() override;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category = "Wave HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WaveText;

	UPROPERTY(BlueprintReadOnly, Category = "Wave HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TimerText;

	UPROPERTY(BlueprintReadOnly, Category = "Wave HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EnemiesText;

private:
	void BuildDefaultLayout();
	UTextBlock* MakeText(FName Name, int32 Size);

	FString WaveLine;
	FString TimerLine;
	FString EnemiesLine;
	bool bSoon = false;
};
