// Fill out your copyright notice in the Description page of Project Settings.

// QuakeImport
#include "BspUtilities.h"
#include "QuakeCommon.h"

// EPIC
#include "AssetRegistryModule.h"
#include "Containers/UnrealString.h"
#include "Editor/EditorEngine.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Factories/MaterialFactoryNew.h"
#include "Materials/Material.h"
#include "RawMesh/Public/RawMesh.h"
#include "UObject/Package.h"

namespace bsputils
{
    BspLoader::BspLoader() :
        m_bsp29(nullptr)
    {
        /* do nothing */
    }

    BspLoader::~BspLoader()
    {
        delete m_bsp29;
    }

    void BspLoader::Load(const uint8*& data)
    {
        bspformat29::Header header;

        QuakeCommon::ReadData<bspformat29::Header>(data, 0, header);

        if (header.version != bspformat29::HEADER_VERSION_29)
        {
            return;
        }

        m_bsp29 = new bspformat29::Bsp_29();

        DeserializeLump<bspformat29::Point3f>(data, header.lumps[bspformat29::LUMP_VERTEXES], m_bsp29->vertices);
        DeserializeLump<bspformat29::Edge>(data, header.lumps[bspformat29::LUMP_EDGES], m_bsp29->edges);
        DeserializeLump<bspformat29::Surfedge>(data, header.lumps[bspformat29::LUMP_SURFEDGES], m_bsp29->surfedges);
        DeserializeLump<bspformat29::Face>(data, header.lumps[bspformat29::LUMP_FACES], m_bsp29->faces);
        DeserializeLump<uint8>(data, header.lumps[bspformat29::LUMP_LIGHTING], m_bsp29->lightdata);
        DeserializeLump<bspformat29::Plane>(data, header.lumps[bspformat29::LUMP_PLANES], m_bsp29->planes);
        DeserializeLump<bspformat29::Marksurface>(data, header.lumps[bspformat29::LUMP_MARKSURFACES], m_bsp29->marksurfaces);
        DeserializeLump<bspformat29::Leaf>(data, header.lumps[bspformat29::LUMP_LEAFS], m_bsp29->leaves);
        DeserializeLump<bspformat29::Node>(data, header.lumps[bspformat29::LUMP_NODES], m_bsp29->nodes);
        DeserializeLump<bspformat29::SubModel>(data, header.lumps[bspformat29::LUMP_MODELS], m_bsp29->submodels);
        DeserializeLump<bspformat29::TexInfo>(data, header.lumps[bspformat29::LUMP_TEXINFO], m_bsp29->texinfos);
        DeserializeLump<uint8>(data, header.lumps[bspformat29::LUMP_VISIBILITY], m_bsp29->visdata);

        LoadTextures(data, header.lumps[bspformat29::LUMP_TEXTURES]);
        LoadEntities(data, header.lumps[bspformat29::LUMP_ENTITIES]);
    }

    void BspLoader::LoadTextures(const uint8*& data, const bspformat29::Lump& lump)
    {
        int numtex = 0;
        uint32 position = lump.position; // position in the data

        // first int is the number of textures
        position += QuakeCommon::ReadData(data, position, numtex);

        for (int i = 0; i < numtex; i++)
        {
            // this texture details followed by the 4 texture mips
            const bspformat29::Miptex* mt;
            int offset = 0;
            position += QuakeCommon::ReadData(data, position, offset);
            mt = reinterpret_cast<const bspformat29::Miptex*>(data + lump.position + offset);

            // Just getting mip0 (texture lump offset + miptex offset + mip0 offset)
            const uint8* in = reinterpret_cast<const uint8*>(data + lump.position + offset + mt->offsets[0]);
            bspformat29::Texture tex;
            tex.name = mt->name;
            tex.width = mt->width;
            tex.height = mt->height;
            tex.mip0.Append(in, (mt->width * mt->height));
            m_bsp29->textures.Add(tex);
        }
    }

    void BspLoader::LoadEntities(const uint8*& data, const bspformat29::Lump& lump)
    {
        TArray<char> char_array;
        const char* in = reinterpret_cast<const char*>(data + lump.position);
        m_bsp29->entities = ANSI_TO_TCHAR(in);
    }

    void AddWedgeEntry(FRawMesh& mesh, const uint32 index, const FVector3f normal, const FVector2f texcoord0, const FVector2f texcoord1)
    {
        mesh.WedgeIndices.Add(index);
        mesh.WedgeColors.Add(FColor(0));
        mesh.WedgeTangentZ.Add(normal);
        mesh.WedgeTexCoords[0].Add(texcoord0);
    }

    void CreateSubmodel(UPackage& package, const uint8 id, const bspformat29::Bsp_29& model, const UPackage& materialPackage)
    {
        using namespace bsputils;

        struct Triface
        {
            int numtris = 0;
            FVector normal;
            TArray<uint32> points;
            int texinfo;
            TArray<FVector2f> texcoords;
        };

        FString submodelName("submodel");
        submodelName += "_";
        submodelName += FString::FromInt(id);

        UStaticMesh* staticmesh = NewObject<UStaticMesh>(&package, FName(*submodelName), RF_Public | RF_Standalone);
        staticmesh->AddToRoot();

        TArray<Triface> faces;

        for (
            int f = model.submodels[id].firstface;
            f < (model.submodels[id].numfaces + model.submodels[id].firstface);
            f++
            )
        {
            const bspformat29::Face& face = model.faces[f];

            const bspformat29::TexInfo& ti = model.texinfos[face.texinfo];
            const bspformat29::Texture& tex = model.textures[ti.miptex];

            if (tex.name.StartsWith("sky"))
            {
                // Skip sky surfaces. We wont need them.
                continue;
            }

            Triface triface;
            triface.texinfo = face.texinfo;
            triface.numtris = face.numedges - 2; // make up number of this needed for this face 

            for (int i = 0; i < 3; i++)
            {
                triface.normal[i] = model.planes[face.planenum].normal[i];
            }

            for (int e = face.numedges; e-- > 0;) // extract all vertex
            { 
                const bspformat29::Surfedge& surfedge = model.surfedges[face.firstedge + e];
                const bspformat29::Edge& edge = model.edges[abs(surfedge.index)];

                short vertex_id = edge.first;

                if (surfedge.index < 0)
                {
                    vertex_id = edge.second;
                }

                triface.points.Add(vertex_id);

                FVector2f tex_coord;

                // Generate texture coordinates

                FVector3f point = FVector3f(
                    model.vertices[vertex_id].x,
                    model.vertices[vertex_id].y,
                    model.vertices[vertex_id].z
                );

                tex_coord.X = FVector3f::DotProduct(point, FVector3f(ti.vecs[0][0], ti.vecs[0][1], ti.vecs[0][2])) + ti.vecs[0][3];
                tex_coord.Y = FVector3f::DotProduct(point, FVector3f(ti.vecs[1][0], ti.vecs[1][1], ti.vecs[1][2])) + ti.vecs[1][3];

                tex_coord.X /= tex.width;
                tex_coord.Y /= tex.height;

                triface.texcoords.Add(tex_coord);
            }

            faces.Add(triface);
        }

        FRawMesh* rmesh = new FRawMesh();

        // Vertices
        for (int i = 0; i < model.vertices.Num(); i++)
        {
            FVector3f vec(
                -model.vertices[i].x, // flip X axis
                model.vertices[i].y,
                model.vertices[i].z);

            rmesh->VertexPositions.Add(vec);
        }

        // tris

        for (int i = 0; i < faces.Num(); i++)
        {
            for (int j = 0; j < faces[i].numtris; j++)
            {
                AddWedgeEntry(
                    *rmesh,
                    faces[i].points[0],
                    FVector3f(faces[i].normal.X, faces[i].normal.Y, faces[i].normal.Z),
                    faces[i].texcoords[0],
                    faces[i].texcoords[0]
                );

                for (int c = 1; c < 3; c++)
                {
                    int index = j + c;

                    AddWedgeEntry(
                        *rmesh,
                        faces[i].points[index],
                        FVector3f(faces[i].normal.X, faces[i].normal.Y, faces[i].normal.Z),
                        faces[i].texcoords[index],
                        faces[i].texcoords[index]
                    );
                }

                int32 materialId = model.texinfos[faces[i].texinfo].miptex;

                UMaterialInterface* material = (UMaterialInterface*)QuakeCommon::CheckIfAssetExist<UMaterialInterface>(model.textures[materialId].name, materialPackage);

                if (!material)
                {
                    material = UMaterial::GetDefaultMaterial(MD_Surface);
                }

                int32 MaterialIndex = staticmesh->GetStaticMaterials().AddUnique(
                    FStaticMaterial(
                        material,
                        FName(*model.textures[materialId].name),
                        FName(*model.textures[materialId].name)
                    )
                );

                rmesh->FaceMaterialIndices.Add(MaterialIndex);
                rmesh->FaceSmoothingMasks.Add(0); // TODO dont know how that work yet
            }
        }

        FStaticMeshSourceModel* srcModel = &staticmesh->AddSourceModel();

        int lightmapSize = 32;

        if (id == 0)
        {
            lightmapSize = 512; // main model mesh
        }

        srcModel->BuildSettings.MinLightmapResolution = lightmapSize;
        srcModel->BuildSettings.SrcLightmapIndex = 0;
        srcModel->BuildSettings.DstLightmapIndex = 1;
        srcModel->BuildSettings.bGenerateLightmapUVs = true;
        srcModel->BuildSettings.bUseFullPrecisionUVs = true;
        srcModel->RawMeshBulkData->SaveRawMesh(*rmesh);

        staticmesh->SetLightingGuid();
        staticmesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
        //staticmesh->CreateBodySetup();
        staticmesh->SetLightingGuid();
        staticmesh->EnforceLightmapRestrictions(); // Make sure the Lightmap UV point on a valid UVChannel
        staticmesh->Build();
        staticmesh->SetLightingGuid();
        staticmesh->LightMapResolution = lightmapSize;
        staticmesh->LightMapCoordinateIndex = 1;
        staticmesh->PostEditChange();

        package.MarkPackageDirty();

        delete rmesh;
    }

    void ModelToStaticmeshes(const bspformat29::Bsp_29& model, UPackage& package, const UPackage& materialPackage)
    {
        for (int i = 0; i < model.submodels.Num(); i++)
        {
            CreateSubmodel(package, i, model, materialPackage);
        }
    }

    bool AppendNextTextureData(const FString& name, const int frame, const bspformat29::Bsp_29& model, TArray<uint8>& data)
    {
        FString nextName = name;
        nextName[1] += frame;

         for (const auto& it : model.textures)
        {
            if (it.name == nextName)
            {
                data.Append(it.mip0);
                return true;
            }
        }

        return false;
    }
} // namespace bsputils
