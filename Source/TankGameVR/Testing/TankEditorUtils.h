#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TankEditorUtils.generated.h"

class UUserWidget;

/**
 * Editor-only helpers used by the content setup scripts (Tools/TankSetup) for operations Python cannot do.
 * All functions do nothing outside the editor.
 */
UCLASS()
class TANKGAMEVR_API UTankEditorUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Inserts an instance of WidgetClass into the panel named PanelName of a Widget Blueprint (before the last child
	 * when bBeforeLast), copying the slot padding, button style and label font of the panel's first button so it matches.
	 * Does nothing if a widget named NewWidgetName already exists. Compiles the Blueprint. Returns true on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tank|Editor")
	static bool AddWidgetToWidgetBlueprint(UObject* WidgetBlueprint, FName PanelName, TSubclassOf<UUserWidget> WidgetClass, FName NewWidgetName, bool bBeforeLast);

	/** Names and classes of all widgets in a Widget Blueprint's tree (for verification). */
	UFUNCTION(BlueprintCallable, Category = "Tank|Editor")
	static TArray<FString> ListWidgetBlueprintWidgets(UObject* WidgetBlueprint);
};
