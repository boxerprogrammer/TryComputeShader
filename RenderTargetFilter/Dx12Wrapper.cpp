#include "Dx12Wrapper.h"
#include<cassert>
#include<d3dx12.h>
#include"Application.h"

#pragma comment(lib,"DirectXTex.lib")
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;

namespace {
	///モデルのパスとテクスチャのパスから合成パスを得る
	///@param modelPath アプリケーションから見たpmdモデルのパス
	///@param texPath PMDモデルから見たテクスチャのパス
	///@return アプリケーションから見たテクスチャのパス
	std::string GetTexturePathFromModelAndTexPath(const std::string& modelPath, const char* texPath) {
		//ファイルのフォルダ区切りは\と/の二種類が使用される可能性があり
		//ともかく末尾の\か/を得られればいいので、双方のrfindをとり比較する
		//int型に代入しているのは見つからなかった場合はrfindがepos(-1→0xffffffff)を返すため
		int pathIndex1 = modelPath.rfind('/');
		int pathIndex2 = modelPath.rfind('\\');
		auto pathIndex = max(pathIndex1, pathIndex2);
		auto folderPath = modelPath.substr(0, pathIndex + 1);
		return folderPath + texPath;
	}

	///ファイル名から拡張子を取得する
	///@param path 対象のパス文字列
	///@return 拡張子
	string
		GetExtension(const std::string& path) {
		int idx = path.rfind('.');
		return path.substr(idx + 1, path.length() - idx - 1);
	}

	///ファイル名から拡張子を取得する(ワイド文字版)
	///@param path 対象のパス文字列
	///@return 拡張子
	wstring
		GetExtension(const std::wstring& path) {
		int idx = path.rfind(L'.');
		return path.substr(idx + 1, path.length() - idx - 1);
	}

	///テクスチャのパスをセパレータ文字で分離する
	///@param path 対象のパス文字列
	///@param splitter 区切り文字
	///@return 分離前後の文字列ペア
	pair<string, string>
		SplitFileName(const std::string& path, const char splitter = '*') {
		int idx = path.find(splitter);
		pair<string, string> ret;
		ret.first = path.substr(0, idx);
		ret.second = path.substr(idx + 1, path.length() - idx - 1);
		return ret;
	}

	///string(マルチバイト文字列)からwstring(ワイド文字列)を得る
	///@param str マルチバイト文字列
	///@return 変換されたワイド文字列
	std::wstring
		GetWideStringFromString(const std::string& str) {
		//呼び出し1回目(文字列数を得る)
		auto num1 = MultiByteToWideChar(CP_ACP,
			MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
			str.c_str(), -1, nullptr, 0);

		std::wstring wstr;//stringのwchar_t版
		wstr.resize(num1);//得られた文字列数でリサイズ

		//呼び出し2回目(確保済みのwstrに変換文字列をコピー)
		auto num2 = MultiByteToWideChar(CP_ACP,
			MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
			str.c_str(), -1, &wstr[0], num1);

		assert(num1 == num2);//一応チェック
		return wstr;
	}
	///デバッグレイヤーを有効にする
	void EnableDebugLayer() {
		ComPtr<ID3D12Debug> debugLayer = nullptr;
		auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
		debugLayer->EnableDebugLayer();
	}
	///アライメントに揃えたサイズを返す
	///@param size 元のサイズ
	///@param alignment アライメントサイズ
	///@return アライメントをそろえたサイズ
	size_t	AlignmentedSize(size_t size, size_t alignment) {
		return size + alignment - size % alignment;
	}
	/// <summary>
	/// シェーダエラーが起きたときのErrorBlobを出力する
	/// </summary>
	/// <param name="errBlob">errBlob</param>
	void OutputFromErrorBlob(ID3DBlob* errBlob)
	{
		if (errBlob != nullptr) {
			std::string errStr = "";
			auto errSize = errBlob->GetBufferSize();
			errStr.resize(errSize);
			std::copy_n((char*)errBlob->GetBufferPointer(), errSize, errStr.begin());
			OutputDebugStringA(errStr.c_str());
			errBlob->Release();
		}
	}
}

HRESULT
Dx12Wrapper::CreateOffscreenRTBuffer() {
	auto& b=backBuffers_[0];//もともとのバックバッファを取得
	auto bbDesc=b->GetDesc();
	HRESULT result = S_OK;
	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC resDesc = {};

	resDesc = bbDesc;
	D3D12_CLEAR_VALUE clearValue = { DXGI_FORMAT_R8G8B8A8_UNORM ,{ 1.0f,1.0f,1.0f,1.0f } };
	result = dev_->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_RENDER_TARGET,&clearValue,
		IID_PPV_ARGS(&offscreenRTBuffer_));
	assert(SUCCEEDED(result));

	auto rtvHeapDesc=rtvHeaps_->GetDesc();
	rtvHeapDesc.NumDescriptors = 1;
	result = dev_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeapOffscreen_));
	assert(SUCCEEDED(result));

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;
	dev_->CreateRenderTargetView(offscreenRTBuffer_, &rtvDesc,rtvHeapOffscreen_->GetCPUDescriptorHandleForHeapStart());


	return result;
}

Dx12Wrapper::Dx12Wrapper(HWND hwnd){
#ifdef _DEBUG
	//デバッグレイヤーをオンに
	EnableDebugLayer();
#endif

	auto& app=Application::Instance();
	_winSize = app.GetWindowSize();

	//DirectX12関連初期化
	if (FAILED(InitializeDXGIDevice())) {
		assert(0);
		return;
	}
	if (FAILED(InitializeCommand())) {
		assert(0);
		return;
	}
	if (FAILED(CreateSwapChain(hwnd))) {
		assert(0);
		return;
	}
	if (FAILED(CreateFinalRenderTargets())) {
		assert(0);
		return;
	}

	if (FAILED(CreateSceneView())) {
		assert(0);
		return;
	}

	if (FAILED(CreateOffscreenRTBuffer())) {
		assert(0);
		return;
	}

	
	//テクスチャローダー関連初期化
	CreateTextureLoaderTable();



	//深度バッファ作成
	if (FAILED(CreateDepthStencilView())) {
		assert(0);
		return ;
	}

	//フェンスの作成
	if (FAILED(dev_->CreateFence(fenceVal_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.ReleaseAndGetAddressOf())))) {
		assert(0);
		return ;
	}
	//ここコンピュートシェーダ関連
	UINT64 fenceVal = 0;
	if (FAILED(dev_->CreateFence(fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&computeFence_)))) {
		assert(0);
		return;
	}

	uavDescriptorHeap_ = CreateUAVDescriptorHeap();
	rootSignatureCS_ = CreateRootSignatureForComputeShader();
	pipelineCS_ = CreateComputePipeline(rootSignatureCS_);
	auto result = CreateUAVBuffer(dev_.Get(), uavResource_, offscreenRTBuffer_->GetDesc());
	CreateComputeViews(offscreenRTBuffer_, uavResource_, uavDescriptorHeap_);

}

HRESULT 
Dx12Wrapper::CreateDepthStencilView() {
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	auto result = swapchain_->GetDesc1(&desc);
	//深度バッファ作成
	//深度バッファの仕様
	//auto depthResDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT,
	//	desc.Width, desc.Height,
	//	1, 0, 1, 0,
	//	D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);


	D3D12_RESOURCE_DESC resdesc = {};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resdesc.DepthOrArraySize = 1;
	resdesc.Width = desc.Width;
	resdesc.Height = desc.Height;
	resdesc.Format = DXGI_FORMAT_D32_FLOAT;
	resdesc.SampleDesc.Count = 1;
	resdesc.SampleDesc.Quality = 0;
	resdesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resdesc.MipLevels = 1;
	resdesc.Alignment = 0;

	//デプス用ヒーププロパティ
	auto depthHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	CD3DX12_CLEAR_VALUE depthClearValue(DXGI_FORMAT_D32_FLOAT, 1.0f, 0);

	result = dev_->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, //デプス書き込みに使用
		&depthClearValue,
		IID_PPV_ARGS(depthBuffer_.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		//エラー処理
		return result;
	}

	//深度のためのデスクリプタヒープ作成
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};//深度に使うよという事がわかればいい
	dsvHeapDesc.NumDescriptors = 1;//深度ビュー1つのみ
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;//デプスステンシルビューとして使う
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;


	result = dev_->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(dsvHeap_.ReleaseAndGetAddressOf()));

	//深度ビュー作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;//デプス値に32bit使用
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;//フラグは特になし
	dev_->CreateDepthStencilView(depthBuffer_.Get(), &dsvDesc, dsvHeap_->GetCPUDescriptorHandleForHeapStart());
}


Dx12Wrapper::~Dx12Wrapper()
{
}


ComPtr<ID3D12Resource>
Dx12Wrapper::GetTextureByPath(const char* texpath) {
	auto it = textureTable_.find(texpath);
	if (it != textureTable_.end()) {
		//テーブルに内にあったらロードするのではなくマップ内の
		//リソースを返す
		return textureTable_[texpath];
	}
	else {
		return ComPtr<ID3D12Resource>(CreateTextureFromFile(texpath));
	}

}

//テクスチャローダテーブルの作成
void 
Dx12Wrapper::CreateTextureLoaderTable() {
	loadLambdaTable_["sph"] = loadLambdaTable_["spa"] = loadLambdaTable_["bmp"] = loadLambdaTable_["png"] = loadLambdaTable_["jpg"] = [](const wstring& path, TexMetadata* meta, ScratchImage& img)->HRESULT {
		return LoadFromWICFile(path.c_str(), WIC_FLAGS_NONE, meta, img);
	};

	loadLambdaTable_["tga"] = [](const wstring& path, TexMetadata* meta, ScratchImage& img)->HRESULT {
		return LoadFromTGAFile(path.c_str(), meta, img);
	};

	loadLambdaTable_["dds"] = [](const wstring& path, TexMetadata* meta, ScratchImage& img)->HRESULT {
		return LoadFromDDSFile(path.c_str(),DDS_FLAGS_NONE, meta, img);
	};
}
//テクスチャ名からテクスチャバッファ作成、中身をコピー
ID3D12Resource* 
Dx12Wrapper::CreateTextureFromFile(const char* texpath) {
	string texPath = texpath;
	//テクスチャのロード
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};
	auto wtexpath = GetWideStringFromString(texPath);//テクスチャのファイルパス
	auto ext = GetExtension(texPath);//拡張子を取得
	auto result = loadLambdaTable_[ext](wtexpath,
		&metadata,
		scratchImg);
	if (FAILED(result)) {
		return nullptr;
	}
	auto img = scratchImg.GetImage(0, 0, 0);//生データ抽出

	//WriteToSubresourceで転送する用のヒープ設定
	auto texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);
	auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(metadata.format, metadata.width, metadata.height, metadata.arraySize, metadata.mipLevels);

	ID3D12Resource* texbuff = nullptr;
	result = dev_->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,//特に指定なし
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&texbuff)
	);

	if (FAILED(result)) {
		return nullptr;
	}
	result = texbuff->WriteToSubresource(0,
		nullptr,//全領域へコピー
		img->pixels,//元データアドレス
		img->rowPitch,//1ラインサイズ
		img->slicePitch//全サイズ
	);
	if (FAILED(result)) {
		return nullptr;
	}

	return texbuff;
}

HRESULT
Dx12Wrapper::InitializeDXGIDevice() {
	UINT flagsDXGI = 0;
	flagsDXGI |= DXGI_CREATE_FACTORY_DEBUG;
	auto result = CreateDXGIFactory2(flagsDXGI, IID_PPV_ARGS(dxgiFactory_.ReleaseAndGetAddressOf()));
	//DirectX12まわり初期化
	//フィーチャレベル列挙
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	if (FAILED(result)) {
		return result;
	}
	std::vector <IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = nullptr;
	for (int i = 0; dxgiFactory_->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		adapters.push_back(tmpAdapter);
	}
	for (auto adpt : adapters) {
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc(&adesc);
		std::wstring strDesc = adesc.Description;
		if (strDesc.find(L"NVIDIA") != std::string::npos) {
			tmpAdapter = adpt;
			break;
		}
	}
	result = S_FALSE;
	//Direct3Dデバイスの初期化
	D3D_FEATURE_LEVEL featureLevel;
	for (auto l : levels) {
		if (SUCCEEDED(D3D12CreateDevice(tmpAdapter, l, IID_PPV_ARGS(dev_.ReleaseAndGetAddressOf())))) {
			featureLevel = l;
			result = S_OK;
			break;
		}
	}
	return result;
}

///スワップチェイン生成関数
HRESULT
Dx12Wrapper::CreateSwapChain(const HWND& hwnd) {
	RECT rc = {};
	::GetWindowRect(hwnd, &rc);

	
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
	swapchainDesc.Width = _winSize.cx;
	swapchainDesc.Height = _winSize.cy;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchainDesc.BufferCount = 2;
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;


	auto result= dxgiFactory_->CreateSwapChainForHwnd(cmdQueue_.Get(),
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)swapchain_.ReleaseAndGetAddressOf());
	assert(SUCCEEDED(result));
	return result;
}

//コマンドまわり初期化
HRESULT 
Dx12Wrapper::InitializeCommand() {
	auto result = dev_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(cmdAllocator_.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(0);
		return result;
	}
	result = dev_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocator_.Get(), nullptr, IID_PPV_ARGS(cmdList_.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(0);
		return result;
	}

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;//タイムアウトなし
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;//プライオリティ特に指定なし
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;//ここはコマンドリストと合わせてください
	result = dev_->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(cmdQueue_.ReleaseAndGetAddressOf()));//コマンドキュー生成
	assert(SUCCEEDED(result));

	//コンぴゅーとコマンド生成
	if (!CreateComputeCommand(computeCmdQue_, computeCmdAlloc_, computeCmdList_, nullptr)) {
		return E_FAIL;
	}

	return result;
}

//ビュープロジェクション用ビューの生成
HRESULT 
Dx12Wrapper::CreateSceneView(){
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	auto result = swapchain_->GetDesc1(&desc);
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(SceneData) + 0xff) & ~0xff);
	//定数バッファ作成
	result = dev_->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(sceneConstBuff_.ReleaseAndGetAddressOf())
	);

	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	mappedSceneData_ = nullptr;//マップ先を示すポインタ
	result = sceneConstBuff_->Map(0, nullptr, (void**)&mappedSceneData_);//マップ
	
	XMFLOAT3 eye(0, 15, -300);
	XMFLOAT3 target(0, 15, 0);
	XMFLOAT3 up(0, 1, 0);
	mappedSceneData_->view = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
	mappedSceneData_->proj = XMMatrixPerspectiveFovLH(XM_PIDIV4,//画角は45°
		static_cast<float>(desc.Width) / static_cast<float>(desc.Height),//アス比
		0.1f,//近い方
		1000.0f//遠い方
	);						
	mappedSceneData_->eye = eye;
	
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//シェーダから見えるように
	descHeapDesc.NodeMask = 0;//マスクは0
	descHeapDesc.NumDescriptors = 1;//
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;//デスクリプタヒープ種別
	result = dev_->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(sceneDescHeap_.ReleaseAndGetAddressOf()));//生成

	////デスクリプタの先頭ハンドルを取得しておく
	auto heapHandle = sceneDescHeap_->GetCPUDescriptorHandleForHeapStart();
	
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = sceneConstBuff_->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = sceneConstBuff_->GetDesc().Width;
	//定数バッファビューの作成
	dev_->CreateConstantBufferView(&cbvDesc, heapHandle);
	return result;

}

HRESULT	
Dx12Wrapper::CreateFinalRenderTargets() {
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	auto result = swapchain_->GetDesc1(&desc);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;//レンダーターゲットビューなので当然RTV
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;//表裏の２つ
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;//特に指定なし

	result = dev_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(rtvHeaps_.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		SUCCEEDED(result);
		return result;
	}
	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	result = swapchain_->GetDesc(&swcDesc);
	backBuffers_.resize(swcDesc.BufferCount);

	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps_->GetCPUDescriptorHandleForHeapStart();

	//SRGBレンダーターゲットビュー設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	for (int i = 0; i < swcDesc.BufferCount; ++i) {
		result = swapchain_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i]));
		assert(SUCCEEDED(result));
		rtvDesc.Format = backBuffers_[i]->GetDesc().Format;
		dev_->CreateRenderTargetView(backBuffers_[i], &rtvDesc, handle);
		handle.ptr += dev_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}
	viewport_.reset(new CD3DX12_VIEWPORT(backBuffers_[0]));
	scissorrect_.reset(new CD3DX12_RECT(0, 0, desc.Width, desc.Height));
	return result;
}

ComPtr< ID3D12Device> 
Dx12Wrapper::Device() {
	return dev_;
}
ComPtr < ID3D12GraphicsCommandList> 
Dx12Wrapper::CommandList() {
	return cmdList_;
}

void 
Dx12Wrapper::Update() {

}

void
Dx12Wrapper::BeginDraw() {
	//DirectX処理
	//バックバッファのインデックスを取得
	auto bbIdx = swapchain_->GetCurrentBackBufferIndex();
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffers_[bbIdx],
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	cmdList_->ResourceBarrier(1,&barrier);


	//レンダーターゲットを指定
	auto rtvH = rtvHeapOffscreen_->GetCPUDescriptorHandleForHeapStart();//rtvHeaps_->GetCPUDescriptorHandleForHeapStart();
//	rtvH.ptr += bbIdx * dev_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	//深度を指定
	auto dsvH = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
	cmdList_->OMSetRenderTargets(1, &rtvH, false, &dsvH);
	cmdList_->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);


	//画面クリア
	float clearColor[] = { 1.0f,1.0f,1.0f,1.0f };//白色
	cmdList_->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

	//ビューポート、シザー矩形のセット
	cmdList_->RSSetViewports(1, viewport_.get());
	cmdList_->RSSetScissorRects(1, scissorrect_.get());


}

void 
Dx12Wrapper::SetScene() {
	//現在のシーン(ビュープロジェクション)をセット
	ID3D12DescriptorHeap* sceneheaps[] = { sceneDescHeap_.Get() };
	cmdList_->SetDescriptorHeaps(1, sceneheaps);
	cmdList_->SetGraphicsRootDescriptorTable(0, sceneDescHeap_->GetGPUDescriptorHandleForHeapStart());

}

void
Dx12Wrapper::EndDraw() {
	auto bbIdx = swapchain_->GetCurrentBackBufferIndex();
	{
		//命令のクローズ
		cmdList_->Close();
		//ここで一旦レンダーターゲットに書き終わるまで待ち
			//コマンドリストの実行
		ID3D12CommandList* cmdlists[] = { cmdList_.Get() };
		cmdQueue_->ExecuteCommandLists(1, cmdlists);
		////待ち
		cmdQueue_->Signal(fence_.Get(), ++fenceVal_);

		if (fence_->GetCompletedValue() < fenceVal_) {
			auto event = CreateEvent(nullptr, false, false, nullptr);
			fence_->SetEventOnCompletion(fenceVal_, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}
		cmdAllocator_->Reset();//キューをクリア
		cmdList_->Reset(cmdAllocator_.Get(), nullptr);//再びコマンドリストをためる準備
	}

	//コンピュートシェーダ用処理
	{
		computeCmdList_->SetPipelineState(pipelineCS_);
		//レンダリング結果を元にUAVに書き込み
		computeCmdList_->SetComputeRootSignature(rootSignatureCS_);//ルートシグネチャセット
		ID3D12DescriptorHeap* descHeaps[] = { uavDescriptorHeap_ };
		computeCmdList_->SetDescriptorHeaps(1, descHeaps);//ディスクリプタヒープのセット
		computeCmdList_->SetComputeRootDescriptorTable(0,
			uavDescriptorHeap_->GetGPUDescriptorHandleForHeapStart()
		);//ルートパラメータのセット
		auto uavDesc = uavResource_->GetDesc();
		//画像サイズ/スレッド数でディスパッチ
		computeCmdList_->Dispatch(uavDesc.Width, uavDesc.Height, 1);

		computeCmdList_->Close();
		ExecuteAndWait(computeCmdQue_, computeCmdList_, computeFence_, ++fenceVal_);
		computeCmdAlloc_->Reset();//キューをクリア
		computeCmdList_->Reset(computeCmdAlloc_,pipelineCS_);//再びコマンドリストをためる準備
	}


	{
		CopyRenderTarget(uavResource_, backBuffers_[bbIdx]);

		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffers_[bbIdx],
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		cmdList_->ResourceBarrier(1,
			&barrier);

		//命令のクローズ
		cmdList_->Close();



		//コマンドリストの実行
		ID3D12CommandList* cmdlists[] = { cmdList_.Get() };
		cmdQueue_->ExecuteCommandLists(1, cmdlists);
		////待ち
		cmdQueue_->Signal(fence_.Get(), ++fenceVal_);

		if (fence_->GetCompletedValue() < fenceVal_) {
			auto event = CreateEvent(nullptr, false, false, nullptr);
			fence_->SetEventOnCompletion(fenceVal_, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}
		cmdAllocator_->Reset();//キューをクリア
		cmdList_->Reset(cmdAllocator_.Get(), nullptr);//再びコマンドリストをためる準備
	}
}

ComPtr < IDXGISwapChain4> 
Dx12Wrapper::Swapchain() {
	return swapchain_;
}

//ここからコンピュートシェーダ用
/// <summary>
/// UAV書き込みバッファを作成する(最終出力先)
/// </summary>
/// <param name="dev">デバイスオブジェクト</param>
/// <param name="res">計算リソース(返り値用)</param>
/// <returns>result</returns>
HRESULT 
Dx12Wrapper::CreateUAVBuffer(ID3D12Device* dev, ID3D12Resource*& res, const D3D12_RESOURCE_DESC& desc) {
	HRESULT result = S_OK;
	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS ;
	resDesc.Format = desc.Format;
	resDesc.Width = desc.Width;
	resDesc.DepthOrArraySize = 1;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Height = desc.Height;
	resDesc.MipLevels = desc.MipLevels;
	resDesc.SampleDesc.Count = 1;
	resDesc.Layout = desc.Layout;
	result = dev->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
		IID_PPV_ARGS(&res));
	assert(SUCCEEDED(result));
	return result;
}

ID3D12RootSignature* 
Dx12Wrapper::CreateRootSignatureForComputeShader()
{
	HRESULT result = S_OK;
	ID3DBlob* errBlob = nullptr;
	D3D12_DESCRIPTOR_RANGE range[2] = {};
	range[0].NumDescriptors = 1;//1つ
	range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;//u
	range[0].BaseShaderRegister = 0;//u0
	range[0].OffsetInDescriptorsFromTableStart = 0;
	range[0].RegisterSpace = 0;

	range[1].NumDescriptors = 1;//1つ
	range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//t
	range[1].BaseShaderRegister = 0;//t0
	range[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	range[1].RegisterSpace = 0;

	D3D12_ROOT_PARAMETER rp[1] = {};
	rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rp[0].DescriptorTable.NumDescriptorRanges = 2;
	rp[0].DescriptorTable.pDescriptorRanges = range;

	D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.NumParameters = 1;
	rootSigDesc.pParameters = rp;
	rootSigDesc.NumStaticSamplers = 0;
	rootSigDesc.pStaticSamplers = nullptr;
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT;


	ID3DBlob* rootSigBlob = nullptr;
	D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errBlob);
	ID3D12RootSignature* rootSignature = nullptr;
	result = dev_->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	rootSigBlob->Release();
	assert(SUCCEEDED(result));
	return rootSignature;
}

/// <summary>
/// コンピュートシェーダのロード
/// </summary>
/// <returns>コンピュートシェーダBlob</returns>
ID3DBlob* 
Dx12Wrapper::LoadComputeShader()
{
	ID3DBlob* csBlob = nullptr;
	ID3DBlob* errBlob = nullptr;
	auto result = D3DCompileFromFile(L"FilterCS.hlsl", nullptr, nullptr, "MonoCS", "cs_5_1", 0, 0, &csBlob, &errBlob);
	if (errBlob != nullptr) {
		OutputFromErrorBlob(errBlob);
	}
	assert(SUCCEEDED(result));
	return csBlob;
}

ID3D12PipelineState* 
Dx12Wrapper::CreateComputePipeline(ID3D12RootSignature* rootSignatureCS)
{
	ID3D12PipelineState* ret = nullptr;
	ID3DBlob* csBlob = LoadComputeShader();
	D3D12_COMPUTE_PIPELINE_STATE_DESC pldesc = {};
	pldesc.CS.pShaderBytecode = csBlob->GetBufferPointer();
	pldesc.CS.BytecodeLength = csBlob->GetBufferSize();
	pldesc.NodeMask = 0;
	pldesc.pRootSignature = rootSignatureCS;
	auto result = dev_->CreateComputePipelineState(&pldesc, IID_PPV_ARGS(&ret));
	csBlob->Release();
	assert(SUCCEEDED(result));
	return ret;
}

ID3D12DescriptorHeap* 
Dx12Wrapper::CreateUAVDescriptorHeap()
{
	ID3D12DescriptorHeap* ret = nullptr;
	HRESULT result = S_OK;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 2;//UAV,SRV
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	result = dev_->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&ret));
	assert(SUCCEEDED(result));
	return ret;
}

//コンピュートシェーダ用コマンド作成
bool 
Dx12Wrapper::CreateComputeCommand(
	ID3D12CommandQueue*& cmdQue,
	ID3D12CommandAllocator*& cmdAlloc,
	ID3D12GraphicsCommandList*& cmdList,
	ID3D12PipelineState* pipeline) {
	D3D12_COMMAND_QUEUE_DESC queDesc = {};
	queDesc.NodeMask = 0;
	queDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queDesc.Priority = 0;
	queDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	dev_->CreateCommandQueue(&queDesc, IID_PPV_ARGS(&cmdQue));

	//コマンドアロケータ作成
	auto result = dev_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&cmdAlloc));
	assert(SUCCEEDED(result));
	//コマンドリスト作成
	result = dev_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, cmdAlloc, pipeline, IID_PPV_ARGS(&cmdList));
	return true;
}

//UAVとSRVを作る
void
Dx12Wrapper::CreateComputeViews(ID3D12Resource* srcRes, ID3D12Resource* destRes, ID3D12DescriptorHeap* uavHeap) {
	auto desc = srcRes->GetDesc();
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;//テクスチャとして　
	uavDesc.Texture2D.MipSlice = 0;
	uavDesc.Texture2D.PlaneSlice = 0;
	auto handle = uavHeap->GetCPUDescriptorHandleForHeapStart();
	dev_->CreateUnorderedAccessView(
		destRes,
		nullptr,
		&uavDesc,
		handle);


	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//バッファとして
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	handle.ptr += dev_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dev_->CreateShaderResourceView(srcRes, &srvDesc, handle);
}

void 
Dx12Wrapper::ExecuteAndWait(ID3D12CommandQueue* cmdQue, ID3D12CommandList* cmdList, ID3D12Fence* fence, UINT64& fenceValue)
{
	ID3D12CommandList* cmdLists[] = { cmdList };
	cmdQue->ExecuteCommandLists(1, cmdLists);
	cmdQue->Signal(fence, ++fenceValue);
	//待ち
	while (fence->GetCompletedValue() < fenceValue) {
		;
	}
}

HRESULT 
Dx12Wrapper::CopyRenderTarget(ID3D12Resource* srcRes, ID3D12Resource* dstRes) {
	D3D12_TEXTURE_COPY_LOCATION src = {}, dst = {};
	dst.pResource = dstRes;
	dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst.SubresourceIndex = 0;

	src.pResource = srcRes;
	src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;//フットプリント指定
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
	UINT nrow;
	UINT64 rowsize, size;
	auto desc = srcRes->GetDesc();
	dev_->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &nrow, &rowsize, &size);
	src.SubresourceIndex = 0;


	D3D12_RESOURCE_BARRIER BarrierDesc[2] = {};
	BarrierDesc[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	BarrierDesc[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc[0].Transition.pResource = dstRes;
	BarrierDesc[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	BarrierDesc[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	BarrierDesc[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	BarrierDesc[1] = BarrierDesc[0];
	BarrierDesc[1].Transition.pResource = srcRes;
	BarrierDesc[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	BarrierDesc[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	BarrierDesc[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	cmdList_->ResourceBarrier(2, BarrierDesc);

	cmdList_->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	BarrierDesc[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	BarrierDesc[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	BarrierDesc[1].Transition.pResource = srcRes;
	BarrierDesc[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	BarrierDesc[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	cmdList_->ResourceBarrier(2, BarrierDesc);

	return S_OK;

}

