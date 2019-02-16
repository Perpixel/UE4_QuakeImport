// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"


/*
============================================
Alias

============================================
*/

// Raw Texture
struct AliasTexture
{
    TArray<uint8> data;
};

// Texture coordinates
struct AliasTexcoord
{
    int onseam; // 0 or 0x20
    int s; // 0 -> skin width
    int t;  // 0 -> skin height
};

// Triangle indices
struct AliasTriangle
{
    int front;
    int indices[3];
};

// Packed vertex position
struct AliasPoint
{
    uint8 position[3];
    uint8 lightnormalindex;
};

// Define a a group of poses in a single frame
struct AliasGroup
{
    int numframes;
    AliasPoint bboxmin;
    AliasPoint bboxmax;
};

// Animation pose
struct AliasPose
{
    TArray<AliasPoint> points;

    AliasPose(int numpoints)
    {
        points.SetNum(numpoints);
    }
};

// Animation frame description
struct AliasFrame
{
    int             firstpose;
    int             numposes;
    float           interval;
    AliasPoint      bboxmin;
    AliasPoint      bboxmax;
    int             frame;
    FString         name;
    int             type;
};

class Alias
{
public:
    Alias(const FString name, const uint8*& buf);

    // Header
    FString     m_name;
    FVector     m_scale;
    FVector     m_origin;
    FVector     m_eyePosition;
    int         m_skinWidth;
    int         m_skinHeight;
    uint32      m_numSkins;
    uint32      m_numFrames;
    uint32      m_numTris;
    uint32      m_numVerts;

    // Data
    TArray<AliasTexture>    m_skins;
    TArray<AliasTexcoord>   m_texcoords;
    TArray<AliasTriangle>   m_triangles;
    TArray<AliasFrame>      m_frames;
    TArray<AliasPose>       m_poses;

    const FVector UnpackVertex(AliasPoint in) const;
    const FVector GetNormal(uint32 index) const;

// Needed at import time only. No need to keep this in the global namespace
private:

    struct AliasHeader
    {
        int     id;
        int     version;
        float   scale[3];
        float   origin[3];
        float   radius;
        float   offsets[3];
        int     numskins;
        int     skinwidth;
        int     skinheight;
        int     numverts;
        int     numtris;
        int     numframes;
        int     synctype;
        int     flags;
        float   size;
    };
    struct AliasFrameName
    {
        char str[16];
    };

    //

};