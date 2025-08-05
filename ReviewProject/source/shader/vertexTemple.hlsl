
//将纹理资源绑定到纹理寄存器槽0
Texture2D gDiffuseMap : register(t0);

//把下列采样器资源依次绑定到采样器寄存器槽0~5
SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

//将常量缓冲区资源cbuffer绑定到常量缓冲区寄存器槽0
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gView;
};

cbuffer cbMaterial : register(b1)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelRo;
};