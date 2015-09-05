// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WidgetAnimationBinding.generated.h"


/**
 * A single object bound to a UMG sequence.
 */
USTRUCT()
struct FWidgetAnimationBinding
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName WidgetName;

	UPROPERTY()
	FName SlotWidgetName;

	UPROPERTY()
	FGuid AnimationGuid;

public:

	/**
	 * Locates a runtime object to animate from the provided tree of widgets
	 * @return the runtime object to animate or null if not found 
	 */
	UMG_API UObject* FindRuntimeObject(UWidgetTree& WidgetTree) const;

	/**
	 * Compares two widget animation bindings for equality.
	 *
	 * @param X The first binding to compare.
	 * @param Y The second binding to compare.
	 * @return true if the bindings are equal, false otherwise.
	 */
	friend bool operator==(const FWidgetAnimationBinding& X, const FWidgetAnimationBinding& Y)
	{
		return (X.WidgetName == Y.WidgetName) && (X.SlotWidgetName == Y.SlotWidgetName) && (X.AnimationGuid == Y.AnimationGuid);
	}

	/**
	 * Serializes a widget animation binding from/to the given archive.
	 *
	 * @param Ar The archive to serialize to/from.
	 * @param Binding the binding to serialize.
	 */
	friend FArchive& operator<<(FArchive& Ar, FWidgetAnimationBinding& Binding)
	{
		Ar << Binding.WidgetName;
		Ar << Binding.SlotWidgetName;
		Ar << Binding.AnimationGuid;
		return Ar;
	}
};