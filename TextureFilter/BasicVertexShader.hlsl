#include"BasicType.hlsli"
BasicType BasicVS(float4 pos : POSITION,float2 uv:TEXCOORD) {
	BasicType output;//�s�N�Z���V�F�[�_�֓n���l
	output.svpos = pos;
	output.uv = uv;
	return output;
}
