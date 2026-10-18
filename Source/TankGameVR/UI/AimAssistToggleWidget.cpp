#include "UI/AimAssistToggleWidget.h"
#include "Settings/TankSettingsSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

bool UAimAssistToggleWidget::Initialize()
{
	const bool bResult = Super::Initialize();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		ToggleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ToggleButton"));
		Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label"));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = FontSize;
		Label->SetFont(Font);
		Label->SetJustification(ETextJustify::Center);
		ToggleButton->AddChild(Label);
		WidgetTree->RootWidget = ToggleButton;
	}
	if (bUseCustomStyle && ToggleButton && Label)
	{
		ToggleButton->SetStyle(ButtonStyle);
		Label->SetFont(LabelFont);
		Label->SetColorAndOpacity(LabelColor);
	}
	return bResult;
}

void UAimAssistToggleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ToggleButton)
	{
		ToggleButton->OnClicked.AddUniqueDynamic(this, &UAimAssistToggleWidget::HandleClicked);
	}
	if (UTankSettingsSubsystem* Settings = UTankSettingsSubsystem::Get(this))
	{
		Settings->OnAimAssistChanged.AddUniqueDynamic(this, &UAimAssistToggleWidget::HandleSettingChanged);
	}
	RefreshLabel();
}

void UAimAssistToggleWidget::NativeDestruct()
{
	if (UTankSettingsSubsystem* Settings = UTankSettingsSubsystem::Get(this))
	{
		Settings->OnAimAssistChanged.RemoveDynamic(this, &UAimAssistToggleWidget::HandleSettingChanged);
	}
	Super::NativeDestruct();
}

void UAimAssistToggleWidget::Toggle()
{
	if (UTankSettingsSubsystem* Settings = UTankSettingsSubsystem::Get(this))
	{
		Settings->ToggleAimAssist();
	}
	RefreshLabel();
}

FText UAimAssistToggleWidget::GetLabel() const
{
	const UTankSettingsSubsystem* Settings = UTankSettingsSubsystem::Get(this);
	const bool bEnabled = !Settings || Settings->IsAimAssistEnabled();
	return FText::FromString(bEnabled ? TEXT("Aim Assist: On") : TEXT("Aim Assist: Off"));
}

void UAimAssistToggleWidget::HandleClicked()
{
	Toggle();
}

void UAimAssistToggleWidget::HandleSettingChanged(bool bEnabled)
{
	RefreshLabel();
}

void UAimAssistToggleWidget::RefreshLabel()
{
	if (Label)
	{
		Label->SetText(GetLabel());
	}
}
