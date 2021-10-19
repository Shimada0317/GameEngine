#include<Windows.h>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<vector>
#include<string>
#include<DirectXMath.h>
#include<d3dcompiler.h>
#define DIRECTINPUT_VERSION 0x0800//DirectInput�̃o�[�W�����w��
#include<dinput.h>
#include<DirectXTex.h>
#include<wrl.h>
#include<d3dx12.h>
#include"Input.h"

#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

using namespace DirectX;
using namespace Microsoft::WRL;

const int spriteSRVCount = 512;

//�萔�o�b�t�@�p�f�[�^�\����
struct ConstBufferData {
	XMFLOAT4 color;//�F
	XMMATRIX mat;//3D�ϊ��s��
};

//���_�f�[�^�\����
struct Vertex
{
	XMFLOAT3 pos;//xyz���W
	XMFLOAT3 normal;//�@���x�N�g��
	XMFLOAT2 uv;//uv���W
};

struct VertexPosUv
{
	XMFLOAT3 pos;
	XMFLOAT2 uv;
};

struct PipelineSet
{
	ComPtr<ID3D12PipelineState>pipelinestate;

	ComPtr<ID3D12RootSignature>rootsignature;
};

struct Object3d
{
	//�萔�o�b�t�@
	ID3D12Resource* constBUff;
	//�萔�o�b�t�@�r���[�̃n���h��
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandleCBV;
	//�萔�o�b�t�@�r���[�̃n���h��
	D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandleCBV;

	XMFLOAT3 scale = { 1,1,1 };
	XMFLOAT3 rotation = { 0,0,0 };
	XMFLOAT3 position = { 0,0,0 };
	XMMATRIX matWorld;
	Object3d* parent = nullptr;
};

struct Sprite
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
	XMFLOAT3 position = { 1280/2,720/2,0 };
	//���[���h�s��
	XMMATRIX matWorld;
	//�F(RGBA)
	XMFLOAT4 color = { 1,1,1,1 };
	//�e�N�X�`���ԍ�
	UINT texNumber = 0;
};

struct SpriteCommon
{
	//�p�C�v���C���Z�b�g
	PipelineSet pipeineset;
	//�ˉe�s��
	XMMATRIX matProjection{};
	//�e�N�X�`���p�f�X�N���v�^�q�[�v�̐���
	ComPtr<ID3D12DescriptorHeap> descHeap;
	//�e�N�X�`�����\�[�X(�e�N�X�`��)�̔z��
	ComPtr<ID3D12Resource>texBuff[spriteSRVCount];
};

void InitializeObject3d(Object3d* object, int index, ComPtr<ID3D12Device> dev, ComPtr<ID3D12DescriptorHeap> descHeap)
{
	HRESULT result;
	//�萔�o�b�t�@�̃q�[�v�ݒ�
	// ���_�o�b�t�@�̐ݒ�
	D3D12_HEAP_PROPERTIES heapprop{};   // �q�[�v�ݒ�
	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD; // GPU�ւ̓]���p

	D3D12_RESOURCE_DESC resdesc{};  // ���\�[�X�ݒ�
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = (sizeof(ConstBufferData) + 0xff) & ~0xff; // ���_�f�[�^�S�̂̃T�C�Y
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.SampleDesc.Count = 1;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	result = dev->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&object->constBUff)
	);

	UINT descHandleIncrementSize =
		dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	object->cpuDescHandleCBV = descHeap->GetCPUDescriptorHandleForHeapStart();
	object->cpuDescHandleCBV.ptr += index * descHandleIncrementSize;

	object->gpuDescHandleCBV = descHeap->GetGPUDescriptorHandleForHeapStart();
	object->gpuDescHandleCBV.ptr += index * descHandleIncrementSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
	cbvDesc.BufferLocation = object->constBUff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = (UINT)object->constBUff->GetDesc().Width;
	dev->CreateConstantBufferView(&cbvDesc, object->cpuDescHandleCBV);
}

void UpdateObject3d(Object3d* object, XMMATRIX& matview, XMMATRIX& matProjection)
{
	XMMATRIX matScale, matRot, matTrans;

	//�X�P�[���A��]�A���s�ړ��s��̌v�Z
	matScale = XMMatrixScaling(object->scale.x, object->scale.y, object->scale.z);
	matRot = XMMatrixIdentity();
	matRot *= XMMatrixRotationZ(XMConvertToRadians(object->rotation.z));
	matRot *= XMMatrixRotationX(XMConvertToRadians(object->rotation.x));
	matRot *= XMMatrixRotationY(XMConvertToRadians(object->rotation.y));
	matTrans = XMMatrixTranslation(object->position.x, object->position.y, object->position.z);
	//���[���h�s��̍���
	object->matWorld = XMMatrixIdentity();//�ό`�����Z�b�g
	object->matWorld *= matScale;//���[���h�s��ɃX�P�[�����O�𔽉f
	object->matWorld *= matRot;//���[���h�s��ɉ�]�𔽉f
	object->matWorld *= matTrans;//���[���h�s��ɕ��s�ړ��𔽉f
	//�e�I�u�W�F�N�g�������
	if (object->parent != nullptr) {
		//�e�I�u�W�F�N�g�̃��[���h�s����|����
		object->matWorld *= object->parent->matWorld;
	}
	//�萔�o�b�t�@�փf�[�^�]��
	ConstBufferData* constMap = nullptr;
	if (SUCCEEDED(object->constBUff->Map(0, nullptr, (void**)&constMap))) {
		constMap->color = XMFLOAT4(1, 1, 1, 1);
		constMap->mat = object->matWorld * matview * matProjection;
		object->constBUff->Unmap(0, nullptr);
	}
}

PipelineSet object3dCreateGrphicsPipeline(ID3D12Device* dev)
{
#pragma region �V�F�[�_�ǂݍ���
	HRESULT result;
	ComPtr<ID3DBlob> vsBlob = nullptr; // ���_�V�F�[�_�I�u�W�F�N�g
	ComPtr<ID3DBlob> psBlob = nullptr; // �s�N�Z���V�F�[�_�I�u�W�F�N�g
	ComPtr<ID3DBlob> errorBlob = nullptr; // �G���[�I�u�W�F�N�g

	// ���_�V�F�[�_�̓ǂݍ��݂ƃR���p�C��
	result = D3DCompileFromFile(
		L"Resources/shaders/BasicVS.hlsl",  // �V�F�[�_�t�@�C����
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // �C���N���[�h�\�ɂ���
		"main", "vs_5_0", // �G���g���[�|�C���g���A�V�F�[�_�[���f���w��
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // �f�o�b�O�p�ݒ�
		0,
		&vsBlob, &errorBlob);

	if (FAILED(result)) {
		// errorBlob����G���[���e��string�^�ɃR�s�[
		std::string errstr;
		errstr.resize(errorBlob->GetBufferSize());

		std::copy_n((char*)errorBlob->GetBufferPointer(),
			errorBlob->GetBufferSize(),
			errstr.begin());
		errstr += "\n";
		// �G���[���e���o�̓E�B���h�E�ɕ\��
		OutputDebugStringA(errstr.c_str());
		exit(1);
	}

	// �s�N�Z���V�F�[�_�̓ǂݍ��݂ƃR���p�C��
	result = D3DCompileFromFile(
		L"Resources/shaders/BasicPS.hlsl",   // �V�F�[�_�t�@�C����
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // �C���N���[�h�\�ɂ���
		"main", "ps_5_0", // �G���g���[�|�C���g���A�V�F�[�_�[���f���w��
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // �f�o�b�O�p�ݒ�
		0,
		&psBlob, &errorBlob);

	if (FAILED(result)) {
		// errorBlob����G���[���e��string�^�ɃR�s�[
		std::string errstr;
		errstr.resize(errorBlob->GetBufferSize());

		std::copy_n((char*)errorBlob->GetBufferPointer(),
			errorBlob->GetBufferSize(),
			errstr.begin());
		errstr += "\n";
		// �G���[���e���o�̓E�B���h�E�ɕ\��
		OutputDebugStringA(errstr.c_str());
		exit(1);
	}


	//���_���C�A�E�g
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{//xyz���W
			"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
		},
		{//�@���x�N�g��
			"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
		},
		{//uv���W
			"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
		},
	};

#pragma endregion

#pragma region �p�C�v���C��

	//�O���t�B�b�N�X�p�C�v���C���ݒ�
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline{};

	/*gpipeline.VS.pShaderBytecode = vsBlob->GetBufferPointer();
	gpipeline.VS.BytecodeLength = vsBlob->GetBufferSize();
	gpipeline.PS.pShaderBytecode = psBlob->GetBufferPointer();
	gpipeline.PS.BytecodeLength = psBlob->GetBufferSize();*/

	gpipeline.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
	gpipeline.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());



	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;//�W���ݒ�
	//gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;//�J�����O���Ȃ�
	//gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;//�|���S�����h��Ԃ�
	//gpipeline.RasterizerState.DepthClipEnable = true;//�[�x�N���b�s���O��L����

	gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	//gpipeline.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	//�����_�[�^�[�Q�b�g�̃u�����h�ݒ�
	D3D12_RENDER_TARGET_BLEND_DESC& blenddesc = gpipeline.BlendState.RenderTarget[0];
	blenddesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;//�W���ݒ�
	//blenddesc.BlendEnable = true;//�u�����h��L���ɂ���
	//blenddesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;//���Z
	//blenddesc.SrcBlendAlpha = D3D12_BLEND_ONE;//�\�[�X�̒l��100%�g��
	//blenddesc.DestBlendAlpha = D3D12_BLEND_ZERO;//�e�X�g�̒l��0%�g��

	////���Z
	//blenddesc.BlendOp = D3D12_BLEND_OP_ADD;
	//blenddesc.SrcBlend = D3D12_BLEND_ONE;//�\�[�X�̒l��100%�g��
	//blenddesc.DestBlend = D3D12_BLEND_ONE;//�f�X�g�̒l��100%�g��

	////���Z
	//blenddesc.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
	//blenddesc.SrcBlend = D3D12_BLEND_ONE;//�\�[�X�̒l��100%�g��
	//blenddesc.DestBlend = D3D12_BLEND_ONE;//�f�X�g�̒l��100%�g��

	////���]
	//blenddesc.BlendOp = D3D12_BLEND_OP_ADD;
	//blenddesc.SrcBlend = D3D12_BLEND_INV_DEST_COLOR;//1.0f�f�X�g�J���[�̒l
	//blenddesc.DestBlend = D3D12_BLEND_ZERO;//�g��Ȃ�

	//������
	//blenddesc.BlendOp = D3D12_BLEND_OP_ADD;//���Z
	//blenddesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;//�\�[�X�̃��l
	//blenddesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;//1.0f�\�[�X�̒l

	gpipeline.InputLayout.pInputElementDescs = inputLayout;
	gpipeline.InputLayout.NumElements = _countof(inputLayout);

	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	gpipeline.NumRenderTargets = 1;//�`��Ώۂ�1��
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;//0�`255�w���RGBA
	gpipeline.SampleDesc.Count = 1;//1�҂�����ɂ�1��T���v�����O

	//�f�v�X�e���V���X�e�[�g�̐ݒ�
	/*gpipeline.DepthStencilState.DepthEnable = true;
	gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;*/

	gpipeline.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;

#pragma endregion

	PipelineSet pipelineSet;

	CD3DX12_DESCRIPTOR_RANGE descRangeCBV, descRangeSRV;
	descRangeCBV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	descRangeSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER rootparm[2];
	rootparm[0].InitAsConstantBufferView(0);
	rootparm[1].InitAsDescriptorTable(1, &descRangeSRV);

#pragma region �V�O�l�`���[

	CD3DX12_STATIC_SAMPLER_DESC samplerDesc = CD3DX12_STATIC_SAMPLER_DESC(0);

	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//�J��Ԃ�
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//�J��Ԃ�
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//���s�J��Ԃ�
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;//�{�[�_�[�̎��͍�
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;//�⊮���Ȃ�
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;//�~�j�}�b�v�ő�l
	samplerDesc.MinLOD = 0.0f;//�~�j�}�b�v�ŏ��l
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//�s�N�Z���V�F�[�_����̂݉�


	ComPtr<ID3D12RootSignature> rootsignature;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_0(_countof(rootparm), rootparm, 1, &samplerDesc,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);


	//D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	//rootSignatureDesc.Flags =
	//D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	//rootSignatureDesc.pParameters = rootparams;//���[�g�p�����[�^�̐擪�A�h���X
	//rootSignatureDesc.NumParameters = _countof(rootparams);//���[�g�p�����|�^��
	//rootSignatureDesc.pStaticSamplers = &samplerDesc;
	//rootSignatureDesc.NumStaticSamplers = 1;
#pragma endregion
	ComPtr<ID3DBlob>rootSigBlob;
	result = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errorBlob);
	result = dev->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&pipelineSet.rootsignature));


	// �p�C�v���C���Ƀ��[�g�V�O�l�`�����Z�b�g
	gpipeline.pRootSignature = pipelineSet.rootsignature.Get();
	ComPtr<ID3D12PipelineState> pipelinestate = nullptr;
	result = dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&pipelineSet.pipelinestate));


	return pipelineSet;
}

PipelineSet SpriteCreateGraaphicsPipeline(ID3D12Device* dev)
{
#pragma region �V�F�[�_�ǂݍ���
	HRESULT result;
	ComPtr<ID3DBlob> vsBlob = nullptr; // ���_�V�F�[�_�I�u�W�F�N�g
	ComPtr<ID3DBlob> psBlob = nullptr; // �s�N�Z���V�F�[�_�I�u�W�F�N�g
	ComPtr<ID3DBlob> errorBlob = nullptr; // �G���[�I�u�W�F�N�g

	// ���_�V�F�[�_�̓ǂݍ��݂ƃR���p�C��
	result = D3DCompileFromFile(
		L"Resources/shaders/SpriteVS.hlsl",  // �V�F�[�_�t�@�C����
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // �C���N���[�h�\�ɂ���
		"main", "vs_5_0", // �G���g���[�|�C���g���A�V�F�[�_�[���f���w��
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // �f�o�b�O�p�ݒ�
		0,
		&vsBlob, &errorBlob);

	if (FAILED(result)) {
		// errorBlob����G���[���e��string�^�ɃR�s�[
		std::string errstr;
		errstr.resize(errorBlob->GetBufferSize());

		std::copy_n((char*)errorBlob->GetBufferPointer(),
			errorBlob->GetBufferSize(),
			errstr.begin());
		errstr += "\n";
		// �G���[���e���o�̓E�B���h�E�ɕ\��
		OutputDebugStringA(errstr.c_str());
		exit(1);
	}


	// �s�N�Z���V�F�[�_�̓ǂݍ��݂ƃR���p�C��
	result = D3DCompileFromFile(
		L"Resources/shaders/SpritePS.hlsl",   // �V�F�[�_�t�@�C����
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // �C���N���[�h�\�ɂ���
		"main", "ps_5_0", // �G���g���[�|�C���g���A�V�F�[�_�[���f���w��
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // �f�o�b�O�p�ݒ�
		0,
		&psBlob, &errorBlob);

	if (FAILED(result)) {
		// errorBlob����G���[���e��string�^�ɃR�s�[
		std::string errstr;
		errstr.resize(errorBlob->GetBufferSize());

		std::copy_n((char*)errorBlob->GetBufferPointer(),
			errorBlob->GetBufferSize(),
			errstr.begin());
		errstr += "\n";
		// �G���[���e���o�̓E�B���h�E�ɕ\��
		OutputDebugStringA(errstr.c_str());
		exit(1);
	}


	//���_���C�A�E�g
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{//xyz���W
			"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
		},
		{//uv���W
			"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
		},
	};

#pragma endregion

#pragma region �p�C�v���C��

	//�O���t�B�b�N�X�p�C�v���C���ݒ�
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline{};

	/*gpipeline.VS.pShaderBytecode = vsBlob->GetBufferPointer();
	gpipeline.VS.BytecodeLength = vsBlob->GetBufferSize();
	gpipeline.PS.pShaderBytecode = psBlob->GetBufferPointer();
	gpipeline.PS.BytecodeLength = psBlob->GetBufferSize();*/

	gpipeline.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
	gpipeline.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());



	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;//�W���ݒ�
	//gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;//�J�����O���Ȃ�
	//gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;//�|���S�����h��Ԃ�
	//gpipeline.RasterizerState.DepthClipEnable = true;//�[�x�N���b�s���O��L����

	gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	//gpipeline.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	//�����_�[�^�[�Q�b�g�̃u�����h�ݒ�
	D3D12_RENDER_TARGET_BLEND_DESC& blenddesc = gpipeline.BlendState.RenderTarget[0];
	blenddesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;//�W���ݒ�
	//blenddesc.BlendEnable = true;//�u�����h��L���ɂ���
	//blenddesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;//���Z
	//blenddesc.SrcBlendAlpha = D3D12_BLEND_ONE;//�\�[�X�̒l��100%�g��
	//blenddesc.DestBlendAlpha = D3D12_BLEND_ZERO;//�e�X�g�̒l��0%�g��

	////���Z
	//blenddesc.BlendOp = D3D12_BLEND_OP_ADD;
	//blenddesc.SrcBlend = D3D12_BLEND_ONE;//�\�[�X�̒l��100%�g��
	//blenddesc.DestBlend = D3D12_BLEND_ONE;//�f�X�g�̒l��100%�g��

	////���Z
	//blenddesc.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
	//blenddesc.SrcBlend = D3D12_BLEND_ONE;//�\�[�X�̒l��100%�g��
	//blenddesc.DestBlend = D3D12_BLEND_ONE;//�f�X�g�̒l��100%�g��

	////���]
	//blenddesc.BlendOp = D3D12_BLEND_OP_ADD;
	//blenddesc.SrcBlend = D3D12_BLEND_INV_DEST_COLOR;//1.0f�f�X�g�J���[�̒l
	//blenddesc.DestBlend = D3D12_BLEND_ZERO;//�g��Ȃ�

	//������
	//blenddesc.BlendOp = D3D12_BLEND_OP_ADD;//���Z
	//blenddesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;//�\�[�X�̃��l
	//blenddesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;//1.0f�\�[�X�̒l

	gpipeline.InputLayout.pInputElementDescs = inputLayout;
	gpipeline.InputLayout.NumElements = _countof(inputLayout);

	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	gpipeline.NumRenderTargets = 1;//�`��Ώۂ�1��
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;//0�`255�w���RGBA
	gpipeline.SampleDesc.Count = 1;//1�҂�����ɂ�1��T���v�����O

	//�f�v�X�e���V���X�e�[�g�̐ݒ�
	/*gpipeline.DepthStencilState.DepthEnable = true;
	gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;*/

	gpipeline.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	gpipeline.DepthStencilState.DepthEnable = false;
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;

#pragma endregion

	PipelineSet pipelineSet;

	CD3DX12_DESCRIPTOR_RANGE descRangeCBV, descRangeSRV;
	descRangeCBV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	descRangeSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER rootparm[2];
	rootparm[0].InitAsConstantBufferView(0);
	rootparm[1].InitAsDescriptorTable(1, &descRangeSRV);

#pragma region �V�O�l�`���[

	CD3DX12_STATIC_SAMPLER_DESC samplerDesc = CD3DX12_STATIC_SAMPLER_DESC(0);

	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//�J��Ԃ�
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//�J��Ԃ�
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//���s�J��Ԃ�
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;//�{�[�_�[�̎��͍�
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;//�⊮���Ȃ�
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;//�~�j�}�b�v�ő�l
	samplerDesc.MinLOD = 0.0f;//�~�j�}�b�v�ŏ��l
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//�s�N�Z���V�F�[�_����̂݉�


	ComPtr<ID3D12RootSignature> rootsignature;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_0(_countof(rootparm), rootparm, 1, &samplerDesc,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);


	//D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	//rootSignatureDesc.Flags =
	//D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	//rootSignatureDesc.pParameters = rootparams;//���[�g�p�����[�^�̐擪�A�h���X
	//rootSignatureDesc.NumParameters = _countof(rootparams);//���[�g�p�����|�^��
	//rootSignatureDesc.pStaticSamplers = &samplerDesc;
	//rootSignatureDesc.NumStaticSamplers = 1;
#pragma endregion
	ComPtr<ID3DBlob>rootSigBlob;
	result = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errorBlob);
	result = dev->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&pipelineSet.rootsignature));


	// �p�C�v���C���Ƀ��[�g�V�O�l�`�����Z�b�g
	gpipeline.pRootSignature = pipelineSet.rootsignature.Get();
	ComPtr<ID3D12PipelineState> pipelinestate = nullptr;
	result = dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&pipelineSet.pipelinestate));


	return pipelineSet;
}

void DrawObject3d(Object3d* object, ComPtr<ID3D12GraphicsCommandList> cmdList,
	ComPtr<ID3D12DescriptorHeap> descHeap, D3D12_VERTEX_BUFFER_VIEW& vbView,
	D3D12_INDEX_BUFFER_VIEW& ibView, D3D12_GPU_DESCRIPTOR_HANDLE
	gpuDescHandleSRV, UINT numIndices)
{
	//���_�o�b�t�@�̐ݒ�
	cmdList->IASetVertexBuffers(0, 1, &vbView);
	//�C���f�b�N�X�o�b�t�@�̐ݒ�
	cmdList->IASetIndexBuffer(&ibView);
	//�f�X�N���v�^�q�[�v�̔z��
	ID3D12DescriptorHeap* ppHeaps[] = { descHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	//�萔�o�b�t�@�r���[���Z�b�g
	cmdList->SetGraphicsRootConstantBufferView(0, object->constBUff->GetGPUVirtualAddress());
	//�V�F�[�_���\�[�X�r���[���Z�b�g
	cmdList->SetGraphicsRootDescriptorTable(1, gpuDescHandleSRV);
	//�`��R�}���h
	cmdList->DrawIndexedInstanced(numIndices, 1, 0, 0, 0);
}

Sprite SpriteCreate(ID3D12Device* dev, int window_width, int window_height)
{
	HRESULT result = S_FALSE;

	//�V�����X�v���C�g�����
	Sprite sprite{};

	//���_�f�[�^
	VertexPosUv vertices[] = {
		{{0.0f,100.0f,0.0f},{0.0f,1.0f}},
		{{0.0f,0.0f,0.0f},{0.0f,0.0f}},
		{{100.0f,100.0f,0.0f},{1.0f,1.0f}},
		{{100.0f,0.0f,0.0f},{1.0f,0.0f}},
	};
	//���_�o�b�t�@����
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices)),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&sprite.vertBuff));

	//���_�o�b�t�@�ւ̃f�[�^�]��
	VertexPosUv* vertMap = nullptr;
	result = sprite.vertBuff->Map(0, nullptr, (void**)&vertMap);
	memcpy(vertMap, vertices, sizeof(vertices));
	sprite.vertBuff->Unmap(0, nullptr);

	//���_�o�b�t�@�r���[�̍쐬
	sprite.vbView.BufferLocation = sprite.vertBuff->GetGPUVirtualAddress();
	sprite.vbView.SizeInBytes = sizeof(vertices);
	sprite.vbView.StrideInBytes = sizeof(vertices[0]);

	//�萔�o�b�t�@�̐���
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((sizeof(ConstBufferData) + 0xff) & ~0xff),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&sprite.constBuff));

	//�萔�o�b�t�@�Ƀf�[�^�]��
	ConstBufferData* constMap = nullptr;
	result = sprite.constBuff->Map(0, nullptr, (void**)&constMap);
	constMap->color = XMFLOAT4(1, 1, 1, 1);
	//���s���e�s��
	constMap->mat = XMMatrixOrthographicOffCenterLH(
		0.0f, window_width, window_height, 0.0f, 0.0f, 1.0f);
	sprite.constBuff->Unmap(0, nullptr);

	return sprite;
}

//�X�v���C�g���ʂ̃O���t�B�b�N�R�}���h�̃Z�b�g
void SpriteCommonBeginDraw(ID3D12GraphicsCommandList* cmdList, const SpriteCommon& spriteCommon)
{
	//�p�C�v���C���X�e�[�g�̐ݒ�
	cmdList->SetPipelineState(spriteCommon.pipeineset.pipelinestate.Get());
	//���[�g�V�O�l�`���̐ݒ�
	cmdList->SetGraphicsRootSignature(spriteCommon.pipeineset.rootsignature.Get());
	//�v���~�e�B�u�`���ݒ�
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	//�e�N�X�`���p�f�X�N���v�^�q�[�v�̐ݒ�
	ID3D12DescriptorHeap* ppHeaps[] = { spriteCommon.descHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
}

//�X�v���C�g�P�̕`��
void SpriteDraw(const Sprite& sprite, ID3D12GraphicsCommandList* cmdList,const SpriteCommon&spriteCommon,
	ID3D12Device* dev)
{
	//���_�o�b�t�@�A���Z�b�g
	cmdList->IASetVertexBuffers(0, 1, &sprite.vbView);
	//�萔�o�b�t�@���Z�b�g
	cmdList->SetGraphicsRootConstantBufferView(0, sprite.constBuff->GetGPUVirtualAddress());

	//�V�F�[�_���\�[�X�r���[���Z�b�g
	cmdList->SetGraphicsRootDescriptorTable(1,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(
			spriteCommon.descHeap->GetGPUDescriptorHandleForHeapStart(),
			sprite.texNumber,
			dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)));
	//�|���S���̕`��
	cmdList->DrawInstanced(4, 1, 0, 0);
}

//�X�v���C�g�̋��ʃf�[�^����
SpriteCommon SpriteCommonCreate(ID3D12Device* dev,int window_width,int window_height) 
{
	HRESULT result = S_FALSE;
	//�V���ȃX�v���C�g���ʃf�[�^�𐶐�
	SpriteCommon spriteCommon{};
	//�X�v���C�g�p�̃p�C�v���C������
	spriteCommon.pipeineset = SpriteCreateGraaphicsPipeline(dev);
	//���s���e�̎ˉe�s�񐶐�
	spriteCommon.matProjection = XMMatrixOrthographicOffCenterLH(
		0.0f, (float)window_width, (float)window_height, 0.0f, 0.0f, 1.0f);

	//�f�X�N���v�^�q�[�v�𐶐�
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NumDescriptors = spriteSRVCount;
	result = dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&spriteCommon.descHeap));
	//���������X�v���C�g���ʃf�[�^��Ԃ�
	return spriteCommon;
}

//�X�v���C�g�P�̍X�V
void SpriteUpdate(Sprite& sprite, const SpriteCommon& spriteCommon)
{
	//���[���h�s��̍X�V
	sprite.matWorld = XMMatrixIdentity();
	//Z����]
	sprite.matWorld *= XMMatrixRotationZ(XMConvertToRadians(sprite.rotation));
	//���s�ړ�
	sprite.matWorld *= XMMatrixTranslation(sprite.position.x, sprite.position.y, sprite.position.z);
	//�萔�o�b�t�@�̓]��
	ConstBufferData* constMap = nullptr;
	HRESULT result = sprite.constBuff->Map(0, nullptr, (void**)&constMap);
	constMap->mat = sprite.matWorld * spriteCommon.matProjection;
	constMap->color = sprite.color;
	sprite.constBuff->Unmap(0, nullptr);
}
//�X�v���C�g���ʃe�N�X�`���ǂݍ���
void SpriteCommonLoadTexture(SpriteCommon& spriteCommon, UINT texnumber, const wchar_t* filename, ID3D12Device* dev)
{
	//�ُ�Ȕԍ��̎w������o
	assert(texnumber <= spriteSRVCount - 1);

	HRESULT result;
	//WIC�e�N�X�`���̃��[�h
	TexMetadata metadata{};
	ScratchImage scratchImg{};

	result = LoadFromWICFile(
		L"Resources/1f914.png",
		WIC_FLAGS_NONE,
		&metadata, scratchImg
	);

	const Image* img = scratchImg.GetImage(0, 0, 0);

	//���\�[�X�ݒ�
	CD3DX12_RESOURCE_DESC texresDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		metadata.format,
		metadata.width,
		(UINT)metadata.height,
		(UINT16)metadata.arraySize,
		(UINT16)metadata.mipLevels
	);
	//�e�N�X�`���p�̃o�b�t�@�̐���
	//ComPtr<ID3D12Resource>texBuff = nullptr;

	spriteCommon.texBuff[texnumber];

	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0),
		D3D12_HEAP_FLAG_NONE,
		&texresDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&spriteCommon.texBuff[texnumber])
	);

	//�e�N�X�`���o�b�t�@�Ƀf�[�^�]��
	result = spriteCommon.texBuff[texnumber]->WriteToSubresource(
		0,
		nullptr,//�S�̈�փR�s�[
		img->pixels,//���f�[�^�A�h���X
		(UINT)img->rowPitch,//1���C���T�C�Y
		(UINT)img->slicePitch//�S�T�C�Y
	);

#pragma region �V�F�[�_���\�[�X�r���[�̍쐬
	//�V�F�[�_���\�[�X�r���[�ݒ�
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};//�ݒ�\����
	srvDesc.Format = metadata.format;//RGBA
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2D�e�N�X�`��
	srvDesc.Texture2D.MipLevels = 1;

	//�q�[�v��2�ԖڂɃV�F�[�_���\�[�X�r���[�쐬
	dev->CreateShaderResourceView(spriteCommon.texBuff[texnumber].Get(),//�r���[�Ɗ֘A�t����o�b�t�@
		&srvDesc,//�e�N�X�`���ݒ���
		CD3DX12_CPU_DESCRIPTOR_HANDLE(spriteCommon.descHeap->GetCPUDescriptorHandleForHeapStart(),texnumber,
			dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
		)
	);
#pragma endregion

}
LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wparm, LPARAM lparam)
{
	//���b�Z�[�W�ŕ���
	switch (msg)
	{
	case WM_DESTROY://�E�B���h�E���j�󂳂ꂽ
		PostQuitMessage(0);//OS�ɑ΂��āA�A�v���̏I����`����
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparm, lparam);//�W���̏������s��
}

//Windows�A�v���ł̃G���g���[�|�C���g(main�֐�)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	const int window_width = 1280;//����
	const int window_height = 720;//�c��

	WNDCLASSEX w{};
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProc;//�E�B���h�E�v���V�[�W����ݒ�
	w.lpszClassName = L"DirectXGame";//�E�B���h�E�N���X��
	w.hInstance = GetModuleHandle(nullptr);//�E�B���h�E�n���h��
	w.hCursor = LoadCursor(NULL, IDC_ARROW);//�J�[�\���w��

	//�E�B���h�E�N���X��OS�ɓo�^
	RegisterClassEx(&w);
	//�E�B���h�E�T�C�Y{X���W Y���W�@�����@�c��}
	RECT wrc = { 0,0,window_width,window_height };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);//�����ŃT�C�Y�␳

	//�E�B���h�E�I�u�W�F�N�g�̐���
	HWND hwnd = CreateWindow(w.lpszClassName,//�N���X��
		L"DirectXGame",//�^�C�g���o�[�̕���
		WS_OVERLAPPEDWINDOW,//�W���I�ȃE�B���h�E�X�^�C��
		CW_USEDEFAULT,//�\��X���W(OS�ɔC����)
		CW_USEDEFAULT,//�\��Y���W(OS�ɔC����)
		wrc.right - wrc.left,//�E�B���h�E����
		wrc.bottom - wrc.top,//�E�B���h�E�c��
		nullptr,//�e�E�B���h�E�n���h��
		nullptr,//���j���[�n���h��
		w.hInstance,//�Ăяo���A�v���P�[�V�����n���h��
		nullptr);//�I�v�V����

	ShowWindow(hwnd, SW_SHOW);

	MSG msg{};//���b�Z�[�W

	//DirectX�����������@��������
#ifdef _DEBUG
	//�f�o�b�O���C���[���I����
	ID3D12Debug* debugContoroller;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugContoroller))))
	{
		debugContoroller->EnableDebugLayer();
	}
#endif // DEBUG

	HRESULT result;
	ComPtr<ID3D12Device>dev;
	ComPtr<IDXGIFactory6>dxgiFactory;
	ComPtr<IDXGISwapChain4>swapchain;
	ComPtr<ID3D12CommandAllocator>cmdAllocator;
	ComPtr<ID3D12GraphicsCommandList>cmdList;
	ComPtr<ID3D12CommandQueue>cmdQueue;
	ComPtr<ID3D12DescriptorHeap>rtvHeaps;
	//DXGI�t�@�N�g���[�̐���
	result = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
	//�A�_�v�^�[�̗񋓗p
	//std::vector<IDXGIAdapter1*>adapters;
	std::vector<ComPtr<IDXGIAdapter1>>adapters;
	//�����ɓ���̖��O�����A�_�v�^�[�I�u�W�F�N�g������
	//IDXGIAdapter1* tmpAdapter = nullptr;
	ComPtr<IDXGIAdapter1>tmpAdapter;
	for (int i = 0; dxgiFactory->EnumAdapters1(i, &tmpAdapter) !=
		DXGI_ERROR_NOT_FOUND;
		i++)
	{
		adapters.push_back(tmpAdapter);//���I�z��ɒǉ�����
	}

	for (int i = 0; i < adapters.size(); i++) {
		DXGI_ADAPTER_DESC1 adesc;
		adapters[i]->GetDesc1(&adesc);//�A�_�v�^�[�̏����擾
		//�\�t�g�E�F�A�f�o�C�X�����
		if (adesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			continue;
		}

		std::wstring strDesc = adesc.Description;//�A�_�v�^�[��
		//Intel UHD Graphics(�I���|�[�g�O���t�B�b�N)�����
		if (strDesc.find(L"Intel") == std::wstring::npos)
		{
			tmpAdapter = adapters[i];
			break;
		}
	}
	//�Ή����x���̔z��
	D3D_FEATURE_LEVEL levels[] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	D3D_FEATURE_LEVEL featureLevel;

	for (int i = 0; i < _countof(levels); i++) {
		//�̗p�����A�_�v�^�Ńf�o�C�X�𐶐�
		result = D3D12CreateDevice(tmpAdapter.Get(), levels[i], IID_PPV_ARGS(&dev));
		if (result == S_OK) {
			//�f�o�C�X�𐶐��o�������_�Ń��[�v�𔲂���
			featureLevel = levels[i];
			break;
		}
	}

	//�R�}���h�A���P�[�^�𐶐�
	result = dev->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&cmdAllocator)
	);
	//�R�}���h���X�g�𐶐�
	result = dev->CreateCommandList(0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		cmdAllocator.Get(), nullptr,
		IID_PPV_ARGS(&cmdList));

	//�W���ݒ�ŃR�}���h�L���[�𐶐�
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc{};

	dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&cmdQueue));

	//�e��ݒ�����ăX���b�v�`�F�[���𐶐�
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
	swapchainDesc.Width = 1280;
	swapchainDesc.Height = 720;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;//�F���̏���
	swapchainDesc.SampleDesc.Count = 1;//�}���`�T���v�����Ȃ�
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;//�o�b�N�o�b�t�@�p
	swapchainDesc.BufferCount = 2;//�o�b�t�@����2�ɐݒ�
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;//�t���b�v��͔j��
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ComPtr<IDXGISwapChain1>swapchain1;

	dxgiFactory->CreateSwapChainForHwnd(
		cmdQueue.Get(),
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		&swapchain1
	);

	swapchain1.As(&swapchain);

	//�e��ݒ�����ăf�X�N���v�^�q�[�v�𐶐�
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;//�����_�[�^�[�Q�b�g�r���[
	heapDesc.NumDescriptors = 2;//���\��2��
	dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));

	//���\��2���ɂ���
	//std::vector<ID3D12Resource*>backBuffers(2);
	std::vector<ComPtr<ID3D12Resource>>backBuffers(2);
	for (int i = 0; i < 2; i++) {
		//�X���b�v�`�F�[������o�b�t�@���擾
		result = swapchain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i]));
		//�f�X�N���v�^�q�[�v�̃n���h�����擾
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeaps->GetCPUDescriptorHandleForHeapStart(),
			//�����\���A�h���X�������
			i, dev->GetDescriptorHandleIncrementSize(heapDesc.Type));
		//�����_�[�^�[�Q�b�g�r���[�̐���
		dev->CreateRenderTargetView(
			backBuffers[i].Get(),
			nullptr,
			CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeaps->GetCPUDescriptorHandleForHeapStart(),
				i,
				dev->GetDescriptorHandleIncrementSize(heapDesc.Type)
			)
		);
	}
	//�t�F���X�̐���
	ComPtr<ID3D12Fence>fence;
	UINT64 fenceVal = 0;

	result = dev->CreateFence(fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

	//IDirectInput8* dinput = nullptr;
	//result = DirectInput8Create(
	//	w.hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&dinput, nullptr);

	//IDirectInputDevice8* devkeyboard = nullptr;
	//result = dinput->CreateDevice(GUID_SysKeyboard, &devkeyboard, NULL);
	//result = devkeyboard->SetDataFormat(&c_dfDIKeyboard);//�W���`��
	//result = devkeyboard->SetCooperativeLevel(
	//	hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);

	Input* input = nullptr;

	input = new Input();
	input->Initialize(w.hInstance,hwnd);




	//DirectX�����������@�����܂�

	////�`�揉��������

	//WIC�e�N�X�`���̃��[�h
	TexMetadata metadata{};
	ScratchImage scratchImg{};

	result = LoadFromWICFile(
		L"Resources/mameneko.jpg",
		WIC_FLAGS_NONE,
		&metadata, scratchImg
	);

	const Image* img = scratchImg.GetImage(0, 0, 0);

	//���\�[�X�ݒ�
	CD3DX12_RESOURCE_DESC texresDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		metadata.format,
		metadata.width,
		(UINT)metadata.height,
		(UINT16)metadata.arraySize,
		(UINT16)metadata.mipLevels
	);
	//�e�N�X�`���p�̃o�b�t�@�̐���
	ComPtr<ID3D12Resource>texBuff = nullptr;
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0),
		D3D12_HEAP_FLAG_NONE,
		&texresDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&texBuff)
	);

	//�e�N�X�`���o�b�t�@�Ƀf�[�^�]��
	result = texBuff->WriteToSubresource(
		0,
		nullptr,//�S�̈�փR�s�[
		img->pixels,//���f�[�^�A�h���X
		(UINT)img->rowPitch,//1���C���T�C�Y
		(UINT)img->slicePitch//�S�T�C�Y
	);
	//���f�[�^���
	//delete[] imageData;

	const float topHeight = 5.0f;

	

	const int DIV = 3;
	const float radius = 3;
	const int  constantBufferNum = 128;




	Vertex vertices[DIV + 1 + 1];
	for (int i = 0; i < 3; i++) {
		vertices[i].pos.x = radius * sinf(2 * XM_PI / 3 * i) * 3;
		vertices[i].pos.y = radius * cosf(2 * XM_PI / 3 * i) * 3;
		vertices[i].pos.z = 0;
	}
	vertices[3].pos.x = 0;
	vertices[3].pos.y = 0;
	vertices[3].pos.z = 0;

	vertices[4].pos.x = 0;
	vertices[4].pos.y = 0;
	vertices[4].pos.z = -topHeight;

#pragma region ������
	//���_�f�[�^
	//Vertex vertices[] = {
	//	�O
	//	{{-5.0f,-5.0f, 5.0f},{}, {0.0f,1.0f}},//����
	//	{{ 5.0f,-5.0f, 5.0f},{}, {0.0f,0.0f}},//����
	//	{{-5.0f, 5.0f, 5.0f},{}, {1.0f,1.0f}},//�E��
	//	{{ 5.0f, 5.0f, 5.0f},{}, {1.0f,0.0f}},//�E��

	//	���
	//	{{-5.0f, 5.0f,-5.0f},{}, {0.0f,1.0f}},//����
	//	{{ 5.0f, 5.0f,-5.0f},{}, {0.0f,0.0f}},//����
	//	{{-5.0f,-5.0f,-5.0f},{}, {1.0f,1.0f}},//�E��
	//	{{ 5.0f,-5.0f,-5.0f},{}, {1.0f,0.0f}},//�E��

	//	��
	//	{{-5.0f, 5.0f, 5.0f},{}, {0.0f,1.0f}},//����
	//	{{-5.0f, 5.0f,-5.0f},{}, {0.0f,0.0f}},//����
	//	{{-5.0f,-5.0f, 5.0f},{}, {1.0f,1.0f}},//�E��
	//	{{-5.0f,-5.0f,-5.0f},{}, {1.0f,0.0f}},//�E��

	//	�E
	//	{{ 5.0f, -5.0f, 5.0f},{}, {0.0f,1.0f}},//����
	//	{{ 5.0f, -5.0f,-5.0f},{}, {0.0f,0.0f}},//����
	//	{{ 5.0f,  5.0f, 5.0f},{}, {1.0f,1.0f}},//�E��
	//	{{ 5.0f,  5.0f,-5.0f},{}, {1.0f,0.0f}},//�E��

	//	��
	//	{{ 5.0f,5.0f,-5.0f},{}, {0.0f,1.0f}},//����
	//	{{-5.0f,5.0f,-5.0f},{}, {0.0f,0.0f}},//����
	//	{{ 5.0f,5.0f, 5.0f},{}, {1.0f,1.0f}},//�E��
	//	{{-5.0f,5.0f, 5.0f},{}, {1.0f,0.0f}},//�E��

	//	��
	//	{{-5.0f,-5.0f,-5.0f},{}, {0.0f,1.0f}},//����
	//	{{ 5.0f,-5.0f,-5.0f},{}, {0.0f,0.0f}},//����
	//	{{-5.0f,-5.0f, 5.0f},{}, {1.0f,1.0f}},//�E��
	//	{{ 5.0f,-5.0f, 5.0f},{}, {1.0f,0.0f}},//�E��

	//	{ +0.5f, -0.5f, 0.0f}, // �E��
	//	{ -0.5f, +0.5f, 0.0f}, // ����
	//	{ +0.5f, +0.5f, 0.0f},
	//};
#pragma endregion

	unsigned short indices[3 * 3 * 2];
	indices[0] = 1;
	indices[1] = 0;
	indices[2] = 3;

	indices[3] = 2;
	indices[4] = 1;
	indices[5] = 3;

	indices[6] = 0;
	indices[7] = 2;
	indices[8] = 3;

	indices[9] = 0;
	indices[10] = 1;
	indices[11] = 4;

	indices[12] = 1;
	indices[13] = 2;
	indices[14] = 4;

	indices[15] = 2;
	indices[16] = 0;
	indices[17] = 4;

#pragma region �����̒��_���W

#pragma endregion

	for (int i = 0; i < 18 / 3; i++) {
		//�O�p�`�̃C���f�b�N�X�����o���āA�ꎞ�I�ȕϐ��ɓ����
		unsigned short indices0 = indices[i * 3 + 0];
		unsigned short indices1 = indices[i * 3 + 1];
		unsigned short indices2 = indices[i * 3 + 2];
		//�O�p�`�̂��\�����钸�_���W���x�N�g���ɑ��
		XMVECTOR p0 = XMLoadFloat3(&vertices[indices0].pos);
		XMVECTOR p1 = XMLoadFloat3(&vertices[indices1].pos);
		XMVECTOR p2 = XMLoadFloat3(&vertices[indices2].pos);
		//p0>p1�x�N�g���Ap0>p2�x�N�g�����v�Z(�x�N�g���̌��Z)
		XMVECTOR v1 = XMVectorSubtract(p1, p0);
		XMVECTOR v2 = XMVectorSubtract(p2, p0);
		//�O�ς͗������琂���ȃx�N�g��
		XMVECTOR normal = XMVector3Cross(v1, v2);
		//���K��
		normal = XMVector3Normalize(normal);
		//���߂��@���𒸓_�f�[�^�ɑ��
		XMStoreFloat3(&vertices[indices0].normal, normal);
		XMStoreFloat3(&vertices[indices1].normal, normal);
		XMStoreFloat3(&vertices[indices2].normal, normal);
	}


#pragma region ���_�f�[�^	

	//���_�f�[�^�S�̂̃T�C�Y=���_�f�[�^����̃T�C�Y*���_�f�[�^�̗v�f��
	UINT sizeVB = static_cast<UINT>(sizeof(Vertex) * _countof(vertices));
	//D3D12_HEAP_PROPERTIES heapprop{};   // �q�[�v�ݒ�
	//heapprop.Type = D3D12_HEAP_TYPE_UPLOAD; // GPU�ւ̓]���p

	//D3D12_RESOURCE_DESC resdesc{};  // ���\�[�X�ݒ�
	//resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	//resdesc.Width = (sizeof(ConstBufferData) + 0xff) & ~0xff; // ���_�f�[�^�S�̂̃T�C�Y
	//resdesc.Height = 1;
	//resdesc.DepthOrArraySize = 1;
	//resdesc.MipLevels = 1;
	//resdesc.SampleDesc.Count = 1;
	//resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	//// ���_�o�b�t�@�̐���
	//ComPtr<ID3D12Resource> vertBuff = nullptr;
	//result = dev->CreateCommittedResource(
	//	&heapprop, // �q�[�v�ݒ�
	//	D3D12_HEAP_FLAG_NONE,
	//	&resdesc, // ���\�[�X�ݒ�
	//	D3D12_RESOURCE_STATE_GENERIC_READ,
	//	nullptr,
	//	IID_PPV_ARGS(&vertBuff));

	SpriteCommon spriteCommon;

	spriteCommon = SpriteCommonCreate(dev.Get(), window_width, window_height);

	SpriteCommonLoadTexture(spriteCommon, 0, L"Resource/mameneko.jpg", dev.Get());

	Sprite sprite;

	sprite = SpriteCreate(dev.Get(), window_width, window_height);

	SpriteUpdate(sprite, spriteCommon);

	ComPtr<ID3D12Resource>vertBuff;
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeVB),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertBuff)
	);

	// GPU��̃o�b�t�@�ɑΉ��������z���������擾
	Vertex* vertMap = nullptr;
	result = vertBuff->Map(0, nullptr, (void**)&vertMap);

	// �S���_�ɑ΂���
	for (int i = 0; i < _countof(vertices); i++)
	{
		vertMap[i] = vertices[i];   // ���W���R�s�[
	}

	// �}�b�v������
	vertBuff->Unmap(0, nullptr);



#pragma endregion

#pragma region �C���f�b�N�X�f�[�^
	//�C���f�b�N�X�f�[�^�S�̂̃T�C�Y
	UINT sizeIB = static_cast<UINT>(sizeof(unsigned short) * _countof(indices));
	//�C���f�b�N�X�o�b�t�@�̐ݒ�
	//ComPtr<ID3D12Resource> indexBuff = nullptr;
	//resdesc.Width = sizeIB;//�C���f�b�N�X��񂪓��镪�̃T�C�Y
	////�C���f�b�N�X�o�b�t�@�̐���
	//result = dev->CreateCommittedResource(
	//	&heapprop,//�q�[�v�̐ݒ�
	//	D3D12_HEAP_FLAG_NONE,
	//	&resdesc,//���\�[�X�ݒ�
	//	D3D12_RESOURCE_STATE_GENERIC_READ,
	//	nullptr,
	//	IID_PPV_ARGS(&indexBuff)
	//);

	ComPtr<ID3D12Resource>indexBuff;
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeIB),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&indexBuff)
	);

	//GPU��̃o�b�t�@�ɑΉ��������z�������̎擾
	unsigned short* indexMap = nullptr;
	result = indexBuff->Map(0, nullptr, (void**)&indexMap);

	//�S�C���f�b�N�X�ɑ΂���
	for (int i = 0; i < _countof(indices); i++) {
		indexMap[i] = indices[i];//�C���f�b�N�X���R�s�[
	}
	//�Ȃ��������
	indexBuff->Unmap(0, nullptr);
#pragma endregion

	D3D12_INDEX_BUFFER_VIEW ibView{};
	ibView.BufferLocation = indexBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = sizeIB;

	// ���_�o�b�t�@�r���[�̍쐬
	D3D12_VERTEX_BUFFER_VIEW vbView{};

	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
	vbView.SizeInBytes = sizeVB;
	vbView.StrideInBytes = sizeof(Vertex);



	//�q�[�v�ݒ�
	D3D12_HEAP_PROPERTIES cbheapprop{};//�q�[�v�ݒ�
	cbheapprop.Type = D3D12_HEAP_TYPE_UPLOAD;//GPU�ւ̓]���p
	//���\�[�X�ݒ�
	D3D12_RESOURCE_DESC cbresdesc{};//���\�[�X�ݒ�
	cbresdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	cbresdesc.Width = (sizeof(ConstBufferData) + 0xff) & ~0xff;//256�o�C�g�A���C�������g
	cbresdesc.Height = 1;
	cbresdesc.DepthOrArraySize = 1;
	cbresdesc.MipLevels = 1;
	cbresdesc.SampleDesc.Count = 1;
	cbresdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;


	ComPtr<ID3D12Resource> depthBuffer;
	//�[�x�o�b�t�@���\�[�X�ݒ�
	CD3DX12_RESOURCE_DESC depthResDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_D32_FLOAT,
		window_width,
		window_height,
		1, 0,
		1, 0,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	//�[�x�o�b�t�@�̐ݒ�
	result = dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,//�[�x�l�������݂Ɏg�p
		&CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0),
		IID_PPV_ARGS(&depthBuffer)
	);

	//�[�x�r���[�p�f�X�N���v�^�q�[�v�쐬
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	ComPtr<ID3D12DescriptorHeap> dsvHeap = nullptr;
	result = dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));

	//�[�x�r���[�쐬
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dev->CreateDepthStencilView(
		depthBuffer.Get(),
		&dsvDesc,
		dsvHeap->GetCPUDescriptorHandleForHeapStart()
	);

	XMMATRIX matProjection = XMMatrixPerspectiveFovLH(
		XMConvertToRadians(60.0f),
		(float)window_width / window_height,
		0.1f, 1000.0f
	);
	//	constMap->mat = matProjection;
	//	//�r���[�ϊ��s��
	XMMATRIX matView;
	XMFLOAT3 eye(0, 0, -100);//���_���W
	XMFLOAT3 target(0, 0, 0);//�����_���W
	XMFLOAT3 up(0, 1, 0);//������x�N�g��
	matView = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));

	XMMATRIX enemyView;
	XMFLOAT3 eyes(0, 0, -100);//���_���W
	XMFLOAT3 targets(0, 0, 0);//�����_���W
	XMFLOAT3 ups(0, 1, 0);//������x�N�g��
	enemyView = XMMatrixLookAtLH(XMLoadFloat3(&eyes), XMLoadFloat3(&targets), XMLoadFloat3(&ups));

	XMMATRIX bulletView;
	XMFLOAT3 eyess(0, 0, -100);//���_���W
	XMFLOAT3 targetss(0, 0, 0);//�����_���W
	XMFLOAT3 upss(0, 1, 0);//������x�N�g��
	bulletView = XMMatrixLookAtLH(XMLoadFloat3(&eyess), XMLoadFloat3(&targetss), XMLoadFloat3(&upss));

	const int OBJECT_NUM = 1;

	const int OBJECT_ENEMY = 10;

	const int OBJECT_BULLET = 1;

	bool arrive[OBJECT_ENEMY];

	bool stanby[OBJECT_BULLET];

	

	Object3d object3ds;

	Object3d object3dsenemy[OBJECT_ENEMY];

	Object3d object3dsbullet[OBJECT_BULLET];

	//	//�萔�o�b�t�@�p�̃f�X�N���v�^�q�[�v
	ComPtr<ID3D12DescriptorHeap> basicDescHeap = nullptr;

	//�ݒ�\����
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//�V�F�[�_�[���猩����
	descHeapDesc.NumDescriptors = constantBufferNum + 1;//�萔�o�b�t�@�̐�
	//�f�X�N���v�^�q�[�v�̐���
	result = dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap));
	//for (int i = 0; i < _countof(object3ds); i++) {
		InitializeObject3d(&object3ds, 1, dev.Get(), basicDescHeap.Get());
		object3ds.position = { 0,0,0 };

	//}

	result = dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap));
	for (int i = 0; i < _countof(object3dsenemy); i++) {
		InitializeObject3d(&object3dsenemy[i], i, dev.Get(), basicDescHeap.Get());
		//�e�q�\��
		//if (i > 0) {
			//1�O�̃I�u�W�F�N�g��e�I�u�W�F�N�g�Ƃ���
			//object3dsenemy[i].parent = &object3dsenemy[i - 1];
			//�e�I�u�W�F�N�g��9����̑傫��
			object3dsenemy[i].scale = { 0.9f,0.9f,0.9f };
			//�e�I�u�W�F�N�g�ɑ΂��ĉ�]
			object3dsenemy[i].rotation = { 0.0f,0.0f,0.0f };
			//�e�I�u�W�F�N�g�ɑ΂��ĕ������炵
			object3dsenemy[i].position = { -50.0f+rand()%100+1,0.0f+rand()%100-1,100.0f+rand()%100+1 };
			
			arrive[i] = true;
		//}
	}

	object3dsenemy[1].position = { 0,0,0 };

	result = dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap));
	for (int i = 0; i < _countof(object3dsbullet); i++) {
		InitializeObject3d(&object3dsbullet[i], i, dev.Get(), basicDescHeap.Get());
		object3dsbullet[i].position.z = -1000;
		stanby[i] = false;
	}

	//�f�X�N���v�^�q�[�v�̐擪�n���h�����擾
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandleSRV =
		basicDescHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandleSRV =
		basicDescHeap->GetGPUDescriptorHandleForHeapStart();
	//�n���h���A�h���X��i�߂�
	cpuDescHandleSRV.ptr += constantBufferNum * dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	gpuDescHandleSRV.ptr += constantBufferNum * dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		//�V�F�[�_���\�[�X�r���[�쐬

#pragma region �V�F�[�_���\�[�X�r���[�̍쐬
	//�V�F�[�_���\�[�X�r���[�ݒ�
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};//�ݒ�\����
	srvDesc.Format = metadata.format;//RGBA
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2D�e�N�X�`��
	srvDesc.Texture2D.MipLevels = 1;

	//�f�X�N���v�^�̐擪�n���h�����擾
	D3D12_CPU_DESCRIPTOR_HANDLE basicHeaphandle2 = basicDescHeap->GetCPUDescriptorHandleForHeapStart();
	//�n���h���̃A�h���X��i�߂�
	basicHeaphandle2.ptr += 2 * dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//�q�[�v��2�ԖڂɃV�F�[�_���\�[�X�r���[�쐬
	dev->CreateShaderResourceView(texBuff.Get(),//�r���[�Ɗ֘A�t����o�b�t�@
		&srvDesc,//�e�N�X�`���ݒ���
		cpuDescHandleSRV
	);
#pragma endregion

	//D3D12_STATIC_SAMPLER_DESC samplerDesc{};

	PipelineSet object3dPipelineSet = object3dCreateGrphicsPipeline(dev.Get());
	
	PipelineSet spritePipeLineSet = SpriteCreateGraaphicsPipeline(dev.Get());

	float angle = 0.0f;//�J�����̉�]�p�x

	int reTime[OBJECT_ENEMY];

	//int arrive[OBJECT_ENEMY];

	XMFLOAT3 scale;//�X�P�[�����O
	XMFLOAT3 rotation;//��]
	XMFLOAT3 position;//���W

	scale = { 1.0f,1.0f,1.0f };
	rotation = { 0.0f,0.0f,0.0f };
	position = { 0.0f,0.0f,0.0f };

	float length = 0;

	while (true)//�Q�[�����[�v
	{
		//���b�Z�[�W������?
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);//�L�[���̓��b�Z�[�W�̏���
			DispatchMessage(&msg);//�v���V�[�W���Ƀ��b�Z�[�W�𑗂�
		}

		//x�{�^���ŏI�����b�Z�[�W��������Q�[�����[�v�𔲂���
		if (msg.message == WM_QUIT)
		{
			break;
		}
		//DirectX���t���[������ ��������
		input->Update();
		/*devkeyboard->Acquire();
		BYTE key[256] = {};
		result = devkeyboard->GetDeviceState(sizeof(key), key);*/

		//for (int i = 0; i < _countof(object3ds); i++) {
			UpdateObject3d(&object3ds, matView, matProjection);
		//}
			

			//�G
			for (int i = 0; i < _countof(object3dsenemy); i++) {
				UpdateObject3d(&object3dsenemy[i], enemyView, matProjection);
				if (arrive[i] == true) {
					object3dsenemy[i].position.z -= 0.5f;
					if (object3dsenemy[i].position.z <= -70) {
						object3dsenemy[i].position = { -50.0f + rand() % 100 + 1,50.0f + rand() % 50 - 50,300.0f + rand() % 100 + 1 };
						//arrive[i] = true;
					}
				}
				if (arrive[i] == false) {
					reTime[i]++;
					if (reTime[i] >= 500) {
						reTime[i] = 0;
						arrive[i] = true;
					}
				}
			}


		//�e
		for (int j = 0; j < _countof(object3dsbullet); j++) {
			UpdateObject3d(&object3dsbullet[j], bulletView, matProjection);
			if (stanby[j] == false) {
				if (input->TriggerKey(DIK_SPACE)) {
					//���W���킹
					object3dsbullet[j].position.z = object3ds.position.z;
					object3dsbullet[j].position.y = object3ds.position.y;
					object3dsbullet[j].position.x = object3ds.position.x;
					stanby[j] = true;
					break;
				}
			}
			if(stanby[j]==true)
			{
				//����
				object3dsbullet[j].position.z += 1.0f;
				if (object3dsbullet[j].position.z >= 200) {
					//object3dsbullet[j].position.z = -1000;
					stanby[j] = false;
				}

			}
		}
		

		//for (int i = 0; i < _countof(object3dsenemy); i++) {
		//	for (int j = 0;j < _countof(object3dsbullet); j++) {
		//		if (arrive[i] == true) {
		//			length = sqrtf((object3dsenemy[i].position.x - object3dsbullet[j].position.x) * (object3dsenemy[i].position.x - object3dsbullet[j].position.x)
		//			+ (object3dsenemy[i].position.y - object3dsbullet[j].position.y) * (object3dsenemy[i].position.y - object3dsbullet[j].position.y)
		//			+ (object3dsenemy[i].position.z - object3dsbullet[j].position.z) * (object3dsenemy[i].position.z - object3dsbullet[j].position.z));
		//			if (8 >= length) {
		//				//object3dsenemy[i].position = { -50.0f + rand() % 100 + 1,50.0f + rand() % 50 - 50,300.0f + rand() % 100 + 1 };
		//				stanby[j] = false;
		//				arrive[i] = false;
		//			}
		//		}
		//	}
		//}

		//�ړ�
		if (input->PushKey(DIK_UP) || input->PushKey(DIK_DOWN) || input->PushKey(DIK_RIGHT) || input->PushKey(DIK_LEFT)) {
			if (input->PushKey(DIK_UP)) { object3ds.position.y += 1.0f; }
			else if (input->PushKey(DIK_DOWN)) { object3ds.position.y -= 1.0f; }
			if (input->PushKey(DIK_RIGHT)) { object3ds.position.x += 1.0f; }
			else if (input->PushKey(DIK_LEFT)) { object3ds.position.x -= 1.0f; }
		}


		/*if (key[DIK_D] || key[DIK_A])
		{
			if (key[DIK_D]) { object3ds.position.z += 1.0f; }
			else if (key[DIK_A]) { object3ds.position.z -= 1.0f; }
		}*/

		//�o�b�N�o�b�t�@�̔ԍ����擾(2�Ȃ̂�0�Ԃ�1��)
		UINT bbIndex =
			swapchain->GetCurrentBackBufferIndex();
#pragma region 1.���\�[�X�o���A
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffers[bbIndex].Get(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
#pragma endregion

#pragma region 2.�`��w��
		//2.�`���w��
			//�����_�[�^�[�Q�b�g�r���[�p�f�B�X�N���v�^�q�[�v�̃n���h�����擾
		D3D12_CPU_DESCRIPTOR_HANDLE rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += bbIndex * dev->GetDescriptorHandleIncrementSize(heapDesc.Type);
		//cmdList->OMSetRenderTargets(1, &rtvH, false, nullptr);
		D3D12_CPU_DESCRIPTOR_HANDLE dsvH = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		cmdList->OMSetRenderTargets(1, &rtvH, false, &dsvH);
#pragma endregion

#pragma region 3.��ʃN���A
		//3.��ʃN���A
		float clearColor[] = { 0.0f,0.5f,0.8f,0.0f };//���ۂ��F
		/*if (key[DIK_SPACE]) {
			clearColor[0] = { 1.0f };
			clearColor[2] = { 0.0f };
		}*/
		cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
#pragma endregion

#pragma region 4.�`��R�}���h
		////4.�`��R�}���h��������
		//cmdList->IASetIndexBuffer(&ibView);
		cmdList->SetPipelineState(object3dPipelineSet.pipelinestate.Get());
		cmdList->SetGraphicsRootSignature(object3dPipelineSet.rootsignature.Get());
		//�f�X�N���v�^�q�[�v���Z�b�g
		ID3D12DescriptorHeap* ppHeaps[] = { basicDescHeap.Get() };
		cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		//�萔�o�b�t�@�r���[���Z�b�g
		D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandle0;
		D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandle1;
		D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandle2;

		D3D12_VIEWPORT viewport{};

		viewport.Width = window_width;
		viewport.Height = window_height;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		cmdList->RSSetViewports(1, &CD3DX12_VIEWPORT(0.0f, 0.0f, window_width, window_height));

		D3D12_RECT scissorrect{};

		scissorrect.left = 0;//�؂蔲�����W��
		scissorrect.right = scissorrect.left + window_width;//�؂蔲�����W�E
		scissorrect.top = 0;//�؂蔲�����W��
		scissorrect.bottom = scissorrect.top + window_height;//�؂蔲�����W��

		cmdList->RSSetScissorRects(1, &CD3DX12_RECT(0, 0, window_width, window_height));

		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		cmdList->IASetVertexBuffers(0, 1, &vbView);

		//for (int i = 0; i < _countof(object3ds); i++) {
			DrawObject3d(&object3ds, cmdList.Get(), basicDescHeap.Get(), vbView, ibView, gpuDescHandleSRV, _countof(indices));
		//}

		for (int i = 0; i < _countof(object3dsenemy); i++) {
			if (arrive[i]==true) {
				DrawObject3d(&object3dsenemy[i], cmdList.Get(), basicDescHeap.Get(), vbView, ibView, gpuDescHandleSRV, _countof(indices));
			}
		}

		for (int j = 0; j < _countof(object3dsbullet); j++) {
			if (stanby[j] == true) {
				DrawObject3d(&object3dsbullet[j], cmdList.Get(), basicDescHeap.Get(), vbView, ibView, gpuDescHandleSRV, _countof(indices));
			}
		}
		//�X�v���C�g���ʃR�}���h
		SpriteCommonBeginDraw(cmdList.Get(), spriteCommon);
		//�X�v���C�g�`��
		SpriteDraw(sprite, cmdList.Get(),spriteCommon,dev.Get());

		//4.�`��R�}���h�����܂�
#pragma endregion
		//5.���\�[�X�o���A��߂�
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffers[bbIndex].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
		//���߂̃N���[�Y
		cmdList->Close();
		//�R�}���h���X�g�̎��s
		ID3D12CommandList* cmdLists[] = { cmdList.Get() };//�R�}���h���X�g�̔z��
		cmdQueue->ExecuteCommandLists(1, cmdLists);
		//�R�}���h���X�g�̎��s����������
		cmdQueue->Signal(fence.Get(), ++fenceVal);
		if (fence->GetCompletedValue() != fenceVal)
		{
			HANDLE event = CreateEvent(nullptr, false, false, nullptr);
			fence->SetEventOnCompletion(fenceVal, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}

		cmdAllocator->Reset();//�L���[���N���A
		cmdList->Reset(cmdAllocator.Get(), nullptr);//�ĂуR�}���h���X�g�𒙂߂鏀��

		//�o�b�t�@���t���b�v(���\�̓���ւ�)
		swapchain->Present(1, 0);
		//DirectX���t���[�������@�����܂�
	}

	UnregisterClass(w.lpszClassName, w.hInstance);

	//�E�B���h�E�\��
	ShowWindow(hwnd, SW_SHOW);
	//�R���\�[���ւ̕����o��
	OutputDebugStringA("Hello,DirectX!!\n");

	delete input;

	return 0;
}


