//���m�N�����H�����s���R���s���[�g�V�F�[�_
Texture2D<float4> srcImg : register(t0);
RWTexture2D<float4> dstImg : register(u0);

[numthreads(16,16,1)]
void MonoCS( uint3 dtid : SV_DispatchThreadID)
{
    if (dtid.x < 1280 && dtid.y < 720)
    {
        //https://ja.wikipedia.org/wiki/YUV ���
        float b = dot(srcImg[dtid.xy].rgb, float3(0.299,0.587,0.114));
        b=pow(saturate(b),1.0/2.2);
        dstImg[dtid.xy] = float4(b,b,b, 1);
    }
}
