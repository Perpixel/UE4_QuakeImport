// Fill out your copyright notice in the Description page of Project Settings.

#include "AliasFactory.h"

// Epic
#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "CoreMinimal.h"
#include "EditorClassUtils.h"
#include "PackageTools.h"
#include "RawMesh.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Math/Float16Color.h"
#include "Misc/FileHelper.h"
#include "UObject/Package.h"
#include "RHI.h"

// Quake Import
#include "QuakeCommon.h"
#include "BspUtilities.h"
#include "AliasFrameDesc.h"
#include "Alias.h"

#define LOCTEXT_NAMESPACE "AliasFactory"

UAliasFactory::UAliasFactory(const FObjectInitializer& ObjectInitializer):
    Super(ObjectInitializer)
{
    SupportedClass = UStaticMesh::StaticClass();
    Formats.Add(TEXT("mdl;Quake Alias mdl files"));
    bCreateNew = false;
    bEditorImport = true;
}

void GenerateAnimations(const FString& name, const Alias& model, UPackage* package)
{
    // Export Animation

    FString animationFilename = name + "_animation";

    int width = model.m_numVerts;
    int height = model.m_poses.Num();

    TArray<FFloat16Color> animationData; // animation data

    for (int i = 0; i < height; i++)
    {
        for (uint32 j = 0; j < model.m_numVerts; j++)
        {
            FVector3f position = model.UnpackVertex(model.m_poses[i].points[j]);
            position -= model.UnpackVertex(model.m_poses[0].points[j]);

            FFloat16Color color;

            color.R = FFloat16(-position.X); // flip x axis
            color.G = FFloat16(position.Y);
            color.B = FFloat16(position.Z);
            color.A = 255;
            animationData.Add(color);
		}
    }

    // Create Texture
    UTexture2D* animationTexture = NewObject<UTexture2D>(package, FName(*animationFilename), RF_Public | RF_Standalone);
    animationTexture->AddToRoot();

    EPixelFormat pixelformat = PF_A32B32G32R32F;

    animationTexture->PlatformData = new FTexturePlatformData();
    animationTexture->PlatformData->SizeX = width;
    animationTexture->PlatformData->SizeY = height;
    animationTexture->PlatformData->PixelFormat = pixelformat; // PF_B8G8R8A8
    //animationTexture->PlatformData->NumSlices = 1;
    animationTexture->MipGenSettings = TMGS_NoMipmaps;
    animationTexture->CompressionSettings = TextureCompressionSettings::TC_HDR;
    animationTexture->LODGroup = TextureGroup::TEXTUREGROUP_UI;
    animationTexture->Filter = TextureFilter::TF_Nearest;

    // Create first mip
    FTexture2DMipMap* texmip = new(animationTexture->PlatformData->Mips) FTexture2DMipMap();

    int32 nBlocksX = width / GPixelFormats[pixelformat].BlockSizeX;
    int32 nBlocksY = height / GPixelFormats[pixelformat].BlockSizeY;
    texmip->SizeX = width;
    texmip->SizeY = height;
    texmip->BulkData.Lock(LOCK_READ_WRITE);
    uint8* animationTextureData = (uint8*)texmip->BulkData.Realloc(nBlocksX * nBlocksY * GPixelFormats[pixelformat].BlockBytes);
    FMemory::Memcpy(animationTextureData, animationData.GetData(), nBlocksX * nBlocksY * GPixelFormats[pixelformat].BlockBytes);
    texmip->BulkData.Unlock();
    animationTexture->Source.Init(width, height, 1, 1, TSF_RGBA16F, animationTextureData);

    FAssetRegistryModule::AssetCreated(animationTexture);
    animationTexture->UpdateResource();

    package->MarkPackageDirty();
}

void GenerateAnimationNormals(const FString& name, const Alias& model, UPackage* package)
{
    // Export normals

    FString normalFilename = name + "_normal";

    int width = model.m_numVerts;
    int height = model.m_poses.Num();

    TArray<uint8> normalData;

    for (int i = 0; i < height; i++)
    {
        for (uint32 j = 0; j < model.m_numVerts; j++)
        {
            FVector3f normal = model.GetNormal(model.m_poses[i].points[j].lightnormalindex);

            normal.X *= -1;

            normalData.Add(((normal.Z + 1) * 0.5f) * 255.0f);
            normalData.Add(((normal.Y + 1) * 0.5f) * 255.0f);
            normalData.Add(((normal.X + 1) * 0.5f) * 255.0f);
            normalData.Add(255);
        }
    }

    // Create Texture
    UTexture2D* normalTexture = NewObject<UTexture2D>(package, FName(*normalFilename), RF_Public | RF_Standalone);
    normalTexture->AddToRoot();

    EPixelFormat pixelformat = PF_B8G8R8A8;

    normalTexture->PlatformData = new FTexturePlatformData();
    normalTexture->PlatformData->SizeX = width;
    normalTexture->PlatformData->SizeY = height;
    normalTexture->PlatformData->PixelFormat = pixelformat; // PF_B8G8R8A8
    //normalTexture->PlatformData->NumSlices = 1;
    normalTexture->MipGenSettings = TMGS_NoMipmaps;
    normalTexture->CompressionSettings = TextureCompressionSettings::TC_Normalmap;
    normalTexture->LODGroup = TextureGroup::TEXTUREGROUP_UI;
    normalTexture->Filter = TextureFilter::TF_Nearest;

    // Create first mip
    FTexture2DMipMap* texmip = new(normalTexture->PlatformData->Mips) FTexture2DMipMap();

    int32 nBlocksX = width / GPixelFormats[pixelformat].BlockSizeX;
    int32 nBlocksY = height / GPixelFormats[pixelformat].BlockSizeY;
    texmip->SizeX = width;
    texmip->SizeY = height;
    texmip->BulkData.Lock(LOCK_READ_WRITE);
    uint8* normalTextureData = (uint8*)texmip->BulkData.Realloc(nBlocksX * nBlocksY * GPixelFormats[pixelformat].BlockBytes);
    FMemory::Memcpy(normalTextureData, normalData.GetData(), nBlocksX * nBlocksY * GPixelFormats[pixelformat].BlockBytes);
    texmip->BulkData.Unlock();
    normalTexture->Source.Init(width, height, 1, 1, TSF_BGRA8, normalTextureData);

    FAssetRegistryModule::AssetCreated(normalTexture);
    normalTexture->UpdateResource();

    package->MarkPackageDirty();
}

UStaticMesh* BuildStaticMesh(const FName& name, const Alias& model, UPackage* package)
{
    UStaticMesh* staticmesh = NewObject<UStaticMesh>(package, name, RF_Public | RF_Standalone);
    staticmesh->AddToRoot();

    FRawMesh* rmesh = new FRawMesh();

    // Vertices
    // Grab first frame for positions
    // Animated models will use uproceduralmesh for the animations
    for (uint32 i = 0; i < model.m_numVerts; i++)
    {
        FVector3f vec = model.UnpackVertex(model.m_poses[0].points[i]);
        vec.X *= -1; // flip x axis
        rmesh->VertexPositions.Add(vec);
    }

    // Append all triangles

    for (uint32 i = 0; i < model.m_numTris; i++)
    {
        for (uint32 j = 3; j-- > 0;) // flip face
        {
            // Unpack UV
            const AliasTexcoord* st = &model.m_texcoords[model.m_triangles[i].indices[j]];
            FVector2f texcoord((float)st->s / model.m_skinWidth, (float)st->t / model.m_skinHeight);
            if (st->onseam > 0 && !model.m_triangles[i].front)
            {
                texcoord.X += 0.5f; // offset by 0.5 for uv on the back side of our model
            }

            // Set vertex
            int index = model.m_triangles[i].indices[j];
            FVector3f normal = model.GetNormal(model.m_poses[0].points[index].lightnormalindex);
            normal.X *= -1;

            rmesh->WedgeIndices.Add(index);
            rmesh->WedgeColors.Add(FColor(0));
            rmesh->WedgeTangentZ.Add(normal); // normal
            rmesh->WedgeTexCoords[0].Add(texcoord);
            rmesh->WedgeTexCoords[1].Add(FVector2f(((float)index + 0.5f) / model.m_numVerts, 0.5f)); // this channel is for the vertex animation
        }
        rmesh->FaceMaterialIndices.Add(0);
        rmesh->FaceSmoothingMasks.Add(0); // TODO dont know how that work yet
    }

    // build staticmesh

    FStaticMeshSourceModel* srcModel = &staticmesh->AddSourceModel();

    srcModel->BuildSettings.bRecomputeNormals = false;
    srcModel->BuildSettings.bRemoveDegenerates = false;
    srcModel->BuildSettings.bUseFullPrecisionUVs = true;
    srcModel->BuildSettings.DistanceFieldResolutionScale = 0.0f;
    srcModel->BuildSettings.bGenerateLightmapUVs = false;
    srcModel->RawMeshBulkData->SaveRawMesh(*rmesh);

    staticmesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
    staticmesh->CreateBodySetup();
    staticmesh->Build();

    staticmesh->PostEditChange(); // need this?

    package->MarkPackageDirty();

    delete rmesh;

    return staticmesh;
}

UObject* UAliasFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn)
{
    Alias* alias = new Alias(Name.ToString(), Buffer);

    if (alias)
    {
        // Create Package
        FString packageName = TEXT("/Game/Alias/") / Name.ToString();
        UPackage* package = CreatePackage(nullptr, *packageName);
        package->FullyLoad();

        UStaticMesh* staticMesh = BuildStaticMesh(Name, *alias, package);

        if (staticMesh)
        {
            // Load Palette
            TArray<QuakeCommon::QColor> quakePalette;

            if (!QuakeCommon::LoadPalette(quakePalette))
            {
                // ERROR
            }

            for (uint32 i = 0; i < alias->m_numSkins; i++)
            {
                FString skinName = Name.ToString() + "_skin_" + FString::FromInt(i);
                FString materialName = Name.ToString() + "_material_" + FString::FromInt(i);
                UTexture2D* texture = QuakeCommon::CreateUTexture2D(skinName, alias->m_skinWidth, alias->m_skinHeight, alias->m_skins[i].data, *package, quakePalette);
                QuakeCommon::CreateUMaterial(materialName, *package, *texture);
            }

            // if we have a skin lets assign it to the material index 0
            FString skin0 = Name.ToString() + "_material_0";
            if (UMaterialInterface* material = (UMaterialInterface*)QuakeCommon::CheckIfAssetExist<UMaterialInterface>(skin0, *package))
            {
                staticMesh->GetStaticMaterials().AddUnique(FStaticMaterial(material, FName(*skin0), FName(*skin0)));
            }
            
            // 
            FString descFileName = Name.ToString() + "_desc";
            UDataTable* table = NewObject<UDataTable>(package, FName(*descFileName), RF_Public | RF_Standalone);
            table->AddToRoot();
            table->RowStruct = FAliasFrameDesc::StaticStruct();

            FString datacsv;

            datacsv.Append(" ,Name, Type, Start, NumPoses, Interval\n");

            for (int i = 0; i < alias->m_frames.Num(); i++)
            {
                datacsv.Append(FString::FromInt(i) + ",");
                datacsv.Append(alias->m_frames[i].name + ",");
                datacsv.Append(FString::FromInt(alias->m_frames[i].type) + ",");
                datacsv.Append(FString::FromInt(alias->m_frames[i].firstpose) + ",");
                datacsv.Append(FString::FromInt(alias->m_frames[i].numposes) + ",");
                datacsv.Append(FString::SanitizeFloat(alias->m_frames[i].interval));
                datacsv.Append("\n");
            }

            table->CreateTableFromCSVString(datacsv);

            // Animate
            GenerateAnimations(Name.ToString(), *alias, package);

            // Normal
            GenerateAnimationNormals(Name.ToString(), *alias, package);

            // Save
            QuakeCommon::SavePackage(*package);

            // Extract textures
            return staticMesh;
        }
    }

    return nullptr;
}
#undef LOCTEXT_NAMESPACE
