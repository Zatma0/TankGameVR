#include "Testing/TankEditorUtils.h"
#include "UI/AimAssistToggleWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"

#if WITH_EDITOR
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "WidgetBlueprint.h"
#endif

bool UTankEditorUtils::AddWidgetToWidgetBlueprint(UObject* WidgetBlueprint, FName PanelName, TSubclassOf<UUserWidget> WidgetClass, FName NewWidgetName, bool bBeforeLast)
{
#if WITH_EDITOR
	UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(WidgetBlueprint);
	if (!Blueprint || !Blueprint->WidgetTree || !WidgetClass)
	{
		return false;
	}
	if (Blueprint->WidgetTree->FindWidget(NewWidgetName))
	{
		return true;
	}
	UPanelWidget* Panel = Cast<UPanelWidget>(Blueprint->WidgetTree->FindWidget(PanelName));
	if (!Panel)
	{
		return false;
	}

	Blueprint->Modify();
	Blueprint->WidgetTree->SetFlags(RF_Transactional);
	Blueprint->WidgetTree->Modify();

	UUserWidget* NewWidget = Blueprint->WidgetTree->ConstructWidget<UUserWidget>(WidgetClass, NewWidgetName);

	// Look like the menu's existing buttons.
	UButton* ReferenceButton = nullptr;
	for (int32 Index = 0; Index < Panel->GetChildrenCount() && !ReferenceButton; ++Index)
	{
		ReferenceButton = Cast<UButton>(Panel->GetChildAt(Index));
	}
	if (UAimAssistToggleWidget* Toggle = Cast<UAimAssistToggleWidget>(NewWidget); Toggle && ReferenceButton)
	{
		Toggle->bUseCustomStyle = true;
		Toggle->ButtonStyle = ReferenceButton->GetStyle();
		if (const UTextBlock* ReferenceText = Cast<UTextBlock>(ReferenceButton->GetChildAt(0)))
		{
			Toggle->LabelFont = ReferenceText->GetFont();
			Toggle->LabelColor = ReferenceText->GetColorAndOpacity();
		}
	}

	const int32 InsertIndex = bBeforeLast ? FMath::Max(Panel->GetChildrenCount() - 1, 0) : Panel->GetChildrenCount();
	UPanelSlot* NewSlot = Panel->InsertChildAt(InsertIndex, NewWidget);
	if (UVerticalBoxSlot* NewVerticalSlot = Cast<UVerticalBoxSlot>(NewSlot))
	{
		if (const UVerticalBoxSlot* ReferenceSlot = ReferenceButton ? Cast<UVerticalBoxSlot>(ReferenceButton->Slot) : nullptr)
		{
			NewVerticalSlot->SetPadding(ReferenceSlot->GetPadding());
			NewVerticalSlot->SetHorizontalAlignment(ReferenceSlot->GetHorizontalAlignment());
			NewVerticalSlot->SetVerticalAlignment(ReferenceSlot->GetVerticalAlignment());
			NewVerticalSlot->SetSize(ReferenceSlot->GetSize());
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	return true;
#else
	return false;
#endif
}

TArray<FString> UTankEditorUtils::ListWidgetBlueprintWidgets(UObject* WidgetBlueprint)
{
	TArray<FString> Result;
#if WITH_EDITOR
	if (const UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(WidgetBlueprint); Blueprint && Blueprint->WidgetTree)
	{
		Blueprint->WidgetTree->ForEachWidget([&Result](UWidget* Widget)
		{
			Result.Add(FString::Printf(TEXT("%s:%s"), *Widget->GetName(), *Widget->GetClass()->GetName()));
		});
	}
#endif
	return Result;
}
