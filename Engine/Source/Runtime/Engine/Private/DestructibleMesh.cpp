// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DestructibleMesh.cpp: UDestructibleMesh methods.
=============================================================================*/

#include "EnginePrivate.h"
#include "PhysicsPublic.h"
#include "StaticMeshResources.h"
#include "Engine/DestructibleMesh.h"
#include "Engine/DestructibleFractureSettings.h"
#include "PhysicsEngine/PhysXSupport.h"
#include "EditorFramework/AssetImportData.h"

#if WITH_APEX && WITH_EDITOR
#include "ApexDestructibleAssetImport.h"
#endif
#include "PhysicalMaterials/PhysicalMaterial.h"

DEFINE_LOG_CATEGORY(LogDestructible)

UDestructibleMesh::UDestructibleMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDestructibleMesh::PostLoad()
{
	Super::PostLoad();

	// BodySetup is used for uniform lookup of PhysicalMaterials.
	CreateBodySetup();

	// Fix location of destructible physical material
#if WITH_APEX
	// destructible mesh needs to re-evaluate the number of max gpu bone count for each chunk
	// we'll have to rechunk if that exceeds project setting
	// since this might not work well if outside of editor
	// you'll have to save re-chunked asset here, but I don't have a good way to let user
	// know here since PostLoad invalidate any dirty package mark. 
	FSkeletalMeshResource* ImportedResource = GetImportedResource();
	static IConsoleVariable* MaxBonesVar = IConsoleManager::Get().FindConsoleVariable(TEXT("Compat.MAX_GPUSKIN_BONES"));
	const int32 MaxGPUSkinBones = MaxBonesVar->GetInt();
	// if this doesn't have the right MAX GPU Bone count, recreate it. 
	for(int32 LodIndex=0; LodIndex<LODInfo.Num(); LodIndex++)
	{
		FSkeletalMeshLODInfo& ThisLODInfo = LODInfo[LodIndex];
		FStaticLODModel& ThisLODModel = ImportedResource->LODModels[LodIndex];

		// Check that we list the root bone as an active bone.
		if(!ThisLODModel.ActiveBoneIndices.Contains(0))
		{
			ThisLODModel.ActiveBoneIndices.Add(0);
			ThisLODModel.ActiveBoneIndices.Sort();
		}

		for (int32 ChunkIndex = 0; ChunkIndex < ThisLODModel.Chunks.Num(); ++ChunkIndex)
		{
			if (ThisLODModel.Chunks[ChunkIndex].BoneMap.Num() > MaxGPUSkinBones)
			{
#if WITH_EDITOR
				// re create destructible asset if it exceeds
				if (ApexDestructibleAsset != NULL)
				{
					SetApexDestructibleAsset(*this, *ApexDestructibleAsset, NULL, EDestructibleImportOptions::PreserveSettings);
				}
#else
				UE_LOG(LogDestructible, Warning, TEXT("Can't render %s asset because it exceeds max GPU skin bones supported(%d). You'll need to resave this in the editor."), *GetName(), MaxGPUSkinBones);
#endif
				// we don't have to do this multiple times, if one failes, do it once, and get out
				break;
			}
		}
	}
#endif

}

#if WITH_EDITOR
void UDestructibleMesh::PreEditChange(UProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	CreateBodySetup();
}
#endif

float ImpactResistanceToAPEX(bool bCustomResistance, float ImpactResistance)
{
	return bCustomResistance ? ImpactResistance : 0.f;
}


void APEXToImpactResistance(bool& bCustomImpactResistance, float& ImpactResistance)
{
	//apex interprets 0 as disabled, but we want custom flag
	bCustomImpactResistance = ImpactResistance != 0.f;
	if (ImpactResistance == 0.f)
	{
		ImpactResistance = 1.f;
	}
}

int32 DefaultImpactDamageDepthToAPEX(bool bEnableImpactDamage, int32 DefaultImpactDamageDepth)
{
	return bEnableImpactDamage ? DefaultImpactDamageDepth : -1.f;
}

void APEXToDefaultImpactDamageDepth(bool& bEnableImpactDamage, int32& DefaultImpactDamageDepth)
{
	//apex interprets -1 as disabled, but we want custom flag
	bEnableImpactDamage = DefaultImpactDamageDepth != -1;
	DefaultImpactDamageDepth = 1;
}

int32 DebrisDepthToAPEX(bool bEnableDebris, int32 DebrisDepth)
{
	return bEnableDebris ? DebrisDepth : -1;
}

void APEXToDebrisDepth(bool& bEnableDebris, int32& DebrisDepth)
{
	bEnableDebris = DebrisDepth != -1;
	if (DebrisDepth == -1)
	{
		DebrisDepth = 0;
	}
}

void UDestructibleMesh::Serialize(FArchive& Ar)
{
	Super::Serialize( Ar );

	if( Ar.IsLoading() )
	{
		// Deserializing the name of the NxDestructibleAsset
		TArray<uint8> NameBuffer;
		uint32 NameBufferSize;
		Ar << NameBufferSize;
		NameBuffer.AddUninitialized( NameBufferSize );
		Ar.Serialize( NameBuffer.GetData(), NameBufferSize );

		// Buffer for the NxDestructibleAsset
		TArray<uint8> Buffer;
		uint32 Size;
		Ar << Size;
		if( Size > 0 )
		{
			// Size is non-zero, so a binary blob follows
			Buffer.AddUninitialized( Size );
			Ar.Serialize( Buffer.GetData(), Size );
#if WITH_APEX
			// Wrap this blob with the APEX read stream class
			physx::PxFileBuf* Stream = GApexSDK->createMemoryReadStream( Buffer.GetData(), Size );
			// Create an NxParameterized serializer
			NxParameterized::Serializer* Serializer = GApexSDK->createSerializer(NxParameterized::Serializer::NST_BINARY);
			// Deserialize into a DeserializedData buffer
			NxParameterized::Serializer::DeserializedData DeserializedData;
			Serializer->deserialize( *Stream, DeserializedData );
			NxApexAsset* ApexAsset = NULL;
			if( DeserializedData.size() > 0 )
			{
				// The DeserializedData has something in it, so create an APEX asset from it
				ApexAsset = GApexSDK->createAsset( DeserializedData[0], NULL );//(const char*)NameBuffer.GetData() );	// BRG - don't require it to have a given name
				// Make sure it's a Destructible asset
				if (ApexAsset && ApexAsset->getObjTypeID() != GApexModuleDestructible->getModuleID())
				{
					GPhysCommandHandler->DeferredRelease(ApexAsset);
					ApexAsset = NULL;
				}
			}
			ApexDestructibleAsset = (NxDestructibleAsset*)ApexAsset;
			// Release our temporary objects
			Serializer->release();
			GApexSDK->releaseMemoryReadStream( *Stream );
#endif
		}
	}
	else if ( Ar.IsSaving() )
	{
		const char* Name = "NO_APEX";
#if WITH_APEX
		// Get our Destructible asset's name
		Name = NULL;
		if( ApexDestructibleAsset != NULL )
		{
			Name = ApexDestructibleAsset->getName();
		}
		if( Name == NULL )
		{
			Name = "";
		}
#endif
		// Serialize the name
		uint32 NameBufferSize = FCStringAnsi::Strlen( Name )+1;
		Ar << NameBufferSize;
		Ar.Serialize( (void*)Name, NameBufferSize );
#if WITH_APEX
		// Create an APEX write stream
		physx::PxFileBuf* Stream = GApexSDK->createMemoryWriteStream();
		// Create an NxParameterized serializer
		NxParameterized::Serializer* Serializer = GApexSDK->createSerializer(NxParameterized::Serializer::NST_BINARY);
		uint32 Size = 0;
		TArray<uint8> Buffer;
		// Get the NxParameterized data for our Destructible asset
		if( ApexDestructibleAsset != NULL )
		{
			const NxParameterized::Interface* AssetParameterized = ApexDestructibleAsset->getAssetNxParameterized();
			if( AssetParameterized != NULL )
			{
				// Serialize the data into the stream
				Serializer->serialize( *Stream, &AssetParameterized, 1 );
				// Read the stream data into our buffer for UE serialzation
				Size = Stream->getFileLength();
				Buffer.AddUninitialized( Size );
				Stream->read( Buffer.GetData(), Size );
			}
		}
		Ar << Size;
		if ( Size > 0 )
		{
			Ar.Serialize( Buffer.GetData(), Size );
		}
		// Release our temporary objects
		Serializer->release();
		Stream->release();
#else
		uint32 size=0;
		Ar << size;
#endif
	}

	if (Ar.UE4Ver() < VER_UE4_CLEAN_DESTRUCTIBLE_SETTINGS)
	{
		APEXToImpactResistance(DefaultDestructibleParameters.DamageParameters.bCustomImpactResistance, DefaultDestructibleParameters.DamageParameters.ImpactResistance);
		APEXToDefaultImpactDamageDepth(DefaultDestructibleParameters.DamageParameters.bEnableImpactDamage, DefaultDestructibleParameters.DamageParameters.DefaultImpactDamageDepth);
		APEXToDebrisDepth(DefaultDestructibleParameters.SpecialHierarchyDepths.bEnableDebris, DefaultDestructibleParameters.SpecialHierarchyDepths.DebrisDepth);
	}
}

void UDestructibleMesh::FinishDestroy()
{
#if WITH_APEX
	if (ApexDestructibleAsset != NULL)
	{
		GPhysCommandHandler->DeferredRelease(ApexDestructibleAsset);
		ApexDestructibleAsset = NULL;
	}
#endif

	Super::FinishDestroy();
}

#if WITH_APEX

void FDestructibleDamageParameters::FillDestructibleActorDesc(NxParameterized::Interface* Params, UPhysicalMaterial* PhysMat) const
{
	// Damage parameters
	verify(NxParameterized::setParamF32(*Params, "defaultBehaviorGroup.damageThreshold", DamageThreshold * PhysMat->DestructibleDamageThresholdScale));
	verify(NxParameterized::setParamF32(*Params, "defaultBehaviorGroup.damageToRadius", DamageSpread));
	verify(NxParameterized::setParamF32(*Params, "destructibleParameters.forceToDamage", ImpactDamage));
	verify(NxParameterized::setParamF32(*Params, "defaultBehaviorGroup.materialStrength", ImpactResistanceToAPEX(bCustomImpactResistance, ImpactResistance)));
	verify(NxParameterized::setParamI32(*Params, "destructibleParameters.impactDamageDefaultDepth", DefaultImpactDamageDepthToAPEX(bEnableImpactDamage, DefaultImpactDamageDepth)));
}

void FDestructibleDamageParameters::LoadDefaultDestructibleParametersFromApexAsset(const NxParameterized::Interface* Params)
{
	// Damage parameters
	verify(NxParameterized::getParamF32(*Params, "defaultBehaviorGroup.damageThreshold", DamageThreshold));
	verify(NxParameterized::getParamF32(*Params, "defaultBehaviorGroup.damageToRadius", DamageSpread));
	verify(NxParameterized::getParamF32(*Params, "destructibleParameters.forceToDamage", ImpactDamage));
	verify(NxParameterized::getParamF32(*Params, "defaultBehaviorGroup.materialStrength",ImpactResistance));
	verify(NxParameterized::getParamI32(*Params, "destructibleParameters.impactDamageDefaultDepth", DefaultImpactDamageDepth));

	APEXToImpactResistance(bCustomImpactResistance, ImpactResistance);
	APEXToDefaultImpactDamageDepth(bEnableImpactDamage, DefaultImpactDamageDepth);
}

void FDestructibleSpecialHierarchyDepths::FillDestructibleActorDesc(NxParameterized::Interface* Params) const
{
	verify(NxParameterized::setParamU32(*Params, "supportDepth", SupportDepth));
	verify(NxParameterized::setParamU32(*Params, "destructibleParameters.minimumFractureDepth", MinimumFractureDepth));
	verify(NxParameterized::setParamI32(*Params, "destructibleParameters.debrisDepth", DebrisDepthToAPEX(bEnableDebris,DebrisDepth)) );
	verify(NxParameterized::setParamU32(*Params, "destructibleParameters.essentialDepth", EssentialDepth));
}

void FDestructibleSpecialHierarchyDepths::LoadDefaultDestructibleParametersFromApexAsset(const NxParameterized::Interface* Params)
{
	physx::PxU32 PSupportDepth;
	verify(NxParameterized::getParamU32(*Params, "supportDepth", PSupportDepth));
	SupportDepth = (int32)PSupportDepth;
	physx::PxU32 PMinimumFractureDepth;
	verify(NxParameterized::getParamU32(*Params, "destructibleParameters.minimumFractureDepth", PMinimumFractureDepth));
	MinimumFractureDepth = (int32)PMinimumFractureDepth;
	verify(NxParameterized::getParamI32(*Params, "destructibleParameters.debrisDepth", DebrisDepth));
	APEXToDebrisDepth(bEnableDebris, DebrisDepth);

	physx::PxU32 PEssentialDepth;
	verify(NxParameterized::getParamU32(*Params, "destructibleParameters.essentialDepth", PEssentialDepth));
	EssentialDepth = (int32)PEssentialDepth;
}

void FDestructibleAdvancedParameters::FillDestructibleActorDesc(NxParameterized::Interface* Params) const
{
	verify(NxParameterized::setParamF32(*Params, "destructibleParameters.damageCap", DamageCap));
	verify(NxParameterized::setParamF32(*Params, "destructibleParameters.impactVelocityThreshold", ImpactVelocityThreshold));
	verify(NxParameterized::setParamF32(*Params, "destructibleParameters.maxChunkSpeed", MaxChunkSpeed));
	verify(NxParameterized::setParamF32(*Params, "destructibleParameters.fractureImpulseScale", FractureImpulseScale));
}

void FDestructibleAdvancedParameters::LoadDefaultDestructibleParametersFromApexAsset(const NxParameterized::Interface* Params)
{
	verify(NxParameterized::getParamF32(*Params, "destructibleParameters.damageCap", DamageCap));
	verify(NxParameterized::getParamF32(*Params, "destructibleParameters.impactVelocityThreshold", ImpactVelocityThreshold));
	verify(NxParameterized::getParamF32(*Params, "destructibleParameters.maxChunkSpeed", MaxChunkSpeed));
	verify(NxParameterized::getParamF32(*Params, "destructibleParameters.fractureImpulseScale", FractureImpulseScale));
}

void FDestructibleDebrisParameters::FillDestructibleActorDesc(NxParameterized::Interface* Params) const
{
	verify(NxParameterized::setParamF32(*Params, "destructibleParameters.debrisLifetimeMin", DebrisLifetimeMin));
	verify(NxParameterized::setParamF32(*Params, "destructibleParameters.debrisLifetimeMax", DebrisLifetimeMax));
	verify(NxParameterized::setParamF32(*Params, "destructibleParameters.debrisMaxSeparationMin", DebrisMaxSeparationMin));
	verify(NxParameterized::setParamF32(*Params, "destructibleParameters.debrisMaxSeparationMax", DebrisMaxSeparationMax));
	physx::PxBounds3 PValidBounds;
	PValidBounds.minimum.x = ValidBounds.Min.X;
	PValidBounds.minimum.y = ValidBounds.Min.Y;
	PValidBounds.minimum.z = ValidBounds.Min.Z;
	PValidBounds.maximum.x = ValidBounds.Max.X;
	PValidBounds.maximum.y = ValidBounds.Max.Y;
	PValidBounds.maximum.z = ValidBounds.Max.Z;
	verify(NxParameterized::setParamBounds3(*Params, "destructibleParameters.validBounds", PValidBounds));
}

void FDestructibleDebrisParameters::LoadDefaultDestructibleParametersFromApexAsset(const NxParameterized::Interface* Params)
{
	verify(NxParameterized::getParamF32(*Params, "destructibleParameters.debrisLifetimeMin", DebrisLifetimeMin));
	verify(NxParameterized::getParamF32(*Params, "destructibleParameters.debrisLifetimeMax", DebrisLifetimeMax));
	verify(NxParameterized::getParamF32(*Params, "destructibleParameters.debrisMaxSeparationMin", DebrisMaxSeparationMin));
	verify(NxParameterized::getParamF32(*Params, "destructibleParameters.debrisMaxSeparationMax", DebrisMaxSeparationMax));
	physx::PxBounds3 PValidBounds;
	verify(NxParameterized::getParamBounds3(*Params, "destructibleParameters.validBounds", PValidBounds));
	ValidBounds.Min.X = PValidBounds.minimum.x;
	ValidBounds.Min.Y = PValidBounds.minimum.y;
	ValidBounds.Min.Z = PValidBounds.minimum.z;
	ValidBounds.Max.X = PValidBounds.maximum.x;
	ValidBounds.Max.Y = PValidBounds.maximum.y;
	ValidBounds.Max.Z = PValidBounds.maximum.z;

}

void FDestructibleParametersFlag::FillDestructibleActorDesc(NxParameterized::Interface* Params) const
{
	verify(NxParameterized::setParamBool(*Params, "destructibleParameters.flags.ACCUMULATE_DAMAGE", bAccumulateDamage));
	verify(NxParameterized::setParamBool(*Params, "useAssetDefinedSupport", bAssetDefinedSupport));
	verify(NxParameterized::setParamBool(*Params, "useWorldSupport", bWorldSupport));
	verify(NxParameterized::setParamBool(*Params, "destructibleParameters.flags.DEBRIS_TIMEOUT", bDebrisTimeout));
	verify(NxParameterized::setParamBool(*Params, "destructibleParameters.flags.DEBRIS_MAX_SEPARATION", bDebrisMaxSeparation));
	verify(NxParameterized::setParamBool(*Params, "destructibleParameters.flags.CRUMBLE_SMALLEST_CHUNKS", bCrumbleSmallestChunks));
	verify(NxParameterized::setParamBool(*Params, "destructibleParameters.flags.ACCURATE_RAYCASTS", bAccurateRaycasts));
	verify(NxParameterized::setParamBool(*Params, "destructibleParameters.flags.USE_VALID_BOUNDS", bUseValidBounds));
	verify(NxParameterized::setParamBool(*Params, "formExtendedStructures", bFormExtendedStructures));
}

void FDestructibleParametersFlag::LoadDefaultDestructibleParametersFromApexAsset(const NxParameterized::Interface* Params)
{
	bool bFlag;
	verify(NxParameterized::getParamBool(*Params, "destructibleParameters.flags.ACCUMULATE_DAMAGE", bFlag));
	bAccumulateDamage = bFlag ? 1 : 0;
	verify(NxParameterized::getParamBool(*Params, "useAssetDefinedSupport", bFlag));
	bAssetDefinedSupport = bFlag ? 1 : 0;
	verify(NxParameterized::getParamBool(*Params, "useWorldSupport", bFlag));
	bWorldSupport = bFlag ? 1 : 0;
	verify(NxParameterized::getParamBool(*Params, "destructibleParameters.flags.DEBRIS_TIMEOUT", bFlag));
	bDebrisTimeout = bFlag ? 1 : 0;
	verify(NxParameterized::getParamBool(*Params, "destructibleParameters.flags.DEBRIS_MAX_SEPARATION", bFlag));
	bDebrisMaxSeparation = bFlag ? 1 : 0;
	verify(NxParameterized::getParamBool(*Params, "destructibleParameters.flags.CRUMBLE_SMALLEST_CHUNKS", bFlag));
	bCrumbleSmallestChunks = bFlag ? 1 : 0;
	verify(NxParameterized::getParamBool(*Params, "destructibleParameters.flags.ACCURATE_RAYCASTS", bFlag));
	bAccurateRaycasts = bFlag ? 1 : 0;
	verify(NxParameterized::getParamBool(*Params, "destructibleParameters.flags.USE_VALID_BOUNDS", bFlag));
	bUseValidBounds = bFlag ? 1 : 0;
	verify(NxParameterized::getParamBool(*Params, "formExtendedStructures", bFlag));
	bFormExtendedStructures = bFlag ? 1 : 0;
}

void FDestructibleDepthParameters::FillDestructibleActorDesc(NxParameterized::Interface* Params, const char* OverrideName, const char* OverrideValueName) const
{
	switch (ImpactDamageOverride)
	{
	case IDO_None:
		verify(NxParameterized::setParamBool(*Params, OverrideName, false));
		break;
	case IDO_On:
		verify(NxParameterized::setParamBool(*Params, OverrideName, true));
		verify(NxParameterized::setParamBool(*Params, OverrideValueName, true));
		break;
	case IDO_Off:
		verify(NxParameterized::setParamBool(*Params, OverrideName, true));
		verify(NxParameterized::setParamBool(*Params, OverrideValueName, false));
		break;
	default:
		break;
	}
}

void FDestructibleDepthParameters::LoadDefaultDestructibleParametersFromApexAsset(const NxParameterized::Interface* Params, const char* OverrideName, const char* OverrideValueName)
{
	bool bOverride;
	bool bOverrideValue;
	verify(NxParameterized::getParamBool(*Params, OverrideName, bOverride));
	verify(NxParameterized::getParamBool(*Params, OverrideValueName, bOverrideValue));

	if (!bOverride)
	{
		ImpactDamageOverride = IDO_None;
	}
	else
	{
		ImpactDamageOverride = bOverrideValue ? IDO_On : IDO_Off;
	}
}

NxParameterized::Interface* UDestructibleMesh::GetDestructibleActorDesc(UPhysicalMaterial* PhysMat)
{
	NxParameterized::Interface* Params = NULL;

	if (ApexDestructibleAsset != NULL)
	{
		Params = ApexDestructibleAsset->getDefaultActorDesc();
	}
	if (Params != NULL)
	{
		DefaultDestructibleParameters.DamageParameters.FillDestructibleActorDesc(Params, PhysMat);
		DefaultDestructibleParameters.SpecialHierarchyDepths.FillDestructibleActorDesc(Params);
		DefaultDestructibleParameters.AdvancedParameters.FillDestructibleActorDesc(Params);
		DefaultDestructibleParameters.DebrisParameters.FillDestructibleActorDesc(Params);
		DefaultDestructibleParameters.Flags.FillDestructibleActorDesc(Params);
		
		//depth params done per level
		for (int32 Depth = 0; Depth < DefaultDestructibleParameters.DepthParameters.Num(); ++Depth)
		{
			char OverrideName[MAX_SPRINTF];
			FCStringAnsi::Sprintf(OverrideName, "depthParameters[%d].OVERRIDE_IMPACT_DAMAGE", Depth);
			char OverrideValueName[MAX_SPRINTF];
			FCStringAnsi::Sprintf(OverrideValueName, "depthParameters[%d].OVERRIDE_IMPACT_DAMAGE_VALUE", Depth);

			DefaultDestructibleParameters.DepthParameters[Depth].FillDestructibleActorDesc(Params, OverrideName, OverrideValueName);
		}
	}

	return Params;
}
#endif // WITH_APEX

void UDestructibleMesh::LoadDefaultDestructibleParametersFromApexAsset()
{
#if WITH_APEX
	const NxParameterized::Interface* Params = ApexDestructibleAsset->getAssetNxParameterized();

	if (Params != NULL)
	{
		DefaultDestructibleParameters.DebrisParameters.LoadDefaultDestructibleParametersFromApexAsset(Params);
		DefaultDestructibleParameters.SpecialHierarchyDepths.LoadDefaultDestructibleParametersFromApexAsset(Params);
		DefaultDestructibleParameters.AdvancedParameters.LoadDefaultDestructibleParametersFromApexAsset(Params);
		DefaultDestructibleParameters.DamageParameters.LoadDefaultDestructibleParametersFromApexAsset(Params);
		DefaultDestructibleParameters.Flags.LoadDefaultDestructibleParametersFromApexAsset(Params);

		// Depth parameters
		for (int32 Depth = 0; Depth < DefaultDestructibleParameters.DepthParameters.Num(); ++Depth)
		{
			char OverrideName[MAX_SPRINTF];
			FCStringAnsi::Sprintf(OverrideName, "depthParameters[%d].OVERRIDE_IMPACT_DAMAGE", Depth);
			char OverrideValueName[MAX_SPRINTF];
			FCStringAnsi::Sprintf(OverrideValueName, "depthParameters[%d].OVERRIDE_IMPACT_DAMAGE_VALUE", Depth);

			DefaultDestructibleParameters.DepthParameters[Depth].LoadDefaultDestructibleParametersFromApexAsset(Params, OverrideName, OverrideValueName);
		}
	}
#endif
}

void UDestructibleMesh::CreateFractureSettings()
{
#if WITH_EDITORONLY_DATA
	if (FractureSettings == NULL)
	{
		FractureSettings = NewObject<UDestructibleFractureSettings>(this);
		check(FractureSettings);
	}
#endif	// WITH_EDITORONLY_DATA
}

#if WITH_APEX && WITH_EDITORONLY_DATA

bool CreateSubmeshFromSMSection(const FStaticMeshLODResources& RenderMesh, int32 SubmeshIdx, const FStaticMeshSection& Section, physx::NxExplicitSubmeshData& SubmeshData, TArray<physx::NxExplicitRenderTriangle>& Triangles)
{
	// Create submesh descriptor, just a material name and a vertex format
	FCStringAnsi::Strncpy(SubmeshData.mMaterialName, TCHAR_TO_ANSI(*FString::Printf(TEXT("Material%d"),Section.MaterialIndex)), physx::NxExplicitSubmeshData::MaterialNameBufferSize);
	SubmeshData.mVertexFormat.mHasStaticPositions = SubmeshData.mVertexFormat.mHasStaticNormals = SubmeshData.mVertexFormat.mHasStaticTangents = true;
	SubmeshData.mVertexFormat.mHasStaticBinormals = true;
	SubmeshData.mVertexFormat.mBonesPerVertex = 1;
	SubmeshData.mVertexFormat.mUVCount =  FMath::Min((physx::PxU32)RenderMesh.VertexBuffer.GetNumTexCoords(), (physx::PxU32)NxVertexFormat::MAX_UV_COUNT);

	const uint32 NumVertexColors = RenderMesh.ColorVertexBuffer.GetNumVertices();

	FIndexArrayView StaticMeshIndices = RenderMesh.IndexBuffer.GetArrayView();

	// Now pull in the triangles from this element, all belonging to this submesh
	const int32 TriangleCount = Section.NumTriangles;
	for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
	{
		physx::NxExplicitRenderTriangle Triangle;
		for (int32 PointIndex = 0; PointIndex < 3; PointIndex++)
		{
			physx::NxVertex& Vertex = Triangle.vertices[PointIndex];
			const uint32 UnrealVertIndex = StaticMeshIndices[Section.FirstIndex + ((TriangleIndex * 3) + PointIndex)];
			Vertex.position = U2PVector(RenderMesh.PositionVertexBuffer.VertexPosition(UnrealVertIndex));	Vertex.position.y *= -1;
			Vertex.normal = U2PVector((FVector)RenderMesh.VertexBuffer.VertexTangentZ(UnrealVertIndex));	Vertex.normal.y *= -1;
			Vertex.tangent = U2PVector((FVector)RenderMesh.VertexBuffer.VertexTangentX(UnrealVertIndex));	Vertex.tangent.y *= -1;
			Vertex.binormal = U2PVector((FVector)RenderMesh.VertexBuffer.VertexTangentY(UnrealVertIndex));	Vertex.binormal.y *= -1;
			for (int32 TexCoordSourceIndex = 0; TexCoordSourceIndex < (int32)SubmeshData.mVertexFormat.mUVCount; ++TexCoordSourceIndex)
			{
				const FVector2D& TexCoord = RenderMesh.VertexBuffer.GetVertexUV(UnrealVertIndex, TexCoordSourceIndex);
				Vertex.uv[TexCoordSourceIndex].set(TexCoord.X, -TexCoord.Y + 1.0);
			}
			FLinearColor VertColor(1.0f, 1.0f, 1.0f);
			if (UnrealVertIndex < NumVertexColors)
			{
				VertColor = RenderMesh.ColorVertexBuffer.VertexColor(UnrealVertIndex).ReinterpretAsLinear();
			}
			
			Vertex.color.set(VertColor.R, VertColor.G, VertColor.B, VertColor.A);
			// Don't need to write the bone index, since there is only one bone (index zero), and the default value is zero
			// Nor do we need to write the bone weights, since there is only one bone per vertex (the weight is assumed to be 1)
		}
		Triangle.submeshIndex = SubmeshIdx;
		Triangle.smoothingMask = 0;
		Triangle.extraDataIndex = 0xFFFFFFFF;
		Triangles.Add(Triangle);
	}

	return true;
}

#endif // WITH_APEX && WITH_EDITORONLY_DATA

bool UDestructibleMesh::BuildFractureSettingsFromStaticMesh(UStaticMesh* StaticMesh)
{
#if WITH_APEX && WITH_EDITOR
	// Make sure we have the authoring data container
	CreateFractureSettings();

	// Array of render meshes to create the fracture settings from. The first RenderMesh is the root mesh, while the other ones are 
	// imported to form the chunk meshes ( if no FBX was imported as level 1 chunks, this will just contain the root mesh )
	TArray<const FStaticMeshLODResources*> RenderMeshes;
	TArray<UStaticMesh*> StaticMeshes;
	TArray<uint32> MeshPartitions;

	// Do we have a main static mesh?
	const FStaticMeshLODResources& MainRenderMesh = StaticMesh->GetLODForExport(0);

	// Keep track of overall triangle and submesh count
	int32 OverallTriangleCount = MainRenderMesh.GetNumTriangles();
	int32 OverallSubmeshCount  = MainRenderMesh.Sections.Num();

	//MeshPartitions.Add(OverallTriangleCount);
		
 	RenderMeshes.Add(&MainRenderMesh);
 	StaticMeshes.Add(StaticMesh);
	
	UE_LOG(LogDestructible, Warning, TEXT("MainMesh Tris: %d"), MainRenderMesh.GetNumTriangles());

	// Get the rendermeshes for the chunks ( if any )
	for (int32 ChunkMeshIdx=0; ChunkMeshIdx < FractureChunkMeshes.Num(); ++ChunkMeshIdx)
	{
		const FStaticMeshLODResources& ChunkRM = FractureChunkMeshes[ChunkMeshIdx]->GetLODForExport(0);
		
		RenderMeshes.Add(&ChunkRM);
		StaticMeshes.Add(FractureChunkMeshes[ChunkMeshIdx]);

		UE_LOG(LogDestructible, Warning, TEXT("Chunk: %d Tris: %d"), ChunkMeshIdx, ChunkRM.GetNumTriangles());

		OverallTriangleCount += ChunkRM.GetNumTriangles();
		OverallSubmeshCount  += ChunkRM.Sections.Num();
	}

	// Build an array of NxExplicitRenderTriangles and NxExplicitSubmeshData for authoring
	TArray<physx::NxExplicitRenderTriangle> Triangles;
	Triangles.Reserve(OverallTriangleCount);
	TArray<physx::NxExplicitSubmeshData> Submeshes;	// Elements <-> Submeshes
	Submeshes.SetNum(OverallSubmeshCount);

	// UE Materials
	TArray<UMaterialInterface*> Materials;

	// Counter for submesh index
	int32 SubmeshIndexCounter = 0;

	

	// Start creating apex data
	for (int32 MeshIdx=0; MeshIdx < RenderMeshes.Num(); ++MeshIdx)
	{
		const FStaticMeshLODResources& RenderMesh = *RenderMeshes[MeshIdx];

		// Get the current static mesh to retrieve the material from
		UStaticMesh* CurrentStaticMesh = StaticMeshes[MeshIdx];

		// Loop through elements
		for (int32 SectionIndex = 0; SectionIndex < RenderMesh.Sections.Num(); ++SectionIndex, ++SubmeshIndexCounter)
		{
			const FStaticMeshSection& Section = RenderMesh.Sections[SectionIndex];
			physx::NxExplicitSubmeshData& SubmeshData = Submeshes[SubmeshIndexCounter];

			// Parallel materials array
			Materials.Add(CurrentStaticMesh->GetMaterial(Section.MaterialIndex));

			CreateSubmeshFromSMSection(RenderMesh, SubmeshIndexCounter, Section, SubmeshData, Triangles);
		}

		//if (MeshIdx > 0)
		{
			MeshPartitions.Add(Triangles.Num());
		}
	}

	FractureSettings->SetRootMesh(Triangles, Materials, Submeshes, MeshPartitions, true);
	return true;
#else // WITH_APEX && WITH_EDITOR
	return false;
#endif // WITH_APEX && WITH_EDITOR
}

ENGINE_API bool UDestructibleMesh::BuildFromStaticMesh( UStaticMesh& StaticMesh )
{
#if WITH_EDITOR
	PreEditChange(NULL);

	// Import the StaticMesh
	if (!BuildFractureSettingsFromStaticMesh(&StaticMesh))
	{
		return false;
	}

	SourceStaticMesh = &StaticMesh;

	SourceSMImportTimestamp = FDateTime::MinValue();
	if ( StaticMesh.AssetImportData && StaticMesh.AssetImportData->SourceData.SourceFiles.Num() == 1 )
	{
		SourceSMImportTimestamp = StaticMesh.AssetImportData->SourceData.SourceFiles[0].Timestamp;
	}

#if WITH_APEX
	BuildDestructibleMeshFromFractureSettings(*this, NULL);
#endif

	PostEditChange();
	MarkPackageDirty();
#endif // WITH_EDITOR
	return true;
}

ENGINE_API bool UDestructibleMesh::SetupChunksFromStaticMeshes( const TArray<UStaticMesh*>& ChunkMeshes )
{
#if WITH_EDITOR
	if (SourceStaticMesh == NULL)
	{
		UE_LOG(LogDestructible, Warning, TEXT("Unable to import FBX as level 1 chunks if the DM was not created from a static mesh."));
		return false;
	}

	PreEditChange(NULL);

	FractureChunkMeshes.Empty(ChunkMeshes.Num());
	FractureChunkMeshes.Append(ChunkMeshes);
	
	// Import the StaticMesh
	if (!BuildFractureSettingsFromStaticMesh(SourceStaticMesh))
	{
		return false;
	}

#if WITH_APEX
	BuildDestructibleMeshFromFractureSettings(*this, NULL);
#endif

	// Clear the fracture chunk meshes again
	FractureChunkMeshes.Empty();

	PostEditChange();
	MarkPackageDirty();
#endif // WITH_EDITOR
	return true;
}
