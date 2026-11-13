// Reads and writes through a root UAV. A root descriptor is only a GPU address, with no
// size, so there is no bounds check: FPageFaultInterface points it at unmapped memory
// and the first access faults inside this shader.
//
// Compiled offline to TouchUnmappedMemory.h with the Windows SDK DXC, which signs the DXIL:
//   dxc -T cs_6_6 -E TouchUnmappedMemory -Fh TouchUnmappedMemory.h -Vn GTouchUnmappedMemory TouchUnmappedMemory.hlsl
RWByteAddressBuffer Target : register(u0);

[RootSignature("UAV(u0)")]
[numthreads(64, 1, 1)]
void TouchUnmappedMemory(uint3 DispatchThreadId : SV_DispatchThreadID)
{
	const uint Address = DispatchThreadId.x * 4;
	Target.Store(Address, Target.Load(Address) + 1);
}
