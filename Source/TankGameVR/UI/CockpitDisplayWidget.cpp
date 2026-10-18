#include "UI/CockpitDisplayWidget.h"
#include "Targeting/TankTargetingComponent.h"
#include "Targeting/TargetRegistrySubsystem.h"
#include "Targeting/TargetableComponent.h"
#include "Targeting/TargetSelectionMath.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

namespace
{
	const FVector2D CrosshairSize(96.0, 96.0);
	const FVector2D SelectedMarkerSize(120.0, 120.0);
	const FVector2D MarkerSize(64.0, 64.0);
}

bool UCockpitDisplayWidget::Initialize()
{
	const bool bResult = Super::Initialize();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
	return bResult;
}

UBorder* UCockpitDisplayWidget::MakeRing(const FLinearColor& Color, float Thickness, bool bCircle)
{
	UBorder* Ring = WidgetTree->ConstructWidget<UBorder>();
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FSlateColor(FLinearColor::Transparent);
	Brush.OutlineSettings.Width = Thickness;
	Brush.OutlineSettings.Color = FSlateColor(Color);
	Brush.OutlineSettings.RoundingType = bCircle ? ESlateBrushRoundingType::HalfHeightRadius : ESlateBrushRoundingType::FixedRadius;
	Brush.OutlineSettings.CornerRadii = FVector4(6.0, 6.0, 6.0, 6.0);
	Ring->SetBrush(Brush);
	return Ring;
}

void UCockpitDisplayWidget::SetRingColor(UBorder* Ring, const FLinearColor& Color) const
{
	if (Ring && !Ring->Background.OutlineSettings.Color.GetSpecifiedColor().Equals(Color))
	{
		FSlateBrush Brush = Ring->Background;
		Brush.OutlineSettings.Color = FSlateColor(Color);
		Ring->SetBrush(Brush);
	}
}

UTextBlock* UCockpitDisplayWidget::MakeText(int32 Size)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = Size;
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(IdleColor));
	return Text;
}

void UCockpitDisplayWidget::BuildDefaultLayout()
{
	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas"));
	WidgetTree->RootWidget = Canvas;

	// Dark screen background.
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetBrushColor(FLinearColor(0.01f, 0.03f, 0.02f, 0.95f));
	UCanvasPanelSlot* BackgroundSlot = Canvas->AddChildToCanvas(Background);
	BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BackgroundSlot->SetOffsets(FMargin(0.0f));

	// Fixed boresight (hull forward) so the player can see how far the aim is offset.
	UBorder* Boresight = MakeRing(InactiveColor, 2.0f, false);
	UCanvasPanelSlot* BoresightSlot = Canvas->AddChildToCanvas(Boresight);
	BoresightSlot->SetSize(FVector2D(24.0, 24.0));
	BoresightSlot->SetPosition(DisplaySize * 0.5 - FVector2D(12.0, 12.0));

	for (int32 Index = 0; Index < MaxTargetMarkers; ++Index)
	{
		UBorder* Marker = MakeRing(SelectableColor, 4.0f, false);
		Canvas->AddChildToCanvas(Marker)->SetSize(MarkerSize);
		Marker->SetVisibility(ESlateVisibility::Collapsed);
		Markers.Add(Marker);
	}

	Crosshair = MakeRing(IdleColor, 6.0f, true);
	Canvas->AddChildToCanvas(Crosshair)->SetSize(CrosshairSize);

	StatusText = MakeText(30);
	UCanvasPanelSlot* StatusSlot = Canvas->AddChildToCanvas(StatusText);
	StatusSlot->SetPosition(FVector2D(28.0, 20.0));
	StatusSlot->SetAutoSize(true);

	TargetText = MakeText(34);
	UCanvasPanelSlot* TargetSlot = Canvas->AddChildToCanvas(TargetText);
	TargetSlot->SetPosition(FVector2D(28.0, DisplaySize.Y - 70.0));
	TargetSlot->SetAutoSize(true);
}

void UCockpitDisplayWidget::SetTargeting(UTankTargetingComponent* InTargeting)
{
	Targeting = InTargeting;
	UpdateDisplay();
}

void UCockpitDisplayWidget::PlaceCentered(UWidget* Widget, const FVector2D& UV, const FVector2D& Size) const
{
	if (UCanvasPanelSlot* CanvasSlot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
	{
		CanvasSlot->SetSize(Size);
		CanvasSlot->SetPosition(UV * DisplaySize - Size * 0.5);
	}
}

void UCockpitDisplayWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateDisplay();
}

void UCockpitDisplayWidget::UpdateDisplay()
{
	const UTankTargetingComponent* Targeting_ = Targeting.Get();
	if (!Targeting_)
	{
		LastStatus = TEXT("NO SIGNAL");
		LastTargetLine.Reset();
		if (StatusText)
		{
			StatusText->SetText(FText::FromString(LastStatus));
		}
		return;
	}

	const FTransform Sight = Targeting_->GetSightTransform();
	const float Aspect = static_cast<float>(DisplaySize.X / FMath::Max(DisplaySize.Y, 1.0));
	const UTargetableComponent* Current = Targeting_->GetCurrentTarget();
	const bool bAssist = Targeting_->IsAimAssistEnabled();

	// Crosshair: projection of the actual aim direction, so it visibly glides onto the selected target.
	bool bCrosshairOnScreen = false;
	UTankSightMath::ProjectToSight(Sight, SightFieldOfView, Aspect, Targeting_->GetAimOrigin() + Targeting_->GetAimDirection() * 100000.0, CrosshairUV, bCrosshairOnScreen);
	PlaceCentered(Crosshair, CrosshairUV, CrosshairSize);
	SetRingColor(Crosshair, Current ? LockedColor : (bAssist ? IdleColor : ManualColor));

	// Target markers.
	TArray<UTargetableComponent*> Targets;
	if (UTargetRegistrySubsystem* Registry = UTargetRegistrySubsystem::Get(Targeting_))
	{
		Registry->GetValidTargets(Targets);
	}
	const FVector Origin = Targeting_->GetAimOrigin();
	const FVector SightForward = Sight.GetRotation().GetForwardVector();
	bool bCurrentOnScreen = false;
	int32 Used = 0;
	for (UTargetableComponent* Target : Targets)
	{
		if (Used >= Markers.Num() || Target->GetOwner() == Targeting_->GetOwner())
		{
			continue;
		}
		FVector2D UV;
		bool bOnScreen = false;
		UTankSightMath::ProjectToSight(Sight, SightFieldOfView, Aspect, Target->GetTargetLocation(), UV, bOnScreen);

		FTargetCandidate Candidate;
		Candidate.Target = Target;
		Candidate.Location = Target->GetTargetLocation();
		UTargetSelectionMath::EvaluateCandidate(Candidate, Origin, SightForward, SightForward, Targeting_->Selection);

		const bool bSelected = Target == Current;
		bCurrentOnScreen |= bSelected && bOnScreen;
		FLinearColor Color = InactiveColor;
		if (bSelected)
		{
			Color = bOnScreen ? LockedColor : LockedColor * 0.6f;
		}
		else if (bAssist && bOnScreen && Candidate.IsEligible())
		{
			Color = SelectableColor;
		}

		UBorder* Marker = Markers[Used++];
		Marker->SetVisibility(ESlateVisibility::HitTestInvisible);
		SetRingColor(Marker, Color);
		PlaceCentered(Marker, UV, bSelected ? SelectedMarkerSize : (bOnScreen ? MarkerSize : MarkerSize * 0.6));
	}
	for (int32 Index = Used; Index < Markers.Num(); ++Index)
	{
		Markers[Index]->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Text lines.
	LastStatus = FString::Printf(TEXT("AIM ASSIST: %s"), bAssist ? TEXT("ON") : TEXT("OFF"));
	if (Current)
	{
		const float Meters = static_cast<float>(FVector::Dist(Origin, Current->GetTargetLocation()) / 100.0);
		LastTargetLine = FString::Printf(TEXT("TARGET: %s  %.0f m%s"), *Current->GetTargetDisplayName().ToString(), Meters,
			bCurrentOnScreen ? TEXT("") : TEXT("  (OFF SCREEN)"));
	}
	else if (!bAssist)
	{
		LastTargetLine = TEXT("MANUAL AIM");
	}
	else
	{
		LastTargetLine = Targets.Num() > 0 ? TEXT("NO TARGET IN VIEW") : TEXT("NO TARGET");
	}
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(LastStatus));
		StatusText->SetColorAndOpacity(FSlateColor(bAssist ? SelectableColor : ManualColor));
	}
	if (TargetText)
	{
		TargetText->SetText(FText::FromString(LastTargetLine));
		TargetText->SetColorAndOpacity(FSlateColor(Current ? LockedColor : IdleColor));
	}
}
