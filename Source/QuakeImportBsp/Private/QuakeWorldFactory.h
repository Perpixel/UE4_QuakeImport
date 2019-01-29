// Copyright 2018 Guillaume Plourde. All Rights Reserved.
// https://github.com/Perpixel/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "RHIDefinitions.h"
#include "Factories/Factory.h"
#include "QuakeWorldFactory.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogQuakeImporter, Log, All);

UCLASS(MinimalAPI)
class UQuakeWorldFactory : public UFactory
{
    GENERATED_UCLASS_BODY()

    // FROM UFACTORY
    virtual bool ConfigureProperties() override;
    virtual UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn) override;
    virtual FText GetToolTip() const override;
    virtual FString GetToolTipDocumentationPage() const override;
    virtual FString GetToolTipDocumentationExcerpt() const override;
};
