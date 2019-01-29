// Copyright 2018 Guillaume Plourde. All Rights Reserved.
// https://github.com/Perpixel/

#include "QuakeWorldFactory.h"

// Epic
#include "AssetToolsModule.h"
#include "EditorClassUtils.h"
#include "PackageTools.h"
#include "Editor/EditorEngine.h"
//#include "Engine/Level.h"
#include "Engine/World.h"
#include "ThumbnailRendering/WorldThumbnailInfo.h"
#include "UObject/Package.h"

// Quake Import
#include "BspUtilities.h"
#include "EntityMaker.h"

DEFINE_LOG_CATEGORY(LogQuakeImporter);

#define LOCTEXT_NAMESPACE "QuakeWorldFactory"

UQuakeWorldFactory::UQuakeWorldFactory(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SupportedClass = UWorld::StaticClass();
    Formats.Add(TEXT("bsp;Quake BSP models"));
    bCreateNew = false;
    bEditorImport = true;
}

bool UQuakeWorldFactory::ConfigureProperties()
{
    return true;
}

UObject* UQuakeWorldFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn)
{
    using namespace bsputils;

    // Create Packages
    FString worldPackageName = TEXT("/Game/Maps/") / Name.ToString();
    FString modelPackageName = TEXT("/Game/Models/") / Name.ToString() / Name.ToString();
    FString texturePackageName = TEXT("/Game/Textures/Textures");
    FString materialsPackageName = TEXT("/Game/Textures/Materials");

    UPackage* worldPackage = CreatePackage(nullptr, *worldPackageName);
    UPackage* modelPackage = CreatePackage(nullptr, *modelPackageName);
    UPackage* texturePackage = CreatePackage(nullptr, *texturePackageName);
    UPackage* materialPackage = CreatePackage(nullptr, *materialsPackageName);

    worldPackage->FullyLoad();
    modelPackage->FullyLoad();
    texturePackage->FullyLoad();
    materialPackage->FullyLoad();

    // Check if we already have this world package

    if (LoadObject<UWorld>(NULL, *worldPackageName, nullptr, LOAD_Quiet | LOAD_NoWarn))
    {
        UE_LOG(LogQuakeImporter, Error, TEXT("Failed to import bsp file '%s'. Reimport not supported."), *Name.ToString());
        return nullptr;
    }

    // Create Submodels
    BspLoader* loader = new BspLoader();
    loader->Load(Buffer);
    const bspformat29::Bsp_29* model = loader->GetBspPtr();

    if (!model)
    {
        UE_LOG(LogQuakeImporter, Error, TEXT("Failed to import bsp file '%s'"), *Name.ToString());
        return nullptr;
    }

    // Load Palette
    TArray<bspformat29::QColor> quakePalette;
    if (!bsputils::LoadPalette(quakePalette))
    {
        UE_LOG(LogQuakeImporter, Error, TEXT("Palette.lmp not found."));
        return nullptr;
    }

    // Create Textures and Materials
    for (const auto& it : model->textures)
    {
        UTexture2D* texture = bsputils::CreateUTexture2D(it, *texturePackage, quakePalette);
        
        if (texture)
        {
            bsputils::CreateUMaterial(it.name, *materialPackage, *texture);
        }
    }

    // Create a new world.
    UWorld* world = UWorld::CreateWorld(EWorldType::Inactive, false, Name, Cast<UPackage>(worldPackage), true, ERHIFeatureLevel::Num);
    world->SetFlags(Flags);
    world->ThumbnailInfo = NewObject<UWorldThumbnailInfo>(world, NAME_None, RF_Transactional);

    // Add Submodels
    ModelToStaticmeshes(*model, *modelPackage, *materialPackage);

    // Add submodel 0 to level

    // Add entitites
    TArray<AttributeGroup> entities;
    DeserializeGroup(model->entities, entities);
    EntityMaker(*world, entities);

    return world;
}

FText UQuakeWorldFactory::GetToolTip() const
{
    return ULevel::StaticClass()->GetToolTipText();
}

FString UQuakeWorldFactory::GetToolTipDocumentationPage() const
{
    return FEditorClassUtils::GetDocumentationPage(ULevel::StaticClass());
}

FString UQuakeWorldFactory::GetToolTipDocumentationExcerpt() const
{
    return FEditorClassUtils::GetDocumentationExcerpt(ULevel::StaticClass());
}

#undef LOCTEXT_NAMESPACE