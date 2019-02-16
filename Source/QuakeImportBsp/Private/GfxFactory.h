// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "GfxFactory.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI)
class UGfxFactory : public UFactory
{
    GENERATED_UCLASS_BODY()

        // FROM UFACTORY
        virtual UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn) override;
};
