#pragma once

#include "CoreMinimal.h"

class UTexture2D;
class UPackage;

namespace QuakeCommon
{
    // Represent a single RGB 8bit sample
    struct QColor
    {
        uint8 r;
        uint8 g;
        uint8 b;
    };

    // Load Quake color palette from file in our plugin content
    bool LoadPalette(TArray<QColor>& outPalette);

    // Create a UTexture2D in the given package then save
    UTexture2D* CreateUTexture2D(const FString& name, int width, int height, const TArray<uint8>& data, UPackage& texturePackage, const TArray<QColor>& pal, bool savePackage = true);

    // Create matching material for texture
    void CreateUMaterial(const FString& textureName, UPackage& materialPackage, UTexture2D& initialTexture);

    // Utilities

    template<class T>
    int ReadData(const uint8*& data, const int position, T& out)
    {
        out = *reinterpret_cast<const T*>(data + position);
        return sizeof(T);
    }

    template<typename T>
    UObject* CheckIfAssetExist(FString name, const UPackage& package)
    {
        FString fullname = package.GetName() + TEXT(".") + name;
        return LoadObject<T>(NULL, *fullname, nullptr, LOAD_Quiet | LOAD_NoWarn);
    }

    void SaveAsset(UObject& object, UPackage& package);

    void SavePackage(UPackage& package);

} // namespace QuakeCommon