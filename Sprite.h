#pragma once

#include<Windows.h>
#include<wrl.h>
#include<d3d12.h>
#include <DirectXMath.h>


class Sprite
{
public:
	struct SpriteData
	{
		//頂点バッファ
		ComPtr<ID3D12Resource> vertBuff = nullptr;
		//頂点バッファビュー
		D3D12_VERTEX_BUFFER_VIEW vbView{};
		//定数バッファ
		ComPtr<ID3D12Resource>constBuff = nullptr;
		//Z軸回りの回転角
		float rotation = 45.0f;
		//座標
		XMFLOAT3 position = { 1280 / 2,720 / 2,0 };
		//ワールド行列
		XMMATRIX matWorld;
		//色(RGBA)
		XMFLOAT4 color = { 1,1,1,1 };
		//テクスチャ番号
		UINT texNumber = 0;
	};

	Sprite();
	~Sprite();

private:

};

Sprite::Sprite()
{
}

Sprite::~Sprite()
{
}