#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CockpitDisplayWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UTankTargetingComponent;
class UTextBlock;

/**
 * Cockpit sight display. Shows what the targeting system does: a crosshair at the projected aim direction (it glides
 * onto the locked target), markers for every target (selected / selectable / out of view), Aim Assist state and the
 * current target's name and distance. Rendered on a cockpit screen through a Widget Component.
 *
 * The widget builds a placeholder layout itself; a Blueprint subclass may provide its own widgets instead by naming
 * them Crosshair, StatusText and TargetText. Updating only moves existing widgets (no rebuilds per frame).
 */
UCLASS()
class TANKGAMEVR_API UCockpitDisplayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Display resolution in slate units; must match the Widget Component's Draw Size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FVector2D DisplaySize = FVector2D(1024.0, 768.0);

	/** Horizontal field of view (deg) of the virtual sight shown on the display. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display", meta = (ClampMin = "10.0", ClampMax = "160.0"))
	float SightFieldOfView = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display", meta = (ClampMin = "0", ClampMax = "32"))
	int32 MaxTargetMarkers = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display|Colors")
	FLinearColor LockedColor = FLinearColor(1.0f, 0.15f, 0.1f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display|Colors")
	FLinearColor SelectableColor = FLinearColor(1.0f, 0.85f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display|Colors")
	FLinearColor InactiveColor = FLinearColor(0.45f, 0.5f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display|Colors")
	FLinearColor ManualColor = FLinearColor(0.3f, 0.9f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display|Colors")
	FLinearColor IdleColor = FLinearColor(0.85f, 0.95f, 0.85f);

	/** Connect the display to the tank's targeting system. */
	UFUNCTION(BlueprintCallable, Category = "Display")
	void SetTargeting(UTankTargetingComponent* InTargeting);

	/** Crosshair position in display UV (0..1) after the last update. */
	UFUNCTION(BlueprintPure, Category = "Display")
	FVector2D GetCrosshairUV() const { return CrosshairUV; }

	UFUNCTION(BlueprintPure, Category = "Display")
	FString GetStatusString() const { return LastStatus; }

	UFUNCTION(BlueprintPure, Category = "Display")
	FString GetTargetString() const { return LastTargetLine; }

	/** Runs one update immediately (normally done every tick). */
	UFUNCTION(BlueprintCallable, Category = "Display")
	void UpdateDisplay();

	virtual bool Initialize() override;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category = "Display", meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> Canvas;

	UPROPERTY(BlueprintReadOnly, Category = "Display", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Crosshair;

	UPROPERTY(BlueprintReadOnly, Category = "Display", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(BlueprintReadOnly, Category = "Display", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TargetText;

private:
	void BuildDefaultLayout();
	UBorder* MakeRing(const FLinearColor& Color, float Thickness, bool bCircle);
	UTextBlock* MakeText(int32 Size);
	void PlaceCentered(UWidget* Widget, const FVector2D& UV, const FVector2D& Size) const;
	void SetRingColor(UBorder* Ring, const FLinearColor& Color) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> Markers;

	TWeakObjectPtr<UTankTargetingComponent> Targeting;
	FVector2D CrosshairUV = FVector2D(0.5, 0.5);
	FString LastStatus;
	FString LastTargetLine;
};
