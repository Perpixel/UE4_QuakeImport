// Fill out your copyright notice in the Description page of Project Settings.

#include "GfxFactory.h"

// Epic
#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "PackageTools.h"
#include "Engine/Texture2D.h"
#include "Misc/FileHelper.h"
#include "UObject/Package.h"

// Quake
#include "QuakeCommon.h"

#define LOCTEXT_NAMESPACE "GfxFactory"

UGfxFactory::UGfxFactory(const FObjectInitializer& ObjectInitializer) :
    Super(ObjectInitializer)
{
    SupportedClass = UTexture2D::StaticClass();
    Formats.Add(TEXT("lmp;Quake lmp graphic files"));
    bCreateNew = false;
    bEditorImport = true;
}

UObject* UGfxFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn)
{
    // Load Palette
    TArray<QuakeCommon::QColor> quakePalette;

    if (!QuakeCommon::LoadPalette(quakePalette))
    {
        // ERROR
    }

    // Create Package
    FString packageName = TEXT("/Game/Graphics/Graphics");
    UPackage* package = CreatePackage(nullptr, *packageName);
    package->FullyLoad();

    int width = 0;
    int height = 0;

    QuakeCommon::ReadData<int>(Buffer, 0, width);
    QuakeCommon::ReadData<int>(Buffer, sizeof(int), height);

    if (width < 0 || height < 0 || width > 512 || height > 512)
    {
        return nullptr;
    }

    TArray<uint8> data;
    data.Append(Buffer + sizeof(int) * 2, width*height);
    UTexture2D* texture2D = QuakeCommon::CreateUTexture2D(Name.ToString(), width, height, data, *package, quakePalette);
    return texture2D;
}

#undef LOCTEXT_NAMESPACE