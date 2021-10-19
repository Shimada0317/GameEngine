#pragma once
#include<Windows.h>
#include<wrl.h>
#define DIRECTINPUT_VERSION 0x0800
#include<dinput.h>


class Input
{
public:
	//namespace�ȗ�
	template<class T>using ComPtr = Microsoft::WRL::ComPtr<T>;
public://�����o�֐�
	//������
	void Initialize(HINSTANCE hInstance,HWND hwnd);
	//�X�V
	void Update();

	bool PushKey(BYTE keyNumber);

	bool TriggerKey(BYTE keyNumber);
private:
	ComPtr<IDirectInputDevice8> devkeyboard;

	BYTE key[256] = {};
	BYTE keyPre[256] = {};

	//2-3��P25
};