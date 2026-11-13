#include "PageFault.h"

#include "DynamicRHI.h"
#include "ID3D12DynamicRHI.h"
#include "RenderingThread.h"
#include "RHICommandList.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include "d3d12.h"
#include "Windows/HideWindowsPlatformTypes.h"

// GTouchUnmappedMemory: signed DXIL of TouchUnmappedMemory.hlsl, with its root signature.
#include "TouchUnmappedMemory.h"

namespace
{
// Candidates for an unmapped GPU address, in order: the first unmapped one faults, so the
// later dispatches only matter when a driver maps an earlier address. Low addresses can be
// mapped: 0x10000 did not fault on an RTX 3080, 0x10000000 did.
constexpr D3D12_GPU_VIRTUAL_ADDRESS UnmappedAddresses[] = {
	0x10000000ull,      // 256 MiB
	0x100000000ull,     // 4 GiB
	0x10000000000ull,   // 1 TiB
	0x7F0000000000ull,  // 127 TiB, near the top of a 47-bit address space
};
}

void FPageFaultInterface::Dispatch()
{
	if (!GDynamicRHI || GDynamicRHI->GetInterfaceType() != ERHIInterfaceType::D3D12)
	{
		UE_LOG(LogTemp, Error, TEXT("GPU page fault: needs the D3D12 RHI"));
		return;
	}

	ENQUEUE_RENDER_COMMAND(PageFault)([](FRHICommandListImmediate& RHICmdList)
	{
		// Raw D3D12, because the RHI gives every UAV a size and bounds-checks it.
		RHICmdList.EnqueueLambda(TEXT("PageFault"), [](FRHICommandListBase& ExecutingCmdList)
		{
			ID3D12DynamicRHI* RHI = GetID3D12DynamicRHI();
			ID3D12Device* Device = RHI->RHIGetDevice(0);

			static TRefCountPtr<ID3D12RootSignature> RootSignature;
			static TRefCountPtr<ID3D12PipelineState> Pipeline;
			HRESULT Result = Device->CreateRootSignature(0, GTouchUnmappedMemory, sizeof(GTouchUnmappedMemory), IID_PPV_ARGS(RootSignature.GetInitReference()));
			if (FAILED(Result))
			{
				UE_LOG(LogTemp, Error, TEXT("GPU page fault: CreateRootSignature failed (0x%08x)"), Result);
				return;
			}
			D3D12_COMPUTE_PIPELINE_STATE_DESC Desc = {};
			Desc.pRootSignature = RootSignature.GetReference();
			Desc.CS = { GTouchUnmappedMemory, sizeof(GTouchUnmappedMemory) };
			Result = Device->CreateComputePipelineState(&Desc, IID_PPV_ARGS(Pipeline.GetInitReference()));
			if (FAILED(Result))
			{
				UE_LOG(LogTemp, Error, TEXT("GPU page fault: CreateComputePipelineState failed (0x%08x)"), Result);
				return;
			}

			ID3D12GraphicsCommandList* CommandList = RHI->RHIGetGraphicsCommandList(ExecutingCmdList, 0);
			CommandList->SetComputeRootSignature(RootSignature.GetReference());
			CommandList->SetPipelineState(Pipeline.GetReference());
			for (const D3D12_GPU_VIRTUAL_ADDRESS Address : UnmappedAddresses)
			{
				CommandList->SetComputeRootUnorderedAccessView(0, Address);
				CommandList->Dispatch(1, 1, 1);
			}
			// Gives the command list back to the RHI with its own root signature and heaps.
			RHI->RHIFinishExternalComputeWork(ExecutingCmdList, 0, CommandList);
			UE_LOG(LogTemp, Warning, TEXT("GPU page fault: dispatched TouchUnmappedMemory"));
		});
	});
}
