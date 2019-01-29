// Copyright 2018 Guillaume Plourde. All Rights Reserved.
// https://github.com/Perpixel/

#pragma once

#include "CoreMinimal.h"

class UTexture2D;
class UPackage;

namespace bsputils
{
    enum class ELeafContentType
    {
        Empty = -1,
        Solid = -2,
        Water = -3,
        Slime = -4,
        Lava = -5,
        Sky = -6,
        Origin = -7,
        Clip = -8
    };

    namespace bspformat29
    {
        // bsp data structure
        // only used to deserialize quake bsp files

        constexpr int HEADER_VERSION_29 = 29;   // quake 1 version
        constexpr int HEADER_LUMP_SIZE = 15;   // how many lumps in the header

        constexpr int LUMP_ENTITIES = 0;
        constexpr int LUMP_PLANES = 1;
        constexpr int LUMP_TEXTURES = 2;
        constexpr int LUMP_VERTEXES = 3;
        constexpr int LUMP_VISIBILITY = 4;
        constexpr int LUMP_NODES = 5;
        constexpr int LUMP_TEXINFO = 6;
        constexpr int LUMP_FACES = 7;
        constexpr int LUMP_LIGHTING = 8;
        constexpr int LUMP_CLIPNODES = 9;
        constexpr int LUMP_LEAFS = 10;
        constexpr int LUMP_MARKSURFACES = 11;
        constexpr int LUMP_EDGES = 12;
        constexpr int LUMP_SURFEDGES = 13;
        constexpr int LUMP_MODELS = 14;

        constexpr int MAXLIGHTMAPS = 4;
        constexpr int MAXLEAVES = 8192;

        struct QColor
        {
            uint8 r;
            uint8 g;
            uint8 b;
        };

        struct Point2f
        {
            float x;
            float y;
        };

        struct Point3f
        {
            float x;
            float y;
            float z;
        };

        struct Lump
        {
            int position;
            int length;
        };

        struct Header
        {
            int     version;
            Lump    lumps[HEADER_LUMP_SIZE];
        };

        struct Edge
        {
            short first;    // first vertex
            short second;   // second vertex
        };

        struct Surfedge
        {
            int index;
        };

        struct Marksurface
        {
            short index;
        };

        struct Plane
        {
            float	normal[3];
            float	dist;
            char	type;
        };

        struct Face
        {
            short   planenum;
            short   side;

            int     firstedge;
            short   numedges;
            short   texinfo;

            char    styles[MAXLIGHTMAPS];
            int     lightofs;
        };

        struct Leaf
        {
            ELeafContentType    contents;
            int                 visofs;

            short               mins[3];
            short               maxs[3];

            unsigned short      firstmarksurface;
            unsigned short      nummarksurfaces;

            char                ambient_level[4];
        };

        struct Node
        {
            int			    planenum;
            short           children[2];
            short           mins[3];
            short           maxs[3];
            unsigned short	firstface;
            unsigned short	numfaces;
        };

        struct SubModel
        {
            float   mins[3];
            float   maxs[3];
            float   origin[3];
            int     headnode[4];
            int     visleafs;
            int     firstface;
            int     numfaces;
        };

        struct TexInfo
        {
            float   vecs[2][4];
            int     miptex;
            int     flags;
        };

        struct Miptex
        {
            char        name[16];
            unsigned    width;
            unsigned    height;
            unsigned    offsets[4]; // four mip maps stored
        };

        struct Texture
        {
            FString         name;
            unsigned        width;
            unsigned        height;
            TArray<uint8>   mip0;
        };

        // Data storage for BSP version 29
        struct Bsp_29
        {
            TArray<Point3f>     vertices;
            TArray<Edge>        edges;
            TArray<Surfedge>    surfedges;
            TArray<Plane>       planes;
            TArray<Face>        faces;
            TArray<Marksurface> marksurfaces;
            TArray<Leaf>        leaves;
            TArray<Node>        nodes;
            TArray<SubModel>    submodels;
            TArray<TexInfo>     texinfos;
            TArray<Texture>     textures;
            FString             entities;
            TArray<uint8>       lightdata;
            TArray<uint8>       visdata;

            int GetNumVertices() const { return size_to_int(vertices.Num()); }
            int GetNumLeaves() const { return size_to_int(leaves.Num()); }
            int GetNumNodes() const { return size_to_int(nodes.Num()); }

        private:

            int size_to_int(const size_t& size) const { return static_cast<int>(size); }
        };
    }

    class BspLoader
    {
    public:

        BspLoader();
        ~BspLoader();

        void Load(const uint8*& data);
        const bspformat29::Bsp_29* GetBspPtr() const { return m_bsp29; }

    private:

        bspformat29::Bsp_29* m_bsp29;

        template<class T>
        int ReadData(const uint8*& data, const int position, T& out)
        {
            out = *reinterpret_cast<const T*>(data + position);
            return sizeof(T);
        }

        template<typename T>
        bool DeserializeLump(const uint8*& data, const bspformat29::Lump& lump, TArray<T>& out);

        void LoadTextures(const uint8*& data, const bspformat29::Lump& lump);
        void LoadEntities(const uint8*& data, const bspformat29::Lump& lump);
    };

    template<typename T>
    bool BspLoader::DeserializeLump(const uint8*& data, const bspformat29::Lump& lump, TArray<T>& out)
    {
        if (lump.length % sizeof(T))
        {
            UE_LOG(LogTemp, Log, TEXT("BSP Import error: Lump size mismatch!"));
            return false;
        }

        size_t count = lump.length / sizeof(T);
        out.Empty();
        T* in = (T*)(data + lump.position);
        out.Append(in, count);
        return true;
    }

    // UNREALED Import functions
    
    // Load Quake color palette. 256 colors RGB
    bool LoadPalette(TArray<bspformat29::QColor>& outPalette);
    // Create Utexture2D from raw Quake texture mip
    UTexture2D* CreateUTexture2D(const bspformat29::Texture& texture, UPackage& texturePackage, const TArray<bspformat29::QColor>& pal);
    // Create matching material for texture
    void CreateUMaterial(const FString& textureName, UPackage& materialPackage, UTexture2D& initialTexture);
    // From a Quake BSP model, import all submodels to individual staticmeshes
    void ModelToStaticmeshes(const bspformat29::Bsp_29& model, UPackage& package, const UPackage& materialPackage);

} // namespace bsputils