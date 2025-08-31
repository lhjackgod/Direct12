struct Light
{
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position;
    float SpotPower;
};

struct Material
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;

    float Shininess;
};

//对于每个以MaxLights为光源数量最大轴的对象来说，索引[0, NUM_DIR_LIGHTS)表示的是方向光源
// [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS)表示的是点光源
// [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS+NUM_SPOT_LIGHTS] //表示聚光灯
#define MaxLights 16

float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
    //线性衰变
    // 0 ~ 1
    return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
    float cosTheta = saturate(dot(normal, lightVec));
    return R0 + (1.0f - R0) * pow(1.0f - cosTheta, 5.0f);
}

float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    //m由光泽度推导而来，而光泽度则根据粗糙度求得
    const float m = mat.Shininess * 256.0f;
    float3 halfVec = normalize(toEye + lightVec);
    float ks = pow(max(dot(halfVec, normal), 0.0f), m);
#ifdef CARTON
    if (ks <= 0.1f)
    {
        ks = 0.0f;
    }
    else if (ks <= 0.8f)
    {
        ks = 0.5f;
    }
    else if (ks <= 1.0f)
    {
        ks = 0.8f;
    }
#endif
    float roughnessFactor = (m + 8.0f) *  ks / 8.0f;
    float3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec);

    // 尽管我们进行的是LDR(low dynamic range, 低动态范围)渲染，但spec(镜面反射)公式得到的结果
    //仍会超出范围[0, 1]，因此现将其按比例缩小一些
    float3 specularAlbedo = fresnelFactor * roughnessFactor / (fresnelFactor * roughnessFactor + 1.0f);

    return  lightStrength * (mat.DiffuseAlbedo.rgb + specularAlbedo);
}

float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye)
{
    float3 lightVec = -L.Direction;
    float kd = max(0.0f, dot(normal, lightVec));
#ifdef CARTON
    if (kd <= 0.0f)
    {
        kd = 0.4f;
    }
    else if (kd <= 0.5f)
    {
        kd = 0.6f;
    }
    else if (kd <= 1.0f)
    {
        kd = 1.0f;
    }
#endif
    float3 lightStrength = L.Strength * kd;
    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);
    if (d > L.FalloffEnd) return 0.0f;
    lightVec /= d;
    float ndotI = max(dot(normal, lightVec), 0.0f);
    float3 lightStrength = L.Strength * ndotI;
    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;
    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

float3 ComputeSpotLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    //从表面指向光源的向量
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);
    if (d > L.FalloffEnd) return 0.0f;
    lightVec /= d;
    float nDotI = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * nDotI;
    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;

    float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);
    lightStrength *= spotFactor;
    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

float4 ComputeLighting(Light gLights[MaxLights], Material mat, float3 pos, float3 normal, float3 toEye, float3 shadowFactor)
{
    float3 result = 0.0f;
    int i = 0;
    
#if (NUM_DIR_LIGHTS > 0)
    for (int i = 0; i < NUM_DIR_LIGHTS; i++)
    {
        result += shadowFactor * ComputeDirectionalLight(gLights[i], mat, normal, toEye);
    }
#endif
#if (NUM_POINT_LIGHTS > 0)
    for (int i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS+NUM_POINT_LIGHTS;i++)
    {
        result += ComputePointLight(gLights[i], mat, pos, normal, toEye);
    }
#endif
#if (NUM_SPOT_LIGHTS > 0)
    for (int i = NUM_DIR_LIGHTS+NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS+NUM_POINT_LIGHTS+NUM_SPOT_LIGHTS; i++)
    {
        result += ComputeSpotLight(gLights[i], mat, pos, normal, toEye);
    }
#endif
    return float4(result, 1.0f);
}