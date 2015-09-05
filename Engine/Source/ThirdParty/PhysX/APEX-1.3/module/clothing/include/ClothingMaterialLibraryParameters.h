/*
 * Copyright (c) 2008-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */


// This file was generated by NxParameterized/scripts/GenParameterized.pl
// Created: 2015.06.02 04:11:49

#ifndef HEADER_ClothingMaterialLibraryParameters_h
#define HEADER_ClothingMaterialLibraryParameters_h

#include "NxParametersTypes.h"

#ifndef NX_PARAMETERIZED_ONLY_LAYOUTS
#include "NxParameterized.h"
#include "NxParameters.h"
#include "NxParameterizedTraits.h"
#include "NxTraitsInternal.h"
#endif

namespace physx
{
namespace apex
{
namespace clothing
{

#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to __declspec(align())

namespace ClothingMaterialLibraryParametersNS
{

struct StiffnessScaling_Type;
struct ClothingMaterial_Type;

struct ClothingMaterial_DynamicArray1D_Type
{
	ClothingMaterial_Type* buf;
	bool isAllocated;
	physx::PxI32 elementSize;
	physx::PxI32 arraySizes[1];
};

struct StiffnessScaling_Type
{
	physx::PxF32 compressionRange;
	physx::PxF32 stretchRange;
	physx::PxF32 scale;
};
struct ClothingMaterial_Type
{
	NxParameterized::DummyStringStruct materialName;
	physx::PxF32 verticalStretchingStiffness;
	physx::PxF32 horizontalStretchingStiffness;
	physx::PxF32 bendingStiffness;
	physx::PxF32 shearingStiffness;
	physx::PxF32 tetherStiffness;
	physx::PxF32 tetherLimit;
	bool orthoBending;
	StiffnessScaling_Type verticalStiffnessScaling;
	StiffnessScaling_Type horizontalStiffnessScaling;
	StiffnessScaling_Type bendingStiffnessScaling;
	StiffnessScaling_Type shearingStiffnessScaling;
	physx::PxF32 damping;
	physx::PxF32 stiffnessFrequency;
	physx::PxF32 drag;
	bool comDamping;
	physx::PxF32 friction;
	physx::PxF32 massScale;
	physx::PxU32 solverIterations;
	physx::PxF32 solverFrequency;
	physx::PxF32 gravityScale;
	physx::PxF32 inertiaScale;
	physx::PxF32 hardStretchLimitation;
	physx::PxF32 maxDistanceBias;
	physx::PxU32 hierarchicalSolverIterations;
	physx::PxF32 selfcollisionThickness;
	physx::PxF32 selfcollisionSquashScale;
	physx::PxF32 selfcollisionStiffness;
};

struct ParametersStruct
{

	ClothingMaterial_DynamicArray1D_Type materials;

};

static const physx::PxU32 checksum[] = { 0xf827ceb7, 0xd03d140a, 0x0c8ae038, 0x16333673, };

} // namespace ClothingMaterialLibraryParametersNS

#ifndef NX_PARAMETERIZED_ONLY_LAYOUTS
class ClothingMaterialLibraryParameters : public NxParameterized::NxParameters, public ClothingMaterialLibraryParametersNS::ParametersStruct
{
public:
	ClothingMaterialLibraryParameters(NxParameterized::Traits* traits, void* buf = 0, PxI32* refCount = 0);

	virtual ~ClothingMaterialLibraryParameters();

	virtual void destroy();

	static const char* staticClassName(void)
	{
		return("ClothingMaterialLibraryParameters");
	}

	const char* className(void) const
	{
		return(staticClassName());
	}

	static const physx::PxU32 ClassVersion = ((physx::PxU32)0 << 16) + (physx::PxU32)14;

	static physx::PxU32 staticVersion(void)
	{
		return ClassVersion;
	}

	physx::PxU32 version(void) const
	{
		return(staticVersion());
	}

	static const physx::PxU32 ClassAlignment = 8;

	static const physx::PxU32* staticChecksum(physx::PxU32& bits)
	{
		bits = 8 * sizeof(ClothingMaterialLibraryParametersNS::checksum);
		return ClothingMaterialLibraryParametersNS::checksum;
	}

	static void freeParameterDefinitionTable(NxParameterized::Traits* traits);

	const physx::PxU32* checksum(physx::PxU32& bits) const
	{
		return staticChecksum(bits);
	}

	const ClothingMaterialLibraryParametersNS::ParametersStruct& parameters(void) const
	{
		ClothingMaterialLibraryParameters* tmpThis = const_cast<ClothingMaterialLibraryParameters*>(this);
		return *(static_cast<ClothingMaterialLibraryParametersNS::ParametersStruct*>(tmpThis));
	}

	ClothingMaterialLibraryParametersNS::ParametersStruct& parameters(void)
	{
		return *(static_cast<ClothingMaterialLibraryParametersNS::ParametersStruct*>(this));
	}

	virtual NxParameterized::ErrorType getParameterHandle(const char* long_name, NxParameterized::Handle& handle) const;
	virtual NxParameterized::ErrorType getParameterHandle(const char* long_name, NxParameterized::Handle& handle);

	void initDefaults(void);

protected:

	virtual const NxParameterized::DefinitionImpl* getParameterDefinitionTree(void);
	virtual const NxParameterized::DefinitionImpl* getParameterDefinitionTree(void) const;


	virtual void getVarPtr(const NxParameterized::Handle& handle, void*& ptr, size_t& offset) const;

private:

	void buildTree(void);
	void initDynamicArrays(void);
	void initStrings(void);
	void initReferences(void);
	void freeDynamicArrays(void);
	void freeStrings(void);
	void freeReferences(void);

	static bool mBuiltFlag;
	static NxParameterized::MutexType mBuiltFlagMutex;
};

class ClothingMaterialLibraryParametersFactory : public NxParameterized::Factory
{
	static const char* const vptr;

public:
	virtual NxParameterized::Interface* create(NxParameterized::Traits* paramTraits)
	{
		// placement new on this class using mParameterizedTraits

		void* newPtr = paramTraits->alloc(sizeof(ClothingMaterialLibraryParameters), ClothingMaterialLibraryParameters::ClassAlignment);
		if (!NxParameterized::IsAligned(newPtr, ClothingMaterialLibraryParameters::ClassAlignment))
		{
			NX_PARAM_TRAITS_WARNING(paramTraits, "Unaligned memory allocation for class ClothingMaterialLibraryParameters");
			paramTraits->free(newPtr);
			return 0;
		}

		memset(newPtr, 0, sizeof(ClothingMaterialLibraryParameters)); // always initialize memory allocated to zero for default values
		return NX_PARAM_PLACEMENT_NEW(newPtr, ClothingMaterialLibraryParameters)(paramTraits);
	}

	virtual NxParameterized::Interface* finish(NxParameterized::Traits* paramTraits, void* bufObj, void* bufStart, physx::PxI32* refCount)
	{
		if (!NxParameterized::IsAligned(bufObj, ClothingMaterialLibraryParameters::ClassAlignment)
		        || !NxParameterized::IsAligned(bufStart, ClothingMaterialLibraryParameters::ClassAlignment))
		{
			NX_PARAM_TRAITS_WARNING(paramTraits, "Unaligned memory allocation for class ClothingMaterialLibraryParameters");
			return 0;
		}

		// Init NxParameters-part
		// We used to call empty constructor of ClothingMaterialLibraryParameters here
		// but it may call default constructors of members and spoil the data
		NX_PARAM_PLACEMENT_NEW(bufObj, NxParameterized::NxParameters)(paramTraits, bufStart, refCount);

		// Init vtable (everything else is already initialized)
		*(const char**)bufObj = vptr;

		return (ClothingMaterialLibraryParameters*)bufObj;
	}

	virtual const char* getClassName()
	{
		return (ClothingMaterialLibraryParameters::staticClassName());
	}

	virtual physx::PxU32 getVersion()
	{
		return (ClothingMaterialLibraryParameters::staticVersion());
	}

	virtual physx::PxU32 getAlignment()
	{
		return (ClothingMaterialLibraryParameters::ClassAlignment);
	}

	virtual const physx::PxU32* getChecksum(physx::PxU32& bits)
	{
		return (ClothingMaterialLibraryParameters::staticChecksum(bits));
	}
};
#endif // NX_PARAMETERIZED_ONLY_LAYOUTS

} // namespace clothing
} // namespace apex
} // namespace physx

#pragma warning(pop)

#endif