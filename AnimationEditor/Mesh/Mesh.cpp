#include "Mesh.h"
#include<fbxsdk.h>
#include<functional>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wininet.lib")

bool Mesh::CreateCube(ID3D11Device* device)
{
	if (!device)return false;

	//立方体の８頂点定義
    Vertex vertices[] = {
            { DirectX::XMFLOAT3(-0.5f,  0.5f, -0.5f), DirectX::XMFLOAT4(1.0f, 0.2f, 0.2f, 1.0f) },
            { DirectX::XMFLOAT3(0.5f,  0.5f, -0.5f), DirectX::XMFLOAT4(0.2f, 1.0f, 0.2f, 1.0f) },
            { DirectX::XMFLOAT3(0.5f, -0.5f, -0.5f), DirectX::XMFLOAT4(0.2f, 0.2f, 1.0f, 1.0f) },
            { DirectX::XMFLOAT3(-0.5f, -0.5f, -0.5f), DirectX::XMFLOAT4(1.0f, 1.0f, 0.2f, 1.0f) },
            { DirectX::XMFLOAT3(-0.5f,  0.5f,  0.5f), DirectX::XMFLOAT4(1.0f, 0.2f, 1.0f, 1.0f) },
            { DirectX::XMFLOAT3(0.5f,  0.5f,  0.5f), DirectX::XMFLOAT4(0.2f, 1.0f, 1.0f, 1.0f) },
            { DirectX::XMFLOAT3(0.5f, -0.5f,  0.5f), DirectX::XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f) },
            { DirectX::XMFLOAT3(-0.5f, -0.5f,  0.5f), DirectX::XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f) },
    };

    //頂点バッファ
    D3D11_BUFFER_DESC vbDesc{};
    //バッファのサイズを設定
    vbDesc.ByteWidth = sizeof(vertices);
    //バッファの使用方法を設定
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    //バッファの種類を設定
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbData{};
    //バッファの初期データを設定
    vbData.pSysMem = vertices;
    //バッファを生成
    if (FAILED(device->CreateBuffer(&vbDesc, &vbData, vertexBuffer_.GetAddressOf()))) {
        return false;
    }

    // 立方体の各面を構成するインデックス (時計回り)
    WORD indices[] = {
        0, 1, 2,  0, 2, 3, // 前面
        1, 5, 6,  1, 6, 2, // 右面
        5, 4, 7,  5, 7, 6, // 背面
        4, 0, 3,  4, 3, 7, // 左面
        4, 5, 1,  4, 1, 0, // 上面
        3, 2, 6,  3, 6, 7  // 下面
    };
    //インデックス数を設定
    indexCount_ = _countof(indices);

    // インデックスバッファ生成
    D3D11_BUFFER_DESC ibDesc{};
    //バッファのサイズを設定
    ibDesc.ByteWidth = sizeof(indices);
    //バッファの使用方法を設定
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    //バッファの種類を設定
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibData{};
    //バッファの初期データを設定
    ibData.pSysMem = indices;
    //バッファを生成
    if (FAILED(device->CreateBuffer(&ibDesc, &ibData, indexBuffer_.GetAddressOf()))) {
        return false;
    }

    return true;
}

void Mesh::Draw(ID3D11DeviceContext* context)
{
    if (!context || !vertexBuffer_ || !indexBuffer_) return;
	//バッファのストライドを設定
    UINT stride = sizeof(Vertex);
    //バッファのオフセットを設定
    UINT offset = 0;
    //描画に必要なバッファをセット
    context->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &stride, &offset);
    //描画に必要なインデックスバッファをセット
    context->IASetIndexBuffer(indexBuffer_.Get(), DXGI_FORMAT_R32_UINT, 0);
    //描画するプリミティブの種類を設定
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    //描画
    context->DrawIndexed(indexCount_, 0, 0);
}

bool Mesh::LoadFBX(ID3D11Device* device, const std::string& filePath)
{
    if (!device)return false;

    //FBXマネージャーおよびシーンの作成
    FbxManager* manager = FbxManager::Create();
    if (!manager)return false;

    FbxIOSettings* ios = FbxIOSettings::Create(manager, "");
    manager->SetIOSettings(ios);

    FbxImporter* importer = FbxImporter::Create(manager, "");
    if (!importer->Initialize(filePath.c_str(), -1, manager->GetIOSettings())) {
        importer->Destroy();
        manager->Destroy();
        return false;
    }

    FbxScene* scene = FbxScene::Create(manager, "ImportScene");
    importer->Import(scene);
    importer->Destroy();

    //ジオメトリの三角面化
    FbxGeometryConverter converter(manager);
    converter.Triangulate(scene, true);

    //ルートノードから再帰的にメッシュノードを探索
    FbxNode* rootNode = scene->GetRootNode();
    FbxMesh* targetMesh = nullptr;

    std::function<void(FbxNode*)> findMeshNode = [&](FbxNode* node) {
        if (!node || targetMesh) return;
        if (FbxMesh* mesh = node->GetMesh()) {
            targetMesh = mesh;
            return;
        }
        for (int i = 0; i < node->GetChildCount(); ++i) {
            findMeshNode(node->GetChild(i));
        }
        };
    findMeshNode(rootNode);
    if (!targetMesh) {
        manager->Destroy();
        return false;
    }

    //頂点データ、インデックスの抽出
    const FbxVector4* controlPoints = targetMesh->GetControlPoints();
    const int polygonCount = targetMesh->GetPolygonCount();

    //法線レイヤーの取得
    FbxArray<FbxVector4> normals;
    targetMesh->GetPolygonVertexNormals(normals);

    //UVレイヤーの取得
    FbxStringList uvSetNames;
    targetMesh->GetUVSetNames(uvSetNames);
    const char* uvSetName = (uvSetNames.GetCount() > 0) ? uvSetNames.GetStringAt(0) : nullptr;

    std::vector<Vertex>vertices;
    std::vector<UINT>indices;
    vertices.reserve(polygonCount * 3);
    indices.reserve(polygonCount * 3);

    int vertexCounter = 0;
    for (int p = 0; p < polygonCount; ++p) {
        for (int v = 0; v < 3; ++v) {
            int cpIndex = targetMesh->GetPolygonVertex(p, v);

            Vertex vert{};

            // 頂点ローカル座標 (FBXは右手/左手や単位系cm/mの違いがあるが、まずはそのまま取得)
            vert.pos.x = static_cast<float>(controlPoints[cpIndex][0]);
            vert.pos.y = static_cast<float>(controlPoints[cpIndex][1]);
            vert.pos.z = static_cast<float>(controlPoints[cpIndex][2]);
            // デフォルト頂点カラー (白) を代入して NaN を防ぐ
            vert.col_ = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            // 法線
            if (normals.Size() > vertexCounter) {
                vert.normal.x = static_cast<float>(normals[vertexCounter][0]);
                vert.normal.y = static_cast<float>(normals[vertexCounter][1]);
                vert.normal.z = static_cast<float>(normals[vertexCounter][2]);
            }
            else {
                vert.normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
            }

            // UV
            if (uvSetName) {
                bool unmapped = false;
                FbxVector2 uv;
                targetMesh->GetPolygonVertexUV(p, v, uvSetName, uv, unmapped);
                vert.uv.x = static_cast<float>(uv[0]);
                vert.uv.y = 1.0f - static_cast<float>(uv[1]); // DirectX系はV軸反転
            }
            else {
                vert.uv = DirectX::XMFLOAT2(0.0f, 0.0f);
            }

            vertices.push_back(vert);
            indices.push_back(vertexCounter);
            vertexCounter++;
        }
    }

    indexCount_ = static_cast<UINT>(indices.size());

    // 5. GPU 頂点バッファ生成
    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * vertices.size());
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbData{};
    vbData.pSysMem = vertices.data();
    if (FAILED(device->CreateBuffer(&vbDesc, &vbData, vertexBuffer_.ReleaseAndGetAddressOf()))) {
        manager->Destroy();
        return false;
    }

    // 6. GPU インデックスバッファ生成 (頂点数が増えるため 32bit インデックス: DXGI_FORMAT_R32_UINT)
    D3D11_BUFFER_DESC ibDesc{};
    ibDesc.ByteWidth = static_cast<UINT>(sizeof(UINT) * indices.size());
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibData{};
    ibData.pSysMem = indices.data();
    if (FAILED(device->CreateBuffer(&ibDesc, &ibData, indexBuffer_.ReleaseAndGetAddressOf()))) {
        manager->Destroy();
        return false;
    }

    // 7. FBX SDKマネージャーの破棄 (シーンごとメモリ解放)
    manager->Destroy();
    return true;

}