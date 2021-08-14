#pragma once
#include<Windows.h>
#include<tchar.h>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<DirectXMath.h>
#include<vector>
#include<map>
#include<d3dcompiler.h>
#include<DirectXTex.h>
#include<d3dx12.h>
#include<wrl.h>
#include<memory>

class Dx12Wrapper;
class PMDRenderer;
class PMDActor;
///�V���O���g���N���X
class Application
{
private:
	//�����ɕK�v�ȕϐ�(�o�b�t�@��q�[�v�Ȃ�)������
	//�E�B���h�E����
	WNDCLASSEX _windowClass;
	HWND _hwnd;
	std::shared_ptr<Dx12Wrapper> _dx12;
	std::shared_ptr<PMDRenderer> _pmdRenderer;
	std::shared_ptr<PMDActor> _pmdActor;

	//�Q�[���p�E�B���h�E�̐���
	void CreateGameWindow(HWND &hwnd, WNDCLASSEX &windowClass);

	//���V���O���g���̂��߂ɃR���X�g���N�^��private��
	//����ɃR�s�[�Ƒ�����֎~��
	Application();
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;
public:
	///Application�̃V���O���g���C���X�^���X�𓾂�
	static Application& Instance();

	///������
	bool Init();

	///���[�v�N��
	void Run();

	///�㏈��
	void Terminate();
	SIZE GetWindowSize()const;
	~Application();
};

