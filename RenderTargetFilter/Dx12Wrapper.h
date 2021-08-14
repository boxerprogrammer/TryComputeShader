#pragma once
#include<d3d12.h>
#include<dxgi1_6.h>
#include<map>
#include<memory>
#include<unordered_map>
#include<DirectXTex.h>
#include<wrl.h>
#include<string>
#include<functional>

class Dx12Wrapper
{
	SIZE _winSize;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	//DXGIまわり
	ComPtr < IDXGIFactory4> dxgiFactory_ = nullptr;//DXGIインターフェイス
	ComPtr < IDXGISwapChain4> swapchain_ = nullptr;//スワップチェイン

	//DirectX12まわり
	ComPtr< ID3D12Device> dev_ = nullptr;//デバイス
	ComPtr < ID3D12CommandAllocator> cmdAllocator_ = nullptr;//コマンドアロケータ
	ComPtr < ID3D12GraphicsCommandList> cmdList_ = nullptr;//コマンドリスト
	ComPtr < ID3D12CommandQueue> cmdQueue_ = nullptr;//コマンドキュー

	//表示に関わるバッファ周り
	ComPtr<ID3D12Resource> depthBuffer_ = nullptr;//深度バッファ
	std::vector<ID3D12Resource*> backBuffers_;//バックバッファ(2つ以上…スワップチェインが確保)
	ComPtr<ID3D12DescriptorHeap> rtvHeaps_ = nullptr;//レンダーターゲット用デスクリプタヒープ
	ComPtr<ID3D12DescriptorHeap> dsvHeap_ = nullptr;//深度バッファビュー用デスクリプタヒープ
	std::unique_ptr<D3D12_VIEWPORT> viewport_;//ビューポート
	std::unique_ptr<D3D12_RECT> scissorrect_;//シザー矩形
	
	//シーンを構成するバッファまわり
	ComPtr<ID3D12Resource> sceneConstBuff_ = nullptr;

	struct SceneData {
		DirectX::XMMATRIX view;//ビュー行列
		DirectX::XMMATRIX proj;//プロジェクション行列
		DirectX::XMFLOAT3 eye;//視点座標
	};
	SceneData* mappedSceneData_;
	ComPtr<ID3D12DescriptorHeap> sceneDescHeap_ = nullptr;

	//フェンス
	ComPtr<ID3D12Fence> fence_ = nullptr;
	UINT64 fenceVal_ = 0;

	//最終的なレンダーターゲットの生成
	HRESULT	CreateFinalRenderTargets();
	//デプスステンシルビューの生成
	HRESULT CreateDepthStencilView();

	//スワップチェインの生成
	HRESULT CreateSwapChain(const HWND& hwnd);

	//DXGIまわり初期化
	HRESULT InitializeDXGIDevice();



	//コマンドまわり初期化
	HRESULT InitializeCommand();

	//ビュープロジェクション用ビューの生成
	HRESULT CreateSceneView();

	//ロード用テーブル
	using LoadLambda_t = std::function<HRESULT(const std::wstring& path, DirectX::TexMetadata*, DirectX::ScratchImage&)>;
	std::map < std::string, LoadLambda_t> loadLambdaTable_;
	//テクスチャテーブル
	std::unordered_map<std::string,ComPtr<ID3D12Resource>> textureTable_;
	//テクスチャローダテーブルの作成
	void CreateTextureLoaderTable();
	//テクスチャ名からテクスチャバッファ作成、中身をコピー
	ID3D12Resource* CreateTextureFromFile(const char* texpath);


	//共通
	ID3D12Resource* offscreenRTBuffer_ = nullptr;
	ID3D12DescriptorHeap* rtvHeapOffscreen_ = nullptr;
	//スワップチェーンでないレンダーターゲット用
	//オフスクリーンバッファを作成
	HRESULT CreateOffscreenRTBuffer();

	
	//ここからコンピュートシェーダ用
	ID3D12Fence* computeFence_ = nullptr;
	ID3D12CommandQueue* computeCmdQue_=nullptr;
	ID3D12CommandAllocator* computeCmdAlloc_ = nullptr;
	ID3D12GraphicsCommandList* computeCmdList_ = nullptr;
	ID3D12DescriptorHeap* uavDescriptorHeap_ = nullptr;
	ID3D12RootSignature* rootSignatureCS_ = nullptr;
	ID3D12PipelineState* pipelineCS_ = nullptr;
	ID3D12Resource* uavResource_ = nullptr;
	HRESULT CreateUAVBuffer(ID3D12Device* dev, ID3D12Resource*& res, const D3D12_RESOURCE_DESC& desc);
	ID3D12RootSignature* CreateRootSignatureForComputeShader();
	ID3DBlob* LoadComputeShader();
	ID3D12PipelineState* CreateComputePipeline(ID3D12RootSignature* rootSignatureCS);
	ID3D12DescriptorHeap* CreateUAVDescriptorHeap();
	bool CreateComputeCommand(ID3D12CommandQueue*& cmdQue, ID3D12CommandAllocator*& cmdAlloc, ID3D12GraphicsCommandList*& cmdList, ID3D12PipelineState* pipeline);
	void CreateComputeViews(ID3D12Resource* srcRes, ID3D12Resource* destRes, ID3D12DescriptorHeap* uavHeap);
	void ExecuteAndWait(ID3D12CommandQueue* cmdQue, ID3D12CommandList* cmdList, ID3D12Fence* fence, UINT64& fenceValue);

	HRESULT CopyRenderTarget(ID3D12Resource* srcRes, ID3D12Resource* dstRes);

public:
	Dx12Wrapper(HWND hwnd);
	~Dx12Wrapper();

	void Update();
	void BeginDraw();
	void EndDraw();
	///テクスチャパスから必要なテクスチャバッファへのポインタを返す
	///@param texpath テクスチャファイルパス
	ComPtr<ID3D12Resource> GetTextureByPath(const char* texpath);

	ComPtr< ID3D12Device> Device();//デバイス
	ComPtr < ID3D12GraphicsCommandList> CommandList();//コマンドリスト
	ComPtr < IDXGISwapChain4> Swapchain();//スワップチェイン

	void SetScene();

};

