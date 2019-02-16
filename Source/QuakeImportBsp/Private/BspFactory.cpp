// Fill out your copyright notice in the Description page of Project Settings.

#include "BspFactory.h"
#include "QuakeCommon.h"

// Epic
#include "AssetToolsModule.h"
#include "EditorClassUtils.h"
#include "PackageTools.h"
#include "Editor/EditorEngine.h"
#include "Engine/World.h"
#include "ThumbnailRendering/WorldThumbnailInfo.h"
#include "UObject/Package.h"

#include "Editor/EditorEngine.h"
#include "Public/Editor.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/StaticMeshActor.h"

// Quake Import
#include "BspUtilities.h"
#include "EntityMaker.h"

DEFINE_LOG_CATEGORY(LogQuakeImporter);

#define LOCTEXT_NAMESPACE "BspFactory"

UBspFactory::UBspFactory(const FObjectInitializer& ObjectInitializer):
    Super(ObjectInitializer)
{
    SupportedClass = UPackage::StaticClass();
    Formats.Add(TEXT("bsp;Quake BSP model files"));
    bCreateNew = false;
    bEditorImport = true;
}

bool FindPlayerStart(const TArray<AttributeGroup>& entities)
{
    for (const auto& it : entities)
    {
        if (it.Get("classname") != nullptr)
        {
            if (it.Get("classname")->ToString() == "info_player_start")
            {
                return true;
            }
        }
    }

    return false;
}

UObject* UBspFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn)
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

    //worldPackage->FullyLoad();
    //modelPackage->FullyLoad();
    //texturePackage->FullyLoad();
    //materialPackage->FullyLoad();

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
    TArray<QuakeCommon::QColor> quakePalette;
    if (!QuakeCommon::LoadPalette(quakePalette))
    {
        UE_LOG(LogQuakeImporter, Error, TEXT("Palette.lmp not found."));
        return nullptr;
    }

    // Create Textures and Materials
    for (const auto& it : model->textures)
    {
        if (it.name.StartsWith("sky"))
        {
            // sky texture split the data buffer at the center and create 2 textures _front and _back
            TArray<uint8> front;
            TArray<uint8> back;

            for (unsigned i = 0; i < it.height; i++)
            {
                for (unsigned j = 0; j < it.width; j++)
                {
                    unsigned pos = (i * it.width) + j;

                    if (j < it.width / 2)
                    {
                        front.Add(it.mip0[pos]);
                    }
                    else
                    {
                        back.Add(it.mip0[pos]);
                    }
                }
            }

            QuakeCommon::CreateUTexture2D(it.name + "_front", it.width / 2, it.height, front, *texturePackage, quakePalette);
            UTexture2D* skyTexture = QuakeCommon::CreateUTexture2D(it.name + "_back", it.width / 2, it.height, back, *texturePackage, quakePalette);

            QuakeCommon::CreateUMaterial(it.name, *materialPackage, *skyTexture);
        }
        else if (it.name.StartsWith("+0"))
        {
            // First flipbook frame. Append the rest.
            TArray<uint8> data;

            // append first frame data
            data.Append(it.mip0);

            int numFrames = 1;

            while (AppendNextTextureData(it.name, numFrames, *model, data))
            {
                numFrames++;
            }

            UTexture2D* flipbookTexture = QuakeCommon::CreateUTexture2D(it.name, it.width, it.height * numFrames, data, *texturePackage, quakePalette);
            QuakeCommon::CreateUMaterial(it.name, *materialPackage, *flipbookTexture);
        }
        else
        {
            UTexture2D* texture = QuakeCommon::CreateUTexture2D(it.name, it.width, it.height, it.mip0, *texturePackage, quakePalette);

            if (texture)
            {
                QuakeCommon::CreateUMaterial(it.name, *materialPackage, *texture);
            }
        }
    }

    // Deserialize entities
    TArray<AttributeGroup> entities;
    DeserializeGroup(model->entities, entities);

    // Add Submodels
    ModelToStaticmeshes(*model, *modelPackage, *materialPackage);

    // Look for info_player_start. Map found without this are just normal pickup items made out of BSP.

    if (FindPlayerStart(entities))
    {
        // Create a new world.
        UWorld* world = UWorld::CreateWorld(EWorldType::Inactive, false, Name, Cast<UPackage>(worldPackage), true, ERHIFeatureLevel::Num);
        world->SetFlags(Flags);
        world->ThumbnailInfo = NewObject<UWorldThumbnailInfo>(world, NAME_None, RF_Transactional);

        // Add submodels 0 to level
        FString model0Name = modelPackageName + ".submodel_0";
        UStaticMesh* submodel = LoadObject<UStaticMesh>(NULL, *model0Name);

        if (submodel != nullptr)
        {
            AStaticMeshActor* staticMesh = Cast<AStaticMeshActor>(GEditor->AddActor(world->GetCurrentLevel(), AStaticMeshActor::StaticClass(), FTransform()));

            staticMesh->GetStaticMeshComponent()->Mobility = EComponentMobility::Static;
            staticMesh->GetStaticMeshComponent()->SetStaticMesh(submodel);
        }

        GEditor->EditorUpdateComponents();
        world->UpdateWorldComponents(true, false);


        // Add entitites
        EntityMaker(*world, entities);
    }

    QuakeCommon::SavePackage(*worldPackage);
    QuakeCommon::SavePackage(*modelPackage);
    QuakeCommon::SavePackage(*texturePackage);
    QuakeCommon::SavePackage(*materialPackage);

    return worldPackage;
}

#undef LOCTEXT_NAMESPACE
