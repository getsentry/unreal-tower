#pragma once

#include "CoreMinimal.h"

// Makes a compute shader access unmapped GPU memory, so the GPU reports a page fault
// in that shader. D3D12 only; the shader is TouchUnmappedMemory.hlsl.
class SENTRYSHADERS_API FPageFaultInterface
{
public:
	// Can be called from any thread.
	static void Dispatch();
};
