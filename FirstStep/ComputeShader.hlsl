

//CPU側のデータ型と合わせる必要がある。
struct SimpleBuffer_t
{
	int		i;
	float	f;
};

StructuredBuffer<SimpleBuffer_t>	inBuff0 : register(t0);
StructuredBuffer<SimpleBuffer_t>	inBuff1 : register(t1);

struct Buffer_t {
	float grpId;
	float grpThrdId;
	float dspThrdId;//ひとまずこれだけ使用する
	uint groupIndex;
};
RWStructuredBuffer<Buffer_t> outBuff:register(u0);
//idを書き込むだけのCS
[numthreads(4, 4, 4)]
void main(uint3 dtid : SV_DispatchThreadID) {
	//ただただIDを代入
	outBuff[dtid.x * 8 * 8 + dtid.y * 8 + dtid.z].dspThrdId = dtid.x * 8 * 8 + dtid.y * 8 + dtid.z;
}

	//uint3 groupId : SV_GroupID,
		//uint3 groupThreadId : SV_GroupThreadID,
	//uint groupIndex : SV_GroupIndex
	/* *inBuff0[groupIndex].f;
	
	
	outBuff[groupId.x * 2 * 2 + groupId.y * 2 + groupId.z].grpId = groupId.x * 2 * 2 + groupId.y * 2 + groupId.z;


	float angle = 3.14f * (float)groupIndex / 32.0f;
	outBuff[groupThreadId.x * 2 * 2 + groupThreadId.y * 2 + groupThreadId.z].grpThrdId =
		groupThreadId.x * 2 * 2 + groupThreadId.y * 2 + (float)groupThreadId.z*cos(angle)+sin(angle);
	outBuff[groupIndex].groupIndex = inBuff0[groupIndex].i*groupIndex;
	*/
//}