// Copyright 2015-2017 Piperift. All Rights Reserved.

#include "UtilityTree/UTBlueprintNodeOptionalPinManager.h"
#include "UObject/UnrealType.h"
#include "Kismet2/BlueprintEditorUtils.h"

#include "UtilityTree/UtilityTreeGraphSchema.h"
#include "UtilityTree/AIGraphNode_Base.h"


FUTBlueprintNodeOptionalPinManager::FUTBlueprintNodeOptionalPinManager(class UAIGraphNode_Base* Node, TArray<UEdGraphPin*>* InOldPins)
	: BaseNode(Node)
	, OldPins(InOldPins)
{
	if (OldPins != NULL)
	{
		for (auto PinIt = OldPins->CreateIterator(); PinIt; ++PinIt)
		{
			UEdGraphPin* Pin = *PinIt;
			OldPinMap.Add(Pin->PinName, Pin);
		}
	}
}

void FUTBlueprintNodeOptionalPinManager::GetRecordDefaults(UProperty* TestProperty, FOptionalPinFromProperty& Record) const
{
	const UUtilityTreeGraphSchema* Schema = GetDefault<UUtilityTreeGraphSchema>();

	// Determine if this is a pose or array of poses
	UArrayProperty* ArrayProp = Cast<UArrayProperty>(TestProperty);
	UStructProperty* StructProp = Cast<UStructProperty>((ArrayProp != NULL) ? ArrayProp->Inner : TestProperty);
	const bool bIsAIInput = (StructProp != NULL) && (StructProp->Struct->IsChildOf(FAILinkBase::StaticStruct()));

	//@TODO: Error if they specified two or more of these flags
	const bool bAlwaysShow = TestProperty->HasMetaData(Schema->NAME_AlwaysAsPin) || bIsAIInput;
	const bool bOptional_ShowByDefault = TestProperty->HasMetaData(Schema->NAME_PinShownByDefault);
	const bool bOptional_HideByDefault = TestProperty->HasMetaData(Schema->NAME_PinHiddenByDefault);
	const bool bNeverShow = TestProperty->HasMetaData(Schema->NAME_NeverAsPin);
	const bool bPropertyIsCustomized = TestProperty->HasMetaData(Schema->NAME_CustomizeProperty);

	Record.bCanToggleVisibility = bOptional_ShowByDefault || bOptional_HideByDefault;
	Record.bShowPin = bAlwaysShow || bOptional_ShowByDefault;
	Record.bPropertyIsCustomized = bPropertyIsCustomized;
}

void FUTBlueprintNodeOptionalPinManager::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex, UProperty* Property) const
{
	if (BaseNode != nullptr)
	{
		BaseNode->CustomizePinData(Pin, SourcePropertyName, ArrayIndex);
	}
}

void FUTBlueprintNodeOptionalPinManager::PostInitNewPin(UEdGraphPin* Pin, FOptionalPinFromProperty& Record, int32 ArrayIndex, UProperty* Property, uint8* PropertyAddress, uint8* DefaultPropertyAddress) const
{
	check(PropertyAddress != nullptr);
	check(Record.bShowPin);
	
	const UUtilityTreeGraphSchema* Schema = GetDefault<UUtilityTreeGraphSchema>();

	// In all cases set autogenerated default value from node defaults
	if (DefaultPropertyAddress)
	{
		FString LiteralValue;
		FBlueprintEditorUtils::PropertyValueToString_Direct(Property, DefaultPropertyAddress, LiteralValue);

		Schema->SetPinAutogeneratedDefaultValue(Pin, LiteralValue);
	}
	else
	{
		Schema->SetPinAutogeneratedDefaultValueBasedOnType(Pin);
	}

	if (OldPins == nullptr)
	{
		// Initial construction of a visible pin; copy values from the struct
		FString LiteralValue;
		FBlueprintEditorUtils::PropertyValueToString_Direct(Property, PropertyAddress, LiteralValue);

		Schema->SetPinDefaultValueAtConstruction(Pin, LiteralValue);
	}
	else if (Record.bCanToggleVisibility)
	{
		if (UEdGraphPin* OldPin = OldPinMap.FindRef(Pin->PinName))
		{
			// Was already visible, values will get copied over in pin reconstruction code
		}
		else
		{
			// Convert the struct property into DefaultValue/DefaultValueObject
			FString LiteralValue;
			FBlueprintEditorUtils::PropertyValueToString_Direct(Property, PropertyAddress, LiteralValue);

			Schema->SetPinDefaultValueAtConstruction(Pin, LiteralValue);
		}

		// Clear the asset reference on the node if the pin exists
		// In theory this is only needed for pins that are newly created but there are a lot of existing nodes that have dead asset references
		UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property);
		if (ObjectProperty)
		{
			ObjectProperty->SetObjectPropertyValue(PropertyAddress, nullptr);
		}
	}
}

void FUTBlueprintNodeOptionalPinManager::PostRemovedOldPin(FOptionalPinFromProperty& Record, int32 ArrayIndex, UProperty* Property, uint8* PropertyAddress, uint8* DefaultPropertyAddress) const
{
	check(PropertyAddress != nullptr);
	check(!Record.bShowPin);

	if (Record.bCanToggleVisibility && (OldPins != nullptr))
	{
		const FString OldPinName = (ArrayIndex != INDEX_NONE) ? FString::Printf(TEXT("%s_%d"), *(Record.PropertyName.ToString()), ArrayIndex) : Record.PropertyName.ToString();
		// Pin was visible but it's now hidden
		if (UEdGraphPin* OldPin = OldPinMap.FindRef(OldPinName))
		{
			// Convert DefaultValue/DefaultValueObject and push back into the struct
			FBlueprintEditorUtils::PropertyValueFromString_Direct(Property, OldPin->GetDefaultAsString(), PropertyAddress);
		}
	}
}

void FUTBlueprintNodeOptionalPinManager::AllocateDefaultPins(UStruct* SourceStruct, uint8* StructBasePtr, uint8* DefaultsPtr)
{
	RebuildPropertyList(BaseNode->ShowPinForProperties, SourceStruct);
	CreateVisiblePins(BaseNode->ShowPinForProperties, SourceStruct, EGPD_Input, BaseNode, StructBasePtr, DefaultsPtr);
}
