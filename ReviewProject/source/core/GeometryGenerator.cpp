#include "D3DUtil.h"
#include "GeometryGenerator.h"
GeometryGenerator::MeshData GeometryGenerator::CreateCylinder(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount)
{
    MeshData meshData;
    // 构建对叠层
    float stackHeight = height / stackCount;

    // 计算从上至下每个相邻分层时所需的半径增量
    float radiusStep = (topRadius - bottomRadius) / stackCount;

    uint32 ringCount = stackCount + 1;
    for (uint32 i = 0; i < ringCount; i++)
    {
        float y = -0.5f * height + i * stackHeight;
        float r = bottomRadius + i * radiusStep;

        //环上各个顶点
        float dTheta = 2.0f * DirectX::XM_PI / sliceCount;
        for (uint32 j = 0; j <= sliceCount; j++)
        {
            Vertex vertex;
            float c = cosf(dTheta * j);
            float s = sinf(dTheta * j);

            vertex.Position = DirectX::XMFLOAT3(c * r, y, s * r);
            vertex.TexC.x = (float)j / sliceCount;
            vertex.TexC.y = 1.0f - i / stackCount; //从下面开始的
            vertex.TangentU = DirectX::XMFLOAT3(-s, 0.0f, c); //位置的变化切线为u
            float dr = bottomRadius - topRadius;
            DirectX::XMFLOAT3 bitangent(c * dr, - height, s * r);

            DirectX::XMVECTOR T = DirectX::XMLoadFloat3(&vertex.TangentU);
            DirectX::XMVECTOR B = DirectX::XMLoadFloat3(&bitangent);
            DirectX::XMVECTOR N = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(T ,B));
            DirectX::XMStoreFloat3(&vertex.Normal , N);

            meshData.Vertices.push_back(vertex);
        }
    }
    //+1是希望每环的第一个顶点和最后一个顶点重合，这是因为它们的纹理坐标不相同
    uint32 ringVertexCount = sliceCount + 1;
    for (uint32 i = 0; i < stackCount; i++)
    {
        for (uint32 j = 0; j < sliceCount; j++)
        {
            meshData.Indices32.push_back(i * ringVertexCount + j);
            meshData.Indices32.push_back((i + 1) * ringVertexCount + j);
            meshData.Indices32.push_back((i + 1) * ringVertexCount + j + 1);

            meshData.Indices32.push_back(i * ringVertexCount + j);
            meshData.Indices32.push_back((i + 1) * ringVertexCount + j + 1);
            meshData.Indices32.push_back(i * ringVertexCount + j + 1);
        }
    }
    BuildCyclinderTopCap(bottomRadius, topRadius, height, sliceCount, stackCount, meshData);
    BuildCyclinderBottomCap(bottomRadius, topRadius, height, sliceCount, stackCount, meshData);
    return meshData;
}

void GeometryGenerator::BuildCyclinderTopCap(float bottomRadius, float topRadius, float height, uint32 sliceCount,
    uint32 stackCount, MeshData& meshData)
{
    uint32 baseIndex = stackCount * (sliceCount + 1);

    meshData.Vertices.push_back(Vertex(0.0f, 0.5f * height, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.0f));

    uint32 centerIndex = (uint32)meshData.Vertices.size() - 1;
    for (uint32 i = 0; i < sliceCount; i++)
    {
        meshData.Indices32.push_back(centerIndex);
        meshData.Indices32.push_back(baseIndex + i + 1);
        meshData.Indices32.push_back(baseIndex + i);
    }
}

void GeometryGenerator::BuildCyclinderBottomCap(float bottomRadius, float topRadius, float height, uint32 sliceCount,
    uint32 stackCount, MeshData& meshData)
{
    uint32 baseIndex = 0;

    meshData.Vertices.push_back(Vertex(0.0f, 0.5f * height, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 1.0f));

    uint32 centerIndex = (uint32)meshData.Vertices.size() - 1;
    for (uint32 i = 0; i < sliceCount; i++)
    {
        meshData.Indices32.push_back(centerIndex);
        meshData.Indices32.push_back(baseIndex + i + 1);
        meshData.Indices32.push_back(baseIndex + i);
    }
}

GeometryGenerator::MeshData GeometryGenerator::CreateSphere(float radius, uint32 sliceCount, uint32 stackCount)
{
    if (stackCount % 2 != 0)
    {
        assert("stackCount must be odd");
    }
    MeshData meshData;
    float Height = radius * 2.0f;
    float stackHeight = Height / (float)((stackCount + 2)); // 到上下顶点的高度
    uint32 ringCount = stackCount + 3;
    uint32 maxRadiusRingIndex = (stackCount / 2) + 1; //最大的r的环的下标
    for (uint32 i = 0; i < ringCount; i++)
    {
        float y = i * stackHeight -0.5f * Height;
        if (i == 0 || i == ringCount - 1)
        {
            meshData.Vertices.push_back(Vertex(0.0f, y, 0.0f, 0.0f, (i == 0?-1.0f : 1.0f), 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, (i == 0? 1.0f : 0.0f)));
        }
        else
        {
            float r = std::sqrtf(radius * radius - y * y);
            float dTheta = 2.0f * DirectX::XM_PI / sliceCount;
            for (int j = 0; j <= sliceCount; j++)
            {
                Vertex vertex;
                float c = cosf(j * dTheta);
                float s = sinf(j * dTheta);
                
                vertex.Position = DirectX::XMFLOAT3(c * r, y, s * r);
                vertex.TexC.x = (float) j / (float)sliceCount;
                vertex.TexC.y = 1.0f - (float)i / (float)(ringCount - 1);

                vertex.TangentU = DirectX::XMFLOAT3(-s, 0.0f, c);
                DirectX::XMFLOAT3 bitangent;
                if (i < maxRadiusRingIndex)
                {
                    bitangent = DirectX::XMFLOAT3(-c * r, -Height, -s * r);
                }
                else if (i == maxRadiusRingIndex)
                {
                    bitangent = DirectX::XMFLOAT3(0.0f, -Height, 0.0f);
                }
                else
                {
                    bitangent = DirectX::XMFLOAT3(c * r, -Height, s * r);
                }
                DirectX::XMVECTOR T = DirectX::XMLoadFloat3(&vertex.TangentU);
                DirectX::XMVECTOR B = DirectX::XMLoadFloat3(&bitangent);
                DirectX::XMVECTOR N = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(T, B));

                DirectX::XMStoreFloat3(&vertex.Normal, N);
                meshData.Vertices.push_back(vertex);
            }
        }
    }

    // 先记录侧面
    uint32 ringVertexCount = sliceCount + 1;
    uint32 baseIndex = 1; // 最下面的点
    for (uint32 i = 0; i < stackCount; i++)
    {
        for (int j = 0; j < sliceCount; j++)
        {
            meshData.Indices32.push_back(i * ringVertexCount + j + baseIndex);
            meshData.Indices32.push_back((i + 1) * ringVertexCount + j + baseIndex);
            meshData.Indices32.push_back((i + 1) * ringVertexCount + j + 1 + baseIndex);

            meshData.Indices32.push_back(i * ringVertexCount + j + baseIndex);
            meshData.Indices32.push_back((i + 1) * ringVertexCount + j + 1 + baseIndex);
            meshData.Indices32.push_back(i * ringVertexCount + j + 1 + baseIndex);
        }
    }

    //然后记录顶面和底面
    for (uint32 i = 0; i < sliceCount; i++)
    {
        meshData.Indices32.push_back(i + baseIndex);
        meshData.Indices32.push_back(0);
        meshData.Indices32.push_back(i + baseIndex + 1);
    }
    baseIndex = 1 + stackCount * ringVertexCount; //top ring begin
    uint32 topVertexIndex = (uint32)meshData.Vertices.size() - 1;
    for (uint32 i = 0; i < sliceCount; i++)
    {
        meshData.Indices32.push_back(i + baseIndex);
        meshData.Indices32.push_back(topVertexIndex);
        meshData.Indices32.push_back(i + baseIndex + 1);
    }
    return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreateGeosphere(float radius, uint32 numSubdivisions)
{
    MeshData meshData;
    numSubdivisions = std::min<uint32>(numSubdivisions, 6u);

    const float x = 0.525731f;
    const float z = 0.850651f;
    DirectX::XMFLOAT3 pos[12] =
    {
        DirectX::XMFLOAT3(-x, 0.0f, z), DirectX::XMFLOAT3(x, 0.0f, z),
        DirectX::XMFLOAT3(-x, 0.0f, -z), DirectX::XMFLOAT3(x, 0.0f, -z),
        DirectX::XMFLOAT3(0.0f, z, x), DirectX::XMFLOAT3(0.0f, z, -x),
        DirectX::XMFLOAT3(0.0f, -z, x), DirectX::XMFLOAT3(0.0f, -z, -x),
        DirectX::XMFLOAT3(z, x, 0.0f), DirectX::XMFLOAT3(-z, x, 0.0f),
        DirectX::XMFLOAT3(z, -x, 0.0f), DirectX::XMFLOAT3(-z, x, 0.0f)
    };

    uint32 k[60] = {
        1,4,0, 4,9,0, 4,5,9, 8,5,4, 1,8,4,
        1,10,8, 10,3,8, 8,3,5, 3,2,5, 3,7,2,
        3,10,7, 10,6,7, 6,11,7, 6,0,11, 6,1,0,
        10,1,6, 11,0,9, 2,11,9, 5,2,9, 11,2,7
    };
    meshData.Vertices.resize(12);
    meshData.Indices32.assign(&k[0], &k[60]);
    for (uint32 i = 0; i < 12; i++)
    {
        meshData.Vertices[i].Position = pos[i];
    }
    for (uint32 i = 0; i < numSubdivisions; i++)
    {
        Subdivide(meshData);
    }
    for (uint32 i = 0; i < meshData.Vertices.size(); i++)
    {
        DirectX::XMVECTOR n = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&meshData.Vertices[i].Position));

        DirectX::XMVECTOR p = DirectX::XMVectorMultiply(n, DirectX::XMVectorSet(radius, radius, radius, 0));
        DirectX::XMStoreFloat3(&meshData.Vertices[i].Position, p);
        DirectX::XMStoreFloat3(&meshData.Vertices[i].Normal, n);

        //根据球面坐标推导出纹理坐标
        float theta = atan2f(meshData.Vertices[i].Position.z, meshData.Vertices[i].Position.x);

        if (theta < 0.0f)
            theta += DirectX::XM_2PI;

        float phi = acosf(meshData.Vertices[i].Position.y / radius);
        meshData.Vertices[i].TexC.x = theta / DirectX::XM_2PI;
        meshData.Vertices[i].TexC.y = phi / DirectX::XM_PI;

        meshData.Vertices[i].TangentU.x = -radius * sinf(theta) * sinf(phi);
        meshData.Vertices[i].TangentU.y = 0.0f;
        meshData.Vertices[i].TangentU.z = radius * cosf(theta) * sinf(phi);

        DirectX::XMVECTOR T = DirectX::XMLoadFloat3(&meshData.Vertices[i].TangentU);
        DirectX::XMStoreFloat3(&meshData.Vertices[i].TangentU, DirectX::XMVector3Normalize(T));
    }
    return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreateBox(float width, float height, float depth)
{
    MeshData meshData;
    DirectX::XMFLOAT3 pos[8] = {
        //behine
        DirectX::XMFLOAT3(-width * 0.5f, -height * 0.5f, depth * 0.5f),
        DirectX::XMFLOAT3(-width * 0.5f, height * 0.5f, depth * 0.5f),
        DirectX::XMFLOAT3(width * 0.5f, height * 0.5f, depth * 0.5f),
        DirectX::XMFLOAT3(width * 0.5f, -height * 0.5f, depth * 0.5f),
        // front
        DirectX::XMFLOAT3(-width * 0.5f, -height * 0.5f, -depth * 0.5f),
        DirectX::XMFLOAT3(-width * 0.5f, height * 0.5f, -depth * 0.5f),
        DirectX::XMFLOAT3(width * 0.5f, height * 0.5f, -depth * 0.5f),
        DirectX::XMFLOAT3(width * 0.5f, -height * 0.5f, -depth * 0.5f)
    };

    uint32 indices[36] = {
        0,1,2, 0,2,3, //behine
        7,6,2, 7,2,3, //right
        4,5,6, 4,6,7, // front
        0,1,5, 0,5,4, //right
        5,1,2, 5,2,6, // top
        0,4,7, 0,7,3 //bottom
    };
    meshData.Indices32.assign(&indices[0], &indices[36]);
    meshData.Vertices.resize(8);
    for (uint32 i = 0; i < 8; i++)
    {
        meshData.Vertices[i].Position = pos[i];
    }
    return meshData;
}

void GeometryGenerator::Subdivide(MeshData& meshData)
{
    uint32 triangleCount = meshData.Indices32.size();
    uint32 baseIndex = meshData.Vertices.size();
    assert(triangleCount > 0 && triangleCount % 3 == 0);
    meshData.Indices32.clear();
    for (uint32 i = 0; i < triangleCount; i+=3)
    {
        uint32 P0Index = meshData.Indices32[i];
        uint32 P1Index = meshData.Indices32[i + 1];
        uint32 P2Index = meshData.Indices32[i + 2];
        
        DirectX::XMVECTOR P0 = DirectX::XMLoadFloat3(&meshData.Vertices[P0Index].Position);
        DirectX::XMVECTOR P1 = DirectX::XMLoadFloat3(&meshData.Vertices[P1Index].Position);
        DirectX::XMVECTOR P2 = DirectX::XMLoadFloat3(&meshData.Vertices[P2Index].Position);
        
        DirectX::XMVECTOR M0 = DirectX::XMVectorDivide(DirectX::XMVectorAdd(P0, P1), DirectX::XMVectorSet(2.0f, 2.0f, 2.0f, 1.0f));
        DirectX::XMVECTOR M1 = DirectX::XMVectorDivide(DirectX::XMVectorAdd(P1, P2), DirectX::XMVectorSet(2.0f, 2.0f, 2.0f, 1.0f));
        DirectX::XMVECTOR M2 = DirectX::XMVectorDivide(DirectX::XMVectorAdd(P2, P0), DirectX::XMVectorSet(2.0f, 2.0f, 2.0f, 1.0f));

        Vertex v0;
        Vertex v1;
        Vertex v2;
        DirectX::XMStoreFloat3(&v0.Position, M0);
        DirectX::XMStoreFloat3(&v1.Position, M1);
        DirectX::XMStoreFloat3(&v2.Position, M2);
        meshData.Vertices.push_back(v0);
        meshData.Vertices.push_back(v1);
        meshData.Vertices.push_back(v2);

        uint32 M0Index = baseIndex;
        uint32 M1Index = baseIndex + 1;
        uint32 M2Index = baseIndex + 2;

        meshData.Indices32.push_back(M0Index);
        meshData.Indices32.push_back(P1Index);
        meshData.Indices32.push_back(M1Index);

        meshData.Indices32.push_back(P0Index);
        meshData.Indices32.push_back(M0Index);
        meshData.Indices32.push_back(M2Index);

        meshData.Indices32.push_back(M2Index);
        meshData.Indices32.push_back(M0Index);
        meshData.Indices32.push_back(M1Index);

        meshData.Indices32.push_back(M2Index);
        meshData.Indices32.push_back(M1Index);
        meshData.Indices32.push_back(P2Index);

        baseIndex+=3;
    }
}
