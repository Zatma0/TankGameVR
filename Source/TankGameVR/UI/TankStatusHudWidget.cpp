#include "UI/TankStatusHudWidget.h"
#include "Tank/TankMovementComponent.h"
#include "Tank/TankVehicle.h"
#include "Targeting/TankTargetingComponent.h"
#include "Targeting/TargetRegistrySubsystem.h"
#include "Targeting/TargetableComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

bool UTankStatusHudWidget::Initialize()
{
	const bool bResult = Super::Initialize();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
	return bResult;
}

void UTankStatusHudWidget::BuildDefaultLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(BackgroundColor);
	Panel->SetPadding(FMargin(16.0f, 10.0f));

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	FSlateFontInfo Font = StatusText->GetFont();
	Font.Size = 16;
	StatusText->SetFont(Font);
	StatusText->SetColorAndOpacity(FSlateColor(TextColor));
	Panel->SetContent(StatusText);

	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAutoSize(true);
	PanelSlot->SetPosition(FVector2D(24.0, 24.0));
}

void UTankStatusHudWidget::SetTank(ATankVehicle* InTank)
{
	Tank = InTank;
	UpdateStatus();
}

void UTankStatusHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateStatus();
}

void UTankStatusHudWidget::UpdateStatus()
{
	const ATankVehicle* Tank_ = Tank.Get();
	if (!Tank_)
	{
		LastStatus = TEXT("NO TANK");
	}
	else
	{
		const UTankMovementComponent* Movement = Tank_->GetTankMovement();
		const UTankTargetingComponent* Targeting = Tank_->GetTargeting();
		const float SpeedKmh = Movement ? Movement->GetCurrentSpeed() * 0.036f : 0.0f;
		const float TurnRate = Movement ? Movement->GetCurrentTurnRate() : 0.0f;

		LastStatus = TEXT("OVERVIEW CAMERA   [C / X: cockpit view]\n");
		LastStatus += FString::Printf(TEXT("SPEED: %.0f km/h%s   TURN: %.0f deg/s\n"), FMath::Abs(SpeedKmh), SpeedKmh < -0.5f ? TEXT(" (reverse)") : TEXT(""), TurnRate);

		if (Targeting)
		{
			const bool bAssist = Targeting->IsAimAssistEnabled();
			LastStatus += FString::Printf(TEXT("AIM ASSIST: %s\n"), bAssist ? TEXT("ON") : TEXT("OFF"));
			if (const UTargetableComponent* Current = Targeting->GetCurrentTarget())
			{
				const float Meters = static_cast<float>(FVector::Dist(Targeting->GetAimOrigin(), Current->GetTargetLocation()) / 100.0);
				LastStatus += FString::Printf(TEXT("TARGET: %s  %.0f m\n"), *Current->GetTargetDisplayName().ToString(), Meters);
			}
			else
			{
				LastStatus += bAssist ? TEXT("TARGET: none\n") : TEXT("TARGET: manual aim\n");
			}
		}

		int32 TargetCount = 0;
		if (UTargetRegistrySubsystem* Registry = UTargetRegistrySubsystem::Get(Tank_))
		{
			TargetCount = Registry->GetValidTargetCount();
		}
		LastStatus += FString::Printf(TEXT("TARGETS IN AREA: %d"), TargetCount);
	}

	if (StatusText)
	{
		StatusText->SetText(FText::FromString(LastStatus));
	}
}
