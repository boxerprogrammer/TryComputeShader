//モノクロ加工等を行うコンピュートシェーダ
Texture2D<float4> srcImg : register(t0);
RWTexture2D<float4> dstImg : register(u0);

[numthreads(4,4,1)]
void MonoCS( uint3 dtid : SV_DispatchThreadID)
{
    //サンプルのためテクスチャサイズは決め打ちしています
    if (dtid.x < 200 && dtid.y < 200)
    {
        //https://ja.wikipedia.org/wiki/YUV より
        float b = dot(srcImg[dtid.xy].rgb, float3(0.299,0.587,0.114));
        b=pow(saturate(b),1.0/2.2);
        dstImg[dtid.xy] = float4(b,b,b, 1);
    }
}
