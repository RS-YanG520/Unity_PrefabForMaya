// Copyright Ciprian Stanciu 2024

#include "Runtime/Launch/Resources/Version.h"

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26

#include "CoreGlobals.h"
#include "SceneManagement.h"
#include "ExternalTexture.h"
#include "Misc/UObjectToken.h"

#include "RenderCore.h"
#include "ExportToUnity.h"

#include "VirtualTexturing.h"
#include "VT/RuntimeVirtualTexture.h"
#include "Runtime/Engine/Private/Materials/MaterialUniformExpressions.h"

DEFINE_LOG_CATEGORY( LogMaterial );

static FGuid GetExternalTextureGuid(const FMaterialRenderContext& Context, const FGuid& ExternalTextureGuid, const FName& ParameterName, int32 SourceTextureIndex)
{
	FGuid GuidToLookup;
	if (ExternalTextureGuid.IsValid())
	{
		// Use the compile-time GUID if it is set
		GuidToLookup = ExternalTextureGuid;
	}
	else
	{
		const UTexture* TextureParameterObject = nullptr;
		if (!ParameterName.IsNone() && Context.MaterialRenderProxy && Context.MaterialRenderProxy->GetTextureValue(ParameterName, &TextureParameterObject, Context) && TextureParameterObject)
		{
			GuidToLookup = TextureParameterObject->GetExternalTextureGuid();
		}
		else
		{
			// Otherwise attempt to use the texture index in the material, if it's valid
			const UTexture* TextureObject = SourceTextureIndex != INDEX_NONE ? GetIndexedTexture<UTexture>(Context.Material, SourceTextureIndex) : nullptr;
			if (TextureObject)
			{
				GuidToLookup = TextureObject->GetExternalTextureGuid();
			}
		}
	}
	return GuidToLookup;
}

static void GetTextureParameterValue(const FHashedMaterialParameterInfo& ParameterInfo, int32 TextureIndex, const FMaterialRenderContext& Context, const UTexture*& OutValue)
{
	if (ParameterInfo.Name.IsNone())
	{
		OutValue = GetIndexedTexture<UTexture>(Context.Material, TextureIndex);
	}
	else if (!Context.MaterialRenderProxy || !Context.MaterialRenderProxy->GetTextureValue(ParameterInfo, &OutValue, Context))
	{
		UTexture* Value = nullptr;

		UMaterialInterface* Interface = Context.Material.GetMaterialInterface();
		if (!Interface || !Interface->GetTextureParameterDefaultValue(ParameterInfo, Value))
		{
			Value = GetIndexedTexture<UTexture>(Context.Material, TextureIndex);
		}

		OutValue = Value;
	}
}

static void GetTextureParameterValue(const FHashedMaterialParameterInfo& ParameterInfo, int32 TextureIndex, const FMaterialRenderContext& Context, const URuntimeVirtualTexture*& OutValue)
{
	if (ParameterInfo.Name.IsNone())
	{
		OutValue = GetIndexedTexture<URuntimeVirtualTexture>(Context.Material, TextureIndex);
	}
	else if (!Context.MaterialRenderProxy || !Context.MaterialRenderProxy->GetTextureValue(ParameterInfo, &OutValue, Context))
	{
		URuntimeVirtualTexture* Value = nullptr;

		UMaterialInterface* Interface = Context.Material.GetMaterialInterface();
		if (!Interface || !Interface->GetRuntimeVirtualTextureParameterDefaultValue(ParameterInfo, Value))
		{
			Value = GetIndexedTexture<URuntimeVirtualTexture>(Context.Material, TextureIndex);
		}

		OutValue = Value;
	}
}

namespace
{
	

	template<typename T>
	inline T ReadPreshaderValue(FPreshaderDataContext& RESTRICT Data)
	{
		T Result;
		FMemory::Memcpy(&Result, Data.Ptr, sizeof(T));
		Data.Ptr += sizeof(T);
		checkSlow(Data.Ptr <= Data.EndPtr);
		return Result;
	}

	template<>
	inline uint8 ReadPreshaderValue<uint8>(FPreshaderDataContext& RESTRICT Data)
	{
		checkSlow(Data.Ptr < Data.EndPtr);
		return *Data.Ptr++;
	}

	template<>
	FScriptName ReadPreshaderValue<FScriptName>(FPreshaderDataContext& RESTRICT Data)
	{
		const int32 Index = ReadPreshaderValue<uint16>(Data);
		check(Index >= 0 && Index < Data.NumNames);
		return Data.Names[Index];
	}

	template<>
	FName ReadPreshaderValue<FName>(FPreshaderDataContext& RESTRICT Data) = delete;

	template<>
	FHashedMaterialParameterInfo ReadPreshaderValue<FHashedMaterialParameterInfo>(FPreshaderDataContext& RESTRICT Data)
	{
		const FScriptName Name = ReadPreshaderValue<FScriptName>(Data);
		const int32 Index = ReadPreshaderValue<int32>(Data);
		const TEnumAsByte<EMaterialParameterAssociation> Association = ReadPreshaderValue<TEnumAsByte<EMaterialParameterAssociation>>(Data);
		return FHashedMaterialParameterInfo(Name, Association, Index);
	}
}

static void GetVectorParameter(const FUniformExpressionSet& UniformExpressionSet, uint32 ParameterIndex, const FMaterialRenderContext& Context, FLinearColor& OutValue)
{
	const FMaterialVectorParameterInfo& Parameter = UniformExpressionSet.GetVectorParameter(ParameterIndex);

	OutValue.R = OutValue.G = OutValue.B = OutValue.A = 0;
	bool bNeedsDefaultValue = false;
	if (!Context.MaterialRenderProxy || !Context.MaterialRenderProxy->GetVectorValue(Parameter.ParameterInfo, &OutValue, Context))
	{
		const bool bOveriddenParameterOnly = Parameter.ParameterInfo.Association == EMaterialParameterAssociation::GlobalParameter;

		UMaterialInterface* Interface = Context.Material.GetMaterialInterface();
		if (!Interface || !Interface->GetVectorParameterDefaultValue(Parameter.ParameterInfo, OutValue, bOveriddenParameterOnly))
		{
			bNeedsDefaultValue = true;
		}
	}

	if (bNeedsDefaultValue)
	{
//#if WITH_EDITOR
		//if (!Context.Material.TransientOverrides.GetVectorOverride(Parameter.ParameterInfo, OutValue))
//#endif // WITH_EDITOR
		{
			Parameter.GetDefaultValue(OutValue);
		}
	}
}

static void GetScalarParameter(const FUniformExpressionSet& UniformExpressionSet, uint32 ParameterIndex, const FMaterialRenderContext& Context, FLinearColor& OutValue)
{
	const FMaterialScalarParameterInfo& Parameter = UniformExpressionSet.GetScalarParameter(ParameterIndex);

	OutValue.A = 0;

	bool bNeedsDefaultValue = false;
	if (!Context.MaterialRenderProxy || !Context.MaterialRenderProxy->GetScalarValue(Parameter.ParameterInfo, &OutValue.A, Context))
	{
		const bool bOveriddenParameterOnly = Parameter.ParameterInfo.Association == EMaterialParameterAssociation::GlobalParameter;

		UMaterialInterface* Interface = Context.Material.GetMaterialInterface();
		if (!Interface || !Interface->GetScalarParameterDefaultValue(Parameter.ParameterInfo, OutValue.A, bOveriddenParameterOnly))
		{
			bNeedsDefaultValue = true;
		}
	}

	if (bNeedsDefaultValue)
	{
//#if WITH_EDITOR
		//if (!Context.Material.TransientOverrides.GetScalarOverride(Parameter.ParameterInfo, OutValue.A))
//#endif // WITH_EDITOR
		{
			Parameter.GetDefaultValue(OutValue.A);
		}
	}

	OutValue.R = OutValue.G = OutValue.B = OutValue.A;
}

using FPreshaderStack = TArray<FLinearColor, TInlineAllocator<64u>>;

template<typename Operation>
static inline void EvaluateUnaryOp(FPreshaderStack& Stack, const Operation& Op)
{
	const FLinearColor Value = Stack.Pop(false);
	Stack.Add(FLinearColor(Op(Value.R), Op(Value.G), Op(Value.B), Op(Value.A)));
}

template<typename Operation>
static inline void EvaluateBinaryOp(FPreshaderStack& Stack, const Operation& Op)
{
	const FLinearColor Value1 = Stack.Pop(false);
	const FLinearColor Value0 = Stack.Pop(false);
	Stack.Add(FLinearColor(Op(Value0.R, Value1.R), Op(Value0.G, Value1.G), Op(Value0.B, Value1.B), Op(Value0.A, Value1.A)));
}

template<typename Operation>
static inline void EvaluateTernaryOp(FPreshaderStack& Stack, const Operation& Op)
{
	const FLinearColor Value2 = Stack.Pop(false);
	const FLinearColor Value1 = Stack.Pop(false);
	const FLinearColor Value0 = Stack.Pop(false);
	Stack.Add(FLinearColor(Op(Value0.R, Value1.R, Value2.R), Op(Value0.G, Value1.G, Value2.G), Op(Value0.B, Value1.B, Value2.B), Op(Value0.A, Value1.A, Value2.A)));
}

static void EvaluateDot(FPreshaderStack& Stack, FPreshaderDataContext& RESTRICT Data)
{
	const uint8 ValueType = ReadPreshaderValue<uint8>(Data);
	const FLinearColor Value1 = Stack.Pop(false);
	const FLinearColor Value0 = Stack.Pop(false);
	float Result = Value0.R * Value1.R;
	Result += (ValueType >= MCT_Float2) ? Value0.G * Value1.G : 0;
	Result += (ValueType >= MCT_Float3) ? Value0.B * Value1.B : 0;
	Result += (ValueType >= MCT_Float4) ? Value0.A * Value1.A : 0;
	Stack.Add(FLinearColor(Result, Result, Result, Result));
}

static void EvaluateCross(FPreshaderStack& Stack, FPreshaderDataContext& RESTRICT Data)
{
	const uint8 ValueType = ReadPreshaderValue<uint8>(Data);
	FLinearColor ValueB = Stack.Pop(false);
	FLinearColor ValueA = Stack.Pop(false);
	
	// Must be Float3, replicate CoerceParameter behavior
	switch (ValueType)
	{
	case MCT_Float:
		ValueA.B = ValueA.G = ValueA.R;
		ValueB.B = ValueB.G = ValueB.R;
		break;
	case MCT_Float1:
		ValueA.B = ValueA.G = 0.f;
		ValueB.B = ValueB.G = 0.f;
		break;
	case MCT_Float2:
		ValueA.B = 0.f;
		ValueB.B = 0.f;
		break;
	};

	const FVector Cross = FVector::CrossProduct(FVector(ValueA), FVector(ValueB));
	Stack.Add(FLinearColor(Cross.X, Cross.Y, Cross.Z, 0.0f));
}

static void EvaluateComponentSwizzle(FPreshaderStack& Stack, FPreshaderDataContext& RESTRICT Data)
{
	const uint8 NumElements = ReadPreshaderValue<uint8>(Data);
	const uint8 IndexR = ReadPreshaderValue<uint8>(Data);
	const uint8 IndexG = ReadPreshaderValue<uint8>(Data);
	const uint8 IndexB = ReadPreshaderValue<uint8>(Data);
	const uint8 IndexA = ReadPreshaderValue<uint8>(Data);

	FLinearColor Value = Stack.Pop(false);
	FLinearColor Result(0.0f, 0.0f, 0.0f, 0.0f);
	switch (NumElements)
	{
	case 1:
		// Replicate scalar
		Result.R = Result.G = Result.B = Result.A = Value.Component(IndexR);
		break;
	case 4:
		Result.A = Value.Component(IndexA);
		// Fallthrough...
	case 3:
		Result.B = Value.Component(IndexB);
		// Fallthrough...
	case 2:
		Result.G = Value.Component(IndexG);
		Result.R = Value.Component(IndexR);
		break;
	default:
		UE_LOG(LogMaterial, Fatal, TEXT("Invalid number of swizzle elements: %d"), NumElements);
		break;
	}
	Stack.Add(Result);
}

static void EvaluateAppenedVector(FPreshaderStack& Stack, FPreshaderDataContext& RESTRICT Data)
{
	const uint8 NumComponentsA = ReadPreshaderValue<uint8>(Data);

	const FLinearColor ValueB = Stack.Pop(false);
	const FLinearColor ValueA = Stack.Pop(false);

	FLinearColor Result;
	Result.R = NumComponentsA >= 1 ? ValueA.R : (&ValueB.R)[0 - NumComponentsA];
	Result.G = NumComponentsA >= 2 ? ValueA.G : (&ValueB.R)[1 - NumComponentsA];
	Result.B = NumComponentsA >= 3 ? ValueA.B : (&ValueB.R)[2 - NumComponentsA];
	Result.A = NumComponentsA >= 4 ? ValueA.A : (&ValueB.R)[3 - NumComponentsA];
	Stack.Add(Result);
}

static const UTexture* GetTextureParameter(const FMaterialRenderContext& Context, FPreshaderDataContext& RESTRICT Data)
{
	const FHashedMaterialParameterInfo ParameterInfo = ReadPreshaderValue<FHashedMaterialParameterInfo>(Data);
	const int32 TextureIndex = ReadPreshaderValue<int32>(Data);
	
	const UTexture* Texture = nullptr;
	GetTextureParameterValue(ParameterInfo, TextureIndex, Context, Texture);
	return Texture;
}

static void EvaluateTextureSize(const FMaterialRenderContext& Context, FPreshaderStack& Stack, FPreshaderDataContext& RESTRICT Data)
{
	const UTexture* Texture = GetTextureParameter(Context, Data);
	if (Texture)
	{
		const uint32 SizeX = Texture->Resource->GetSizeX();
		const uint32 SizeY = Texture->Resource->GetSizeY();
		const uint32 SizeZ = Texture->Resource->GetSizeZ();
		Stack.Add(FLinearColor((float)SizeX, (float)SizeY, (float)SizeZ, 0.0f));
	}
}

static void EvaluateTexelSize(const FMaterialRenderContext& Context, FPreshaderStack& Stack, FPreshaderDataContext& RESTRICT Data)
{
	const UTexture* Texture = GetTextureParameter(Context, Data);
	if (Texture)
	{
		const uint32 SizeX = Texture->Resource->GetSizeX();
		const uint32 SizeY = Texture->Resource->GetSizeY();
		const uint32 SizeZ = Texture->Resource->GetSizeZ();
		Stack.Add(FLinearColor(1.0f / (float)SizeX, 1.0f / (float)SizeY, (SizeZ > 0 ? 1.0f / (float)SizeZ : 0.0f), 0.0f));
	}
}

static FGuid GetExternalTextureGuid(const FMaterialRenderContext& Context, FPreshaderDataContext& RESTRICT Data)
{
	const FScriptName ParameterName = ReadPreshaderValue<FScriptName>(Data);
	const FGuid ExternalTextureGuid = ReadPreshaderValue<FGuid>(Data);
	const int32 TextureIndex = ReadPreshaderValue<int32>(Data);
	return GetExternalTextureGuid(Context, ExternalTextureGuid, ScriptNameToName(ParameterName), TextureIndex);
}

static void EvaluateExternalTextureCoordinateScaleRotation(const FMaterialRenderContext& Context, FPreshaderStack& Stack, FPreshaderDataContext& RESTRICT Data)
{
	const FGuid GuidToLookup = GetExternalTextureGuid(Context, Data);
	FLinearColor Result(1.f, 0.f, 0.f, 1.f);
	if (GuidToLookup.IsValid())
	{
		FExternalTextureRegistry::Get().GetExternalTextureCoordinateScaleRotation(GuidToLookup, Result);
	}
	Stack.Add(Result);
}

static void EvaluateExternalTextureCoordinateOffset(const FMaterialRenderContext& Context, FPreshaderStack& Stack, FPreshaderDataContext& RESTRICT Data)
{
	const FGuid GuidToLookup = GetExternalTextureGuid(Context, Data);
	FLinearColor Result(0.f, 0.f, 0.f, 0.f);
	if (GuidToLookup.IsValid())
	{
		FExternalTextureRegistry::Get().GetExternalTextureCoordinateOffset(GuidToLookup, Result);
	}
	Stack.Add(Result);
}

static void EvaluateRuntimeVirtualTextureUniform(const FMaterialRenderContext& Context, FPreshaderStack& Stack, FPreshaderDataContext& RESTRICT Data)
{
	const FHashedMaterialParameterInfo ParameterInfo = ReadPreshaderValue<FHashedMaterialParameterInfo>(Data);
	const int32 TextureIndex = ReadPreshaderValue<int32>(Data);
	const int32 VectorIndex = ReadPreshaderValue<int32>(Data);

	const URuntimeVirtualTexture* Texture = nullptr;
	if (ParameterInfo.Name.IsNone() || !Context.MaterialRenderProxy || !Context.MaterialRenderProxy->GetTextureValue(ParameterInfo, &Texture, Context))
	{
		Texture = GetIndexedTexture<URuntimeVirtualTexture>(Context.Material, TextureIndex);
	}
	if (Texture != nullptr && VectorIndex != INDEX_NONE)
	{
		Stack.Add(FLinearColor(Texture->GetUniformParameter(VectorIndex)));
	}
	else
	{
		Stack.Add(FLinearColor(0.f, 0.f, 0.f, 0.f));
	}
}

/** Converts an arbitrary number into a safe divisor. i.e. FMath::Abs(Number) >= DELTA */
static float GetSafeDivisor(float Number)
{
	if (FMath::Abs(Number) < DELTA)
	{
		if (Number < 0.0f)
		{
			return -DELTA;
		}
		else
		{
			return +DELTA;
		}
	}
	else
	{
		return Number;
	}
}

/**
 * FORCENOINLINE is required to discourage compiler from vectorizing the Div operation, which may tempt it into optimizing divide as A * rcp(B)
 * This will break shaders that are depending on exact divide results (see SubUV material function)
 * Technically this could still happen for a scalar divide, but it doesn't seem to occur in practice
 */
FORCENOINLINE static float DivideComponent(float A, float B)
{
	return A / GetSafeDivisor(B);
}

void EvaluatePreshader2(const FUniformExpressionSet* UniformExpressionSet, const FMaterialRenderContext& Context, FPreshaderStack& Stack, FPreshaderDataContext& RESTRICT Data, FLinearColor& OutValue,
						uint16 & LastVectorIndex, uint16 & LastScalarIndex, int & NumOpCodes )
{
	static const float LogToLog10 = 1.0f / FMath::Loge(10.f);
	uint8 const* const DataEnd = Data.EndPtr;

	LastVectorIndex = ( uint16 )-1;
	LastScalarIndex = (uint16)-1;
	NumOpCodes = 0;

	Stack.Reset();
	while (Data.Ptr < DataEnd)
	{
		const EMaterialPreshaderOpcode Opcode = (EMaterialPreshaderOpcode)ReadPreshaderValue<uint8>(Data);
		NumOpCodes++;
		switch (Opcode)
		{
		case EMaterialPreshaderOpcode::ConstantZero:
			Stack.Add(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
			break;
		case EMaterialPreshaderOpcode::Constant:
			Stack.Add(ReadPreshaderValue<FLinearColor>(Data));
			break;
		case EMaterialPreshaderOpcode::VectorParameter:
			check(UniformExpressionSet);
			LastVectorIndex = ReadPreshaderValue<uint16>( Data );
			GetVectorParameter(*UniformExpressionSet, LastVectorIndex, Context, Stack.AddDefaulted_GetRef());
			break;
		case EMaterialPreshaderOpcode::ScalarParameter:
			check(UniformExpressionSet);
			LastScalarIndex = ReadPreshaderValue<uint16>( Data );
			GetScalarParameter(*UniformExpressionSet, LastScalarIndex, Context, Stack.AddDefaulted_GetRef());
			break;
		case EMaterialPreshaderOpcode::Add: EvaluateBinaryOp(Stack, [](float Lhs, float Rhs) { return Lhs + Rhs; }); break;
		case EMaterialPreshaderOpcode::Sub: EvaluateBinaryOp(Stack, [](float Lhs, float Rhs) { return Lhs - Rhs; }); break;
		case EMaterialPreshaderOpcode::Mul: EvaluateBinaryOp(Stack, [](float Lhs, float Rhs) { return Lhs * Rhs; }); break;
		case EMaterialPreshaderOpcode::Div: EvaluateBinaryOp(Stack, [](float Lhs, float Rhs) { return DivideComponent(Lhs, Rhs); }); break;
		case EMaterialPreshaderOpcode::Fmod: EvaluateBinaryOp(Stack, [](float Lhs, float Rhs) { return FMath::Fmod(Lhs, Rhs); }); break;
		case EMaterialPreshaderOpcode::Min: EvaluateBinaryOp(Stack, [](float Lhs, float Rhs) { return FMath::Min(Lhs, Rhs); }); break;
		case EMaterialPreshaderOpcode::Max: EvaluateBinaryOp(Stack, [](float Lhs, float Rhs) { return FMath::Max(Lhs, Rhs); }); break;
		case EMaterialPreshaderOpcode::Clamp: EvaluateTernaryOp(Stack, [](float A, float B, float C) { return FMath::Clamp(A, B, C); }); break;
		case EMaterialPreshaderOpcode::Dot: EvaluateDot(Stack, Data); break;
		case EMaterialPreshaderOpcode::Cross: EvaluateCross(Stack, Data); break;
		case EMaterialPreshaderOpcode::Sqrt: EvaluateUnaryOp(Stack, [](float V) { return FMath::Sqrt(V); }); break;
		case EMaterialPreshaderOpcode::Sin: EvaluateUnaryOp(Stack, [](float V) { return FMath::Sin(V); }); break;
		case EMaterialPreshaderOpcode::Cos: EvaluateUnaryOp(Stack, [](float V) { return FMath::Cos(V); }); break;
		case EMaterialPreshaderOpcode::Tan: EvaluateUnaryOp(Stack, [](float V) { return FMath::Tan(V); }); break;
		case EMaterialPreshaderOpcode::Asin: EvaluateUnaryOp(Stack, [](float V) { return FMath::Asin(V); }); break;
		case EMaterialPreshaderOpcode::Acos: EvaluateUnaryOp(Stack, [](float V) { return FMath::Acos(V); }); break;
		case EMaterialPreshaderOpcode::Atan: EvaluateUnaryOp(Stack, [](float V) { return FMath::Atan(V); }); break;
		case EMaterialPreshaderOpcode::Atan2: EvaluateBinaryOp(Stack, [](float A, float B) { return FMath::Atan2(A, B); }); break;
		case EMaterialPreshaderOpcode::Abs: EvaluateUnaryOp(Stack, [](float V) { return FMath::Abs(V); }); break;
		case EMaterialPreshaderOpcode::Saturate: EvaluateUnaryOp(Stack, [](float V) { return FMath::Clamp(V, 0.0f, 1.0f); }); break;
		case EMaterialPreshaderOpcode::Floor: EvaluateUnaryOp(Stack, [](float V) { return FMath::FloorToFloat(V); }); break;
		case EMaterialPreshaderOpcode::Ceil: EvaluateUnaryOp(Stack, [](float V) { return FMath::CeilToFloat(V); }); break;
		case EMaterialPreshaderOpcode::Round: EvaluateUnaryOp(Stack, [](float V) { return FMath::RoundToFloat(V); }); break;
		case EMaterialPreshaderOpcode::Trunc: EvaluateUnaryOp(Stack, [](float V) { return FMath::TruncToFloat(V); }); break;
		case EMaterialPreshaderOpcode::Sign: EvaluateUnaryOp(Stack, [](float V) { return FMath::Sign(V); }); break;
		case EMaterialPreshaderOpcode::Frac: EvaluateUnaryOp(Stack, [](float V) { return FMath::Frac(V); }); break;
		case EMaterialPreshaderOpcode::Fractional: EvaluateUnaryOp(Stack, [](float V) { return FMath::Fractional(V); }); break;
		case EMaterialPreshaderOpcode::Log2: EvaluateUnaryOp(Stack, [](float V) { return FMath::Log2(V); }); break;
		case EMaterialPreshaderOpcode::Log10: EvaluateUnaryOp(Stack, [](float V) { return FMath::Loge(V) * LogToLog10; }); break;
		case EMaterialPreshaderOpcode::ComponentSwizzle: EvaluateComponentSwizzle(Stack, Data); break;
		case EMaterialPreshaderOpcode::AppendVector: EvaluateAppenedVector(Stack, Data); break;
		case EMaterialPreshaderOpcode::TextureSize: EvaluateTextureSize(Context, Stack, Data); break;
		case EMaterialPreshaderOpcode::TexelSize: EvaluateTexelSize(Context, Stack, Data); break;
		case EMaterialPreshaderOpcode::ExternalTextureCoordinateScaleRotation: EvaluateExternalTextureCoordinateScaleRotation(Context, Stack, Data); break;
		case EMaterialPreshaderOpcode::ExternalTextureCoordinateOffset: EvaluateExternalTextureCoordinateOffset(Context, Stack, Data); break;
		case EMaterialPreshaderOpcode::RuntimeVirtualTextureUniform: EvaluateRuntimeVirtualTextureUniform(Context, Stack, Data); break;
		default:
			UE_LOG(LogMaterial, Fatal, TEXT("Unknown preshader opcode %d"), (uint8)Opcode);
			break;
		}
	}
	check(Data.Ptr == DataEnd);

	ensure(Stack.Num() <= 1);
	if (Stack.Num() > 0)
	{
		OutValue = Stack.Last();
		
	}
}

#endif