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
		//���_�o�b�t�@
		ComPtr<ID3D12Resource> vertBuff = nullptr;
		//���_�o�b�t�@�r���[
		D3D12_VERTEX_BUFFER_VIEW vbView{};
		//�萔�o�b�t�@
		ComPtr<ID3D12Resource>constBuff = nullptr;
		//Z�����̉�]�p
		float rotation = 45.0f;
		//���W
		XMFLOAT3 position = { 1280 / 2,720 / 2,0 };
		//���[���h�s��
		XMMATRIX matWorld;
		//�F(RGBA)
		XMFLOAT4 color = { 1,1,1,1 };
		//�e�N�X�`���ԍ�
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