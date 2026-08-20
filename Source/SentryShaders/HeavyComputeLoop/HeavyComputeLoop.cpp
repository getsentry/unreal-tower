#include "HeavyComputeLoop.h"
#include "PixelShaderUtils.h"
#include "MeshPassProcessor.inl"
#include "StaticMeshResources.h"
#include "DynamicMeshBuilder.h"
#include "RenderGraphResources.h"
#include "GlobalShader.h"
#include "UnifiedBuffer.h"
#include "CanvasTypes.h"
#include "MeshDrawShaderBindings.h"
#include "RHIGPUReadback.h"
#include "MeshPassUtils.h"
#include "MaterialShader.h"

DECLARE_STATS_GROUP(TEXT("HeavyComputeLoop"), STATGROUP_HeavyComputeLoop, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("HeavyComputeLoop Execute"), STAT_HeavyComputeLoop_Execute, STATGROUP_HeavyComputeLoop);

// This class carries our parameter declarations and acts as the bridge between cpp and HLSL.
class SENTRYSHADERS_API FHeavyComputeLoop: public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FHeavyComputeLoop);
	SHADER_USE_PARAMETER_STRUCT(FHeavyComputeLoop, FGlobalShader);

	class FHeavyComputeLoop_Perm_TEST : SHADER_PERMUTATION_INT("TEST", 1);
	using FPermutationDomain = TShaderPermutationDomain<
		FHeavyComputeLoop_Perm_TEST
	>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		/*
		* Here's where you define one or more of the input parameters for your shader.
		* Some examples:
		*/
		// SHADER_PARAMETER(uint32, MyUint32) // On the shader side: uint32 MyUint32;
		// SHADER_PARAMETER(FVector3f, MyVector) // On the shader side: float3 MyVector;

		// SHADER_PARAMETER_TEXTURE(Texture2D, MyTexture) // On the shader side: Texture2D<float4> MyTexture; (float4 should be whatever you expect each pixel in the texture to be, in this case float4(R,G,B,A) for 4 channels)
		// SHADER_PARAMETER_SAMPLER(SamplerState, MyTextureSampler) // On the shader side: SamplerState MySampler; // CPP side: TStaticSamplerState<ESamplerFilter::SF_Bilinear>::GetRHI();

		// SHADER_PARAMETER_ARRAY(float, MyFloatArray, [3]) // On the shader side: float MyFloatArray[3];

		// SHADER_PARAMETER_UAV(RWTexture2D<FVector4f>, MyTextureUAV) // On the shader side: RWTexture2D<float4> MyTextureUAV;
		// SHADER_PARAMETER_UAV(RWStructuredBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: RWStructuredBuffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_UAV(RWBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: RWBuffer<FMyCustomStruct> MyCustomStructs;

		// SHADER_PARAMETER_SRV(StructuredBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: StructuredBuffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_SRV(Buffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: Buffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_SRV(Texture2D<FVector4f>, MyReadOnlyTexture) // On the shader side: Texture2D<float4> MyReadOnlyTexture;

		// SHADER_PARAMETER_STRUCT_REF(FMyCustomStruct, MyCustomStruct)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, PageFaultUAV)

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);

		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		const FPermutationDomain PermutationVector(Parameters.PermutationId);

		/*
		* Here you define constants that can be used statically in the shader code.
		* Example:
		*/
		// OutEnvironment.SetDefine(TEXT("MY_CUSTOM_CONST"), TEXT("1"));

		/*
		* These defines are used in the thread count section of our shader
		*/
		OutEnvironment.SetDefine(TEXT("THREADS_X"), NUM_THREADS_HeavyComputeLoop_X);
		OutEnvironment.SetDefine(TEXT("THREADS_Y"), NUM_THREADS_HeavyComputeLoop_Y);
		OutEnvironment.SetDefine(TEXT("THREADS_Z"), NUM_THREADS_HeavyComputeLoop_Z);

		// This shader must support typed UAV load and we are testing if it is supported at runtime using RHIIsTypedUAVLoadSupported
		//OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);

		// FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}
private:
};

// This will tell the engine to create the shader and where the shader entry point is.
//                            ShaderType                            ShaderPath                     Shader function name    Type
IMPLEMENT_GLOBAL_SHADER(FHeavyComputeLoop, "/SentryShaders/HeavyComputeLoop/HeavyComputeLoop.usf", "HeavyComputeLoop", SF_Compute);

void FHeavyComputeLoopInterface::DispatchRenderThread(FRHICommandListImmediate& RHICmdList, FHeavyComputeLoopDispatchParams Params, TFunction<void(int OutputVal)> AsyncCallback) {
	FRDGBuilder GraphBuilder(RHICmdList);

	{
		SCOPE_CYCLE_COUNTER(STAT_HeavyComputeLoop_Execute);
		DECLARE_GPU_STAT(HeavyComputeLoop);
		RDG_EVENT_SCOPE(GraphBuilder, "HeavyComputeLoop");

		typename FHeavyComputeLoop::FPermutationDomain PermutationVector;

		// Add any static permutation options here
		// PermutationVector.Set<FHeavyComputeLoop::FMyPermutationName>(12345);

		TShaderMapRef<FHeavyComputeLoop> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);

		bool bIsShaderValid = ComputeShader.IsValid();

		if (bIsShaderValid) {
			FHeavyComputeLoop::FParameters* PassParameters = GraphBuilder.AllocParameters<FHeavyComputeLoop::FParameters>();

			// Texture backed by an invalid GPU address; any UAV access page-faults the GPU
			// and removes the device. Dispatched on the direct (graphics) queue so the crash
			// surfaces as device-removed rather than an async-compute hang.
			ETextureCreateFlags TexFlags = TexCreate_UAV | TexCreate_ShaderResource | TexCreate_NoFastClear | TexCreate_RenderTargetable | TexCreate_Invalid;
			FRDGTextureDesc CreateInfo = FRDGTextureDesc::Create2D(
				FIntPoint(64, 64),
				PF_R32_UINT,
				FClearValueBinding::None,
				TexFlags);

			FRDGTextureRef PageFaultTexture = GraphBuilder.CreateTexture(CreateInfo, TEXT("HeavyComputeLoop.PageFaultUAV"));
			PassParameters->PageFaultUAV = GraphBuilder.CreateUAV(PageFaultTexture);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ExecuteHeavyComputeLoop"),
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
				ComputeShader,
				PassParameters,
				FIntVector(1, 1, 1));
		} else {
			#if WITH_EDITOR
				GEngine->AddOnScreenDebugMessage((uint64)42145125184, 6.f, FColor::Red, FString(TEXT("The compute shader has a problem.")));
			#endif

			// We exit here as we don't want to crash the game if the shader is not found or has an error.
		}
	}

	GraphBuilder.Execute();
}