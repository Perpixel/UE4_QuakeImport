// Fill out your copyright notice in the Description page of Project Settings.

// QuakeImport
#include "Alias.h"
#include "QuakeCommon.h"

// EPIC
#include "AssetRegistryModule.h"
#include "Interfaces/IPluginManager.h"
#include "Containers/UnrealString.h"
#include "Editor/EditorEngine.h"
#include "Engine/Classes/Materials/MaterialExpressionConstant.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/TextureFactory.h"
#include "Materials/Material.h"
#include "Misc/FileHelper.h"
#include "RawMesh/Public/RawMesh.h"
#include "UObject/Package.h"

Alias::Alias(const FString name, const uint8*& buf) :
    m_name(name),
    m_scale(),
    m_origin(),
    m_eyePosition(),
    m_numSkins(0),
    m_numFrames(0),
    m_numTris(0)
{
    // Read model header
    AliasHeader aliasHeader;
    QuakeCommon::ReadData<AliasHeader>(buf, 0, aliasHeader);

    m_origin = { aliasHeader.origin[0], aliasHeader.origin[1], aliasHeader.origin[2] };
    m_scale = { aliasHeader.scale[0], aliasHeader.scale[1], aliasHeader.scale[2] };
    m_eyePosition = { aliasHeader.offsets[0], aliasHeader.offsets[1], aliasHeader.offsets[2] };

    m_skinWidth = aliasHeader.skinwidth;
    m_skinHeight = aliasHeader.skinheight;
    m_numSkins = aliasHeader.numskins;
    m_numTris = aliasHeader.numtris;
    m_numFrames = aliasHeader.numframes;
    m_numVerts = aliasHeader.numverts;

    int skinSize = aliasHeader.skinwidth * aliasHeader.skinheight; // pre calculate total byte size of 1 skin

    // pre allocate storage for our mesh data
    m_texcoords.SetNum(aliasHeader.numverts);
    m_triangles.SetNum(aliasHeader.numtris);

    // Predefine position of all our data types in the mdl file
    // No table exist for this so it must be calculated this way
    int SKIN_POS = sizeof(AliasHeader);
    int TEXTURE_VERTS_POS = SKIN_POS + ((sizeof(int) + skinSize) * aliasHeader.numskins);
    int TRIANGLES_POS = TEXTURE_VERTS_POS + (sizeof(AliasTexcoord) * aliasHeader.numverts);
    int FRAMES_POS = TRIANGLES_POS + (sizeof(AliasTriangle) * aliasHeader.numtris);

    // SKIN

    m_skins.SetNum(aliasHeader.numskins);

    for (int i = 0; i < aliasHeader.numskins; i++)
    {
        int skin_type;
        SKIN_POS += QuakeCommon::ReadData<int>(buf, SKIN_POS, skin_type);

        if (skin_type == 0)
        {
            // STATIC SKIN
            m_skins[i].data.Append(buf + SKIN_POS, skinSize);
            SKIN_POS += skinSize;
        }
    }

    // TEXTURE COORDINATES
    for (int i = 0; i < aliasHeader.numverts; i++)
    {
        TEXTURE_VERTS_POS += QuakeCommon::ReadData<AliasTexcoord>(buf, TEXTURE_VERTS_POS, m_texcoords[i]);
    }

    // TRIANGLES
    for (int i = 0; i < aliasHeader.numtris; i++)
    {
        TRIANGLES_POS += QuakeCommon::ReadData<AliasTriangle>(buf, TRIANGLES_POS, m_triangles[i]);
    }

    // FRAMES

    for (int i = 0; i < aliasHeader.numframes; i++)
    {
        int frametype;
        FRAMES_POS += QuakeCommon::ReadData<int>(buf, FRAMES_POS, frametype);

        if (frametype == 0) // ALIAS_SINGLE_FRAME
        {
            AliasFrame frame;

            frame.type = frametype;
            frame.numposes = 1;
            frame.firstpose = 0;
            frame.interval = 0;

            FRAMES_POS += QuakeCommon::ReadData<AliasPoint>(buf, FRAMES_POS, frame.bboxmin);
            FRAMES_POS += QuakeCommon::ReadData<AliasPoint>(buf, FRAMES_POS, frame.bboxmax);

            AliasFrameName framename;
            FRAMES_POS += QuakeCommon::ReadData<AliasFrameName>(buf, FRAMES_POS, framename);
            frame.name = framename.str;

            AliasPose pose(aliasHeader.numverts);

            for (int j = 0; j < aliasHeader.numverts; j++)
            {
                FRAMES_POS += QuakeCommon::ReadData<AliasPoint>(
                    buf,
                    FRAMES_POS,
                    pose.points[j]
                    );
            }

            frame.firstpose = (int)m_poses.Num();
            m_poses.Add(pose);
            m_frames.Add(frame);
        }
        else // ALIAS_FRAME_GROUP
        {
            AliasFrame frame;

            frame.type = frametype;
            AliasGroup group;
            FRAMES_POS += QuakeCommon::ReadData<AliasGroup>(buf, FRAMES_POS, group);

            frame.firstpose = (int)m_poses.Num();

            frame.numposes = group.numframes - frame.firstpose;
            frame.bboxmin = group.bboxmin;
            frame.bboxmax = group.bboxmax;

            TArray<float> intervals;
            intervals.Init(0, group.numframes);

            for (int j = 0; j < group.numframes; j++)
            {
                FRAMES_POS += QuakeCommon::ReadData<float>(buf, FRAMES_POS, intervals[j]);
            }

            frame.interval = intervals[0]; // only use first interval for the frame.

            for (int j = 0; j < group.numframes; j++)
            {
                AliasPoint min;
                AliasPoint max;

                AliasPose pose(aliasHeader.numverts);

                FRAMES_POS += QuakeCommon::ReadData<AliasPoint>(buf, FRAMES_POS, min);
                FRAMES_POS += QuakeCommon::ReadData<AliasPoint>(buf, FRAMES_POS, max);

                AliasFrameName framename;
                FRAMES_POS += QuakeCommon::ReadData<AliasFrameName>(buf, FRAMES_POS, framename);

                if (frame.name.IsEmpty())
                {
                    frame.name = framename.str;
                }

                for (int c = 0; c < aliasHeader.numverts; c++)
                {
                    FRAMES_POS += QuakeCommon::ReadData<AliasPoint>(
                        buf,
                        FRAMES_POS,
                        pose.points[c]
                        );
                }

                m_poses.Add(pose);
            }

            m_frames.Add(frame);
        }
    }
}

const FVector Alias::UnpackVertex(AliasPoint in) const
{
    FVector out;

    for (int i = 0; i < 3; i++)
    {
        out[i] = (m_scale[i] * in.position[i]) + m_origin[i];
    }

    return out;
}

const FVector Alias::GetNormal(uint32 index) const
{
    float normals[162][3] = {

        {-0.525731f, 0.000000f, 0.850651f},
        {-0.442863f, 0.238856f, 0.864188f},
        {-0.295242f, 0.000000f, 0.955423f},
        {-0.309017f, 0.500000f, 0.809017f},
        {-0.162460f, 0.262866f, 0.951056f},
        {0.000000f, 0.000000f, 1.000000f},
        {0.000000f, 0.850651f, 0.525731f},
        {-0.147621f, 0.716567f, 0.681718f},
        {0.147621f, 0.716567f, 0.681718f},
        {0.000000f, 0.525731f, 0.850651f},
        {0.309017f, 0.500000f, 0.809017f},
        {0.525731f, 0.000000f, 0.850651f},
        {0.295242f, 0.000000f, 0.955423f},
        {0.442863f, 0.238856f, 0.864188f},
        {0.162460f, 0.262866f, 0.951056f},
        {-0.681718f, 0.147621f, 0.716567f},
        {-0.809017f, 0.309017f, 0.500000f},
        {-0.587785f, 0.425325f, 0.688191f},
        {-0.850651f, 0.525731f, 0.000000f},
        {-0.864188f, 0.442863f, 0.238856f},
        {-0.716567f, 0.681718f, 0.147621f},
        {-0.688191f, 0.587785f, 0.425325f},
        {-0.500000f, 0.809017f, 0.309017f},
        {-0.238856f, 0.864188f, 0.442863f},
        {-0.425325f, 0.688191f, 0.587785f},
        {-0.716567f, 0.681718f, -0.147621f},
        {-0.500000f, 0.809017f, -0.309017f},
        {-0.525731f, 0.850651f, 0.000000f},
        {0.000000f, 0.850651f, -0.525731f},
        {-0.238856f, 0.864188f, -0.442863f},
        {0.000000f, 0.955423f, -0.295242f},
        {-0.262866f, 0.951056f, -0.162460f},
        {0.000000f, 1.000000f, 0.000000f},
        {0.000000f, 0.955423f, 0.295242f},
        {-0.262866f, 0.951056f, 0.162460f},
        {0.238856f, 0.864188f, 0.442863f},
        {0.262866f, 0.951056f, 0.162460f},
        {0.500000f, 0.809017f, 0.309017f},
        {0.238856f, 0.864188f, -0.442863f},
        {0.262866f, 0.951056f, -0.162460f},
        {0.500000f, 0.809017f, -0.309017f},
        {0.850651f, 0.525731f, 0.000000f},
        {0.716567f, 0.681718f, 0.147621f},
        {0.716567f, 0.681718f, -0.147621f},
        {0.525731f, 0.850651f, 0.000000f},
        {0.425325f, 0.688191f, 0.587785f},
        {0.864188f, 0.442863f, 0.238856f},
        {0.688191f, 0.587785f, 0.425325f},
        {0.809017f, 0.309017f, 0.500000f},
        {0.681718f, 0.147621f, 0.716567f},
        {0.587785f, 0.425325f, 0.688191f},
        {0.955423f, 0.295242f, 0.000000f},
        {1.000000f, 0.000000f, 0.000000f},
        {0.951056f, 0.162460f, 0.262866f},
        {0.850651f, -0.525731f, 0.000000f},
        {0.955423f, -0.295242f, 0.000000f},
        {0.864188f, -0.442863f, 0.238856f},
        {0.951056f, -0.162460f, 0.262866f},
        {0.809017f, -0.309017f, 0.500000f},
        {0.681718f, -0.147621f, 0.716567f},
        {0.850651f, 0.000000f, 0.525731f},
        {0.864188f, 0.442863f, -0.238856f},
        {0.809017f, 0.309017f, -0.500000f},
        {0.951056f, 0.162460f, -0.262866f},
        {0.525731f, 0.000000f, -0.850651f},
        {0.681718f, 0.147621f, -0.716567f},
        {0.681718f, -0.147621f, -0.716567f},
        {0.850651f, 0.000000f, -0.525731f},
        {0.809017f, -0.309017f, -0.500000f},
        {0.864188f, -0.442863f, -0.238856f},
        {0.951056f, -0.162460f, -0.262866f},
        {0.147621f, 0.716567f, -0.681718f},
        {0.309017f, 0.500000f, -0.809017f},
        {0.425325f, 0.688191f, -0.587785f},
        {0.442863f, 0.238856f, -0.864188f},
        {0.587785f, 0.425325f, -0.688191f},
        {0.688191f, 0.587785f, -0.425325f},
        {-0.147621f, 0.716567f, -0.681718f},
        {-0.309017f, 0.500000f, -0.809017f},
        {0.000000f, 0.525731f, -0.850651f},
        {-0.525731f, 0.000000f, -0.850651f},
        {-0.442863f, 0.238856f, -0.864188f},
        {-0.295242f, 0.000000f, -0.955423f},
        {-0.162460f, 0.262866f, -0.951056f},
        {0.000000f, 0.000000f, -1.000000f},
        {0.295242f, 0.000000f, -0.955423f},
        {0.162460f, 0.262866f, -0.951056f},
        {-0.442863f, -0.238856f, -0.864188f},
        {-0.309017f, -0.500000f, -0.809017f},
        {-0.162460f, -0.262866f, -0.951056f},
        {0.000000f, -0.850651f, -0.525731f},
        {-0.147621f, -0.716567f, -0.681718f},
        {0.147621f, -0.716567f, -0.681718f},
        {0.000000f, -0.525731f, -0.850651f},
        {0.309017f, -0.500000f, -0.809017f},
        {0.442863f, -0.238856f, -0.864188f},
        {0.162460f, -0.262866f, -0.951056f},
        {0.238856f, -0.864188f, -0.442863f},
        {0.500000f, -0.809017f, -0.309017f},
        {0.425325f, -0.688191f, -0.587785f},
        {0.716567f, -0.681718f, -0.147621f},
        {0.688191f, -0.587785f, -0.425325f},
        {0.587785f, -0.425325f, -0.688191f},
        {0.000000f, -0.955423f, -0.295242f},
        {0.000000f, -1.000000f, 0.000000f},
        {0.262866f, -0.951056f, -0.162460f},
        {0.000000f, -0.850651f, 0.525731f},
        {0.000000f, -0.955423f, 0.295242f},
        {0.238856f, -0.864188f, 0.442863f},
        {0.262866f, -0.951056f, 0.162460f},
        {0.500000f, -0.809017f, 0.309017f},
        {0.716567f, -0.681718f, 0.147621f},
        {0.525731f, -0.850651f, 0.000000f},
        {-0.238856f, -0.864188f, -0.442863f},
        {-0.500000f, -0.809017f, -0.309017f},
        {-0.262866f, -0.951056f, -0.162460f},
        {-0.850651f, -0.525731f, 0.000000f},
        {-0.716567f, -0.681718f, -0.147621f},
        {-0.716567f, -0.681718f, 0.147621f},
        {-0.525731f, -0.850651f, 0.000000f},
        {-0.500000f, -0.809017f, 0.309017f},
        {-0.238856f, -0.864188f, 0.442863f},
        {-0.262866f, -0.951056f, 0.162460f},
        {-0.864188f, -0.442863f, 0.238856f},
        {-0.809017f, -0.309017f, 0.500000f},
        {-0.688191f, -0.587785f, 0.425325f},
        {-0.681718f, -0.147621f, 0.716567f},
        {-0.442863f, -0.238856f, 0.864188f},
        {-0.587785f, -0.425325f, 0.688191f},
        {-0.309017f, -0.500000f, 0.809017f},
        {-0.147621f, -0.716567f, 0.681718f},
        {-0.425325f, -0.688191f, 0.587785f},
        {-0.162460f, -0.262866f, 0.951056f},
        {0.442863f, -0.238856f, 0.864188f},
        {0.162460f, -0.262866f, 0.951056f},
        {0.309017f, -0.500000f, 0.809017f},
        {0.147621f, -0.716567f, 0.681718f},
        {0.000000f, -0.525731f, 0.850651f},
        {0.425325f, -0.688191f, 0.587785f},
        {0.587785f, -0.425325f, 0.688191f},
        {0.688191f, -0.587785f, 0.425325f},
        {-0.955423f, 0.295242f, 0.000000f},
        {-0.951056f, 0.162460f, 0.262866f},
        {-1.000000f, 0.000000f, 0.000000f},
        {-0.850651f, 0.000000f, 0.525731f},
        {-0.955423f, -0.295242f, 0.000000f},
        {-0.951056f, -0.162460f, 0.262866f},
        {-0.864188f, 0.442863f, -0.238856f},
        {-0.951056f, 0.162460f, -0.262866f},
        {-0.809017f, 0.309017f, -0.500000f},
        {-0.864188f, -0.442863f, -0.238856f},
        {-0.951056f, -0.162460f, -0.262866f},
        {-0.809017f, -0.309017f, -0.500000f},
        {-0.681718f, 0.147621f, -0.716567f},
        {-0.681718f, -0.147621f, -0.716567f},
        {-0.850651f, 0.000000f, -0.525731f},
        {-0.688191f, 0.587785f, -0.425325f},
        {-0.587785f, 0.425325f, -0.688191f},
        {-0.425325f, 0.688191f, -0.587785f},
        {-0.425325f, -0.688191f, -0.587785f},
        {-0.587785f, -0.425325f, -0.688191f},
        {-0.688191f, -0.587785f, -0.425325f},

    };

    FVector vector(
        normals[index][0],
        normals[index][1],
        normals[index][2]);

    return vector;
}
