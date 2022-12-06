#include "QuakeCommon.h"

#include "AssetRegistryModule.h"
#include "Interfaces/IPluginManager.h"
#include "Engine/Classes/Materials/MaterialExpressionConstant.h"
#include "Engine/Texture2D.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/TextureFactory.h"
#include "Materials/Material.h"
#include "Misc/FileHelper.h"
#include "UObject/Package.h"

namespace QuakeCommon
{
    bool LoadPalette(TArray<QColor>& outPalette)
    {
        FString palFilename = IPluginManager::Get().FindPlugin(TEXT("QuakeImport"))->GetContentDir() / FString("palette.lmp");

        TArray<uint8> data;
        if (FFileHelper::LoadFileToArray(data, *palFilename))
        {
            int32 count = data.Num() / sizeof(QColor);
            outPalette.Empty();
            QColor* in = (QColor*)data.GetData();
            outPalette.Append(in, count);

            return true;
        }

        return false;
    }

    UTexture2D* CreateUTexture2D(const FString& name, int width, int height, const TArray<uint8>& data, UPackage& texturePackage, const TArray<QColor>& pal, bool savePackage)
    {
        FString finalName = name + "_color";

        if (CheckIfAssetExist<UTexture2D>(finalName, texturePackage))
        {
            return nullptr;
        }

        // get colors from palette
        TArray<uint8> finalData;

        for (const auto& it : data)
        {
            finalData.Add(pal[it].b);
            finalData.Add(pal[it].g);
            finalData.Add(pal[it].r);
            finalData.Add(255);
        }

        // Create Texture
        UTexture2D* texture = NewObject<UTexture2D>(&texturePackage, FName(*finalName), RF_Public | RF_Standalone);

        texture->AddToRoot();
        texture->PlatformData = new FTexturePlatformData();
        texture->PlatformData->SizeX = width;
        texture->PlatformData->SizeY = height;
        texture->PlatformData->PixelFormat = PF_B8G8R8A8;

        // Create first mip
        FTexture2DMipMap* texmip = new(texture->PlatformData->Mips) FTexture2DMipMap();
        texmip->SizeX = width;
        texmip->SizeY = height;
        texmip->BulkData.Lock(LOCK_READ_WRITE);
        uint32 textureDataSize = (width * height) * sizeof(uint8) * 4;
        uint8* textureData = (uint8*)texmip->BulkData.Realloc(textureDataSize);
        FMemory::Memcpy(textureData, finalData.GetData(), textureDataSize);
        texmip->BulkData.Unlock();

        texture->MipGenSettings = TMGS_NoMipmaps;
        texture->Source.Init(width, height, 1, 1, TSF_BGRA8, textureData);

        FAssetRegistryModule::AssetCreated(texture);

        texture->UpdateResource();
        texturePackage.MarkPackageDirty();

        return texture;
    }

    void CreateUMaterial(const FString& materialName, UPackage& materialPackage, UTexture2D& initialTexture)
    {
        if (QuakeCommon::CheckIfAssetExist<UMaterial>(materialName, materialPackage))
        {
            return;
        }

        UMaterialFactoryNew* materialFactory = NewObject<UMaterialFactoryNew>();
        materialFactory->AddToRoot();

        materialFactory->InitialTexture = &initialTexture;

        UMaterial* material = (UMaterial*)materialFactory->FactoryCreateNew(UMaterial::StaticClass(), &materialPackage, *materialName, RF_Standalone | RF_Public, NULL, GWarn);

        UMaterialExpressionConstant* specValue = NewObject<UMaterialExpressionConstant>(material);
        material->AddToRoot();
        material->GetEditorOnlyData()->Specular.Connect(0, specValue);

        FAssetRegistryModule::AssetCreated(material);

        material->PreEditChange(NULL);
        material->MarkPackageDirty();
        materialPackage.SetDirtyFlag(true);
        material->PostEditChange();
    }

    void SaveAsset(UObject& object, UPackage& package)
    {
        UPackage::SavePackage(
            &package,
            &object,
            RF_Public | RF_Standalone,
            *FPackageName::LongPackageNameToFilename(package.GetName(), FPackageName::GetAssetPackageExtension()),
            GError,
            nullptr,
            false,
            true,
            SAVE_NoError
        );
    }

    void SavePackage(UPackage& package)
    {
        UPackage::SavePackage(
            &package,
            nullptr,
            RF_Public | RF_Standalone,
            *FPackageName::LongPackageNameToFilename(package.GetName(), FPackageName::GetAssetPackageExtension()),
            GError,
            nullptr,
            false,
            true,
            SAVE_NoError
        );
    }

} // namespace QuakeCommon
