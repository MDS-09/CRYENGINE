// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SwapChain.h"

void CSwapChain::ReadSwapChainSurfaceDesc()
{
	CRY_ASSERT(m_pSwapChain);

	HRESULT hr = S_OK;
	ZeroMemory(&m_swapChainDesc, sizeof(DXGI_SURFACE_DESC));

	//////////////////////////////////////////////////////////////////////////
	// Read Back Buffers Surface description from the Swap Chain
#if CRY_RENDERER_GNM
	CGnmSwapChain::SDesc desc = m_pSwapChain->GnmGetDesc();
	m_swapChainDesc.Width = desc.width;
	m_swapChainDesc.Height = desc.height;
	m_swapChainDesc.Format = desc.format;
	m_swapChainDesc.SampleDesc.Count = 1;
	m_swapChainDesc.SampleDesc.Quality = 0;
	hr = m_pSwapChain->GnmIsValid() ? S_OK : E_FAIL;
#else
	DXGI_SWAP_CHAIN_DESC backBufferSurfaceDesc;
	hr = m_pSwapChain->GetDesc(&backBufferSurfaceDesc);

	m_swapChainDesc.Width = (UINT)backBufferSurfaceDesc.BufferDesc.Width;
	m_swapChainDesc.Height = (UINT)backBufferSurfaceDesc.BufferDesc.Height;
#if defined(SUPPORT_DEVICE_INFO)
	m_swapChainDesc.Format = backBufferSurfaceDesc.BufferDesc.Format;
	m_swapChainDesc.SampleDesc = backBufferSurfaceDesc.SampleDesc;
#elif CRY_RENDERER_VULKAN
	m_swapChainDesc.Format = backBufferSurfaceDesc.BufferDesc.Format;
	m_swapChainDesc.SampleDesc = backBufferSurfaceDesc.SampleDesc;
#elif CRY_PLATFORM_DURANGO
	m_swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
	m_swapChainDesc.SampleDesc.Count = 1;
	m_swapChainDesc.SampleDesc.Quality = 0;
#endif
#endif
	//////////////////////////////////////////////////////////////////////////
	CRY_ASSERT(!FAILED(hr));
}

#if !CRY_PLATFORM_DURANGO && !CRY_RENDERER_GNM
CSwapChain CSwapChain::CreateSwapChain(HWND hWnd, DXGIOutput* pOutput, uint32_t width, uint32_t height, bool isMainContext, bool isFullscreen)
{
#if defined(USE_SDL2_VIDEO)
	DXGI_FORMAT fmt = DXGI_FORMAT_B8G8R8X8_UNORM;
#else
	DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM;
#endif

#if (CRY_RENDERER_DIRECT3D >= 120)
	const int defaultBufferCount = 4;
#else
	const int defaultBufferCount = 2;
#endif

	const int backbufferWidth = width;
	const int backbufferHeight = height;

	const bool windowed = !isFullscreen;

	IDXGISwapChain* piSwapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC scDesc;

	scDesc.BufferDesc.Width = backbufferWidth;
	scDesc.BufferDesc.Height = backbufferHeight;
	scDesc.BufferDesc.RefreshRate.Numerator = 0;
	scDesc.BufferDesc.RefreshRate.Denominator = 1;
	scDesc.BufferDesc.Format = fmt;
	scDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	scDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	scDesc.SampleDesc.Count = 1;
	scDesc.SampleDesc.Quality = 0;

	scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
	const bool bWaitable = (isMainContext && CRenderer::CV_r_D3D12WaitableSwapChain && windowed);
	scDesc.BufferCount = (isMainContext && (CRenderer::CV_r_MaxFrameLatency > 0 || bWaitable)) ? MAX_FRAMES_IN_FLIGHT : defaultBufferCount;
	scDesc.OutputWindow = hWnd;

	// Always create a swapchain for windowed mode as per Microsoft recommendations here: https://msdn.microsoft.com/en-us/library/windows/desktop/bb174579(v=vs.85).aspx
	// Specifically: "We recommend that you create a windowed swap chain and allow the end user to change the swap chain to full screen through SetFullscreenState; that is, do not set the Windowed member of DXGI_SWAP_CHAIN_DESC to FALSE to force the swap chain to be full screen."
	scDesc.Windowed = 1;
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	scDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

#if (CRY_RENDERER_DIRECT3D >= 120)
	if (bWaitable)
	{
		// Set this flag to create a waitable object you can use to ensure rendering does not begin while a
		// frame is still being presented. When this flag is used, the swapchain's latency must be set with
		// the IDXGISwapChain2::SetMaximumFrameLatency API instead of IDXGIDevice1::SetMaximumFrameLatency.
		scDesc.BufferCount = MAX_FRAMES_IN_FLIGHT + 1;
		scDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
	}
#endif

	DXGI_RATIONAL desktopRefreshRate = scDesc.BufferDesc.RefreshRate;

	if (!windowed)
	{
		DXGI_MODE_DESC match;
		if (SUCCEEDED(pOutput->FindClosestMatchingMode(&scDesc.BufferDesc, &match, gcpRendD3D.DevInfo().Device())))
		{
			scDesc.BufferDesc = match;
			desktopRefreshRate = match.RefreshRate;
		}
	}

#if defined(SUPPORT_DEVICE_INFO)
	HRESULT hr = gcpRendD3D.DevInfo().Factory()->CreateSwapChain(gcpRendD3D.DevInfo().Device(), &scDesc, &piSwapChain);
#elif CRY_PLATFORM_ORBIS
	HRESULT hr = CCryDXOrbisGIFactory().CreateSwapChain(gcpRendD3D.GetDevice().GetRealDevice(), &scDesc, &piSwapChain);
#else
#error UNKNOWN PLATFORM TRYING TO CREATE SWAP CHAIN
#endif
	DXGISwapChain* pSwapChain = nullptr;

	CRY_ASSERT(SUCCEEDED(hr) && piSwapChain != nullptr);
	hr = piSwapChain->QueryInterface(__uuidof(DXGISwapChain), (void**)&pSwapChain);
	CRY_ASSERT(SUCCEEDED(hr) && pSwapChain != 0);

	piSwapChain->Release();

#if (CRY_RENDERER_DIRECT3D >= 120)
	if (scDesc.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)
		pSwapChain->SetMaximumFrameLatency(CRendererCVars::CV_r_MaxFrameLatency);
#endif

	CSwapChain sc = { pSwapChain };
#if defined(SUPPORT_DEVICE_INFO)
	auto refreshRate = !windowed ? scDesc.BufferDesc.RefreshRate : desktopRefreshRate;
	sc.m_refreshRateNumerator = refreshRate.Numerator;
	sc.m_refreshRateDenominator = refreshRate.Denominator;
#endif

	return sc;
}
#endif

#if CRY_PLATFORM_DURANGO
#if (CRY_RENDERER_DIRECT3D >= 120)
CSwapChain CSwapChain::CreateXboxSwapChain(IDXGIFactory2ToCall* pDXGIFactory, ID3D12Device* pD3D12Device, CCryDX12Device* pDX12Device, uint32_t width, uint32_t height)
#else
CSwapChain CSwapChain::CreateXboxSwapChain(IDXGIFactory2ToCall* pDXGIFactory, ID3D11Device* pD3D11Device, uint32_t width, uint32_t height)
#endif
{
	// Create full HD swap chain with backbuffer
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swapChainDesc.Flags = DXGIX_SWAP_CHAIN_MATCH_XBOX360_AND_PC;

	DXGISwapChain* pSwapChain = nullptr;
	{
		LOADING_TIME_PROFILE_SECTION_NAMED("CRenderDisplayContext::CreateXboxSwapChain: CreateSwapChainForCoreWindow()");
#if (CRY_RENDERER_DIRECT3D >= 120)
		IDXGISwapChain1ToCall* pDXGISwapChain = nullptr;
		HRESULT hr = pDXGIFactory->CreateSwapChainForCoreWindow(pD3D12Device, (IUnknown*)gEnv->pWindow, &swapChainDesc, nullptr, &pDXGISwapChain);
		if (hr == S_OK && pDXGISwapChain)
			pSwapChain = CCryDX12SwapChain::Create(pDX12Device, pDXGISwapChain, &swapChainDesc);
#else
		HRESULT hr = pDXGIFactory->CreateSwapChainForCoreWindow(pD3D11Device, (IUnknown*)gEnv->pWindow, &swapChainDesc, nullptr, &pSwapChain);
#endif

		CRY_ASSERT(SUCCEEDED(hr) && pSwapChain != nullptr);
	}

	return CSwapChain{ pSwapChain };
}
#endif

#if CRY_RENDERER_GNM
CSwapChain CSwapChain::CreateGNMSwapChain(uint32_t width, uint32_t height)
{
	CGnmSwapChain::SDesc desc;
	desc.type = CGnmSwapChain::kTypeTelevision;
	desc.width = width;
	desc.height = height;
	desc.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	desc.numBuffers = 2;

	DXGISwapChain* pSwapChain = nullptr;
	HRESULT hr = GnmCreateSwapChain(desc, &pSwapChain) ? S_OK : E_FAIL;
	CRY_ASSERT(SUCCEEDED(hr) && pSwapChain != nullptr);

	CSwapChain sc = { pSwapChain };
#if defined(SUPPORT_DEVICE_INFO)
	DXGI_SWAP_CHAIN_DESC scDesc;
	pSwapChain->GetDesc(&scDesc);

	sc.m_refreshRateNumerator = scDesc.BufferDesc.RefreshRate.Numerator;
	sc.m_refreshRateDenominator = scDesc.BufferDesc.RefreshRate.Denominator;
#endif

	return sc;
}
#endif

#if defined(SUPPORT_DEVICE_INFO_USER_DISPLAY_OVERRIDES)
inline const char* GetScanlineOrderNaming(DXGI_MODE_SCANLINE_ORDER v)
{
	switch (v)
	{
	case DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE:
		return "progressive";
	case DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST:
		return "interlaced (upper field first)";
	case DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST:
		return "interlaced (lower field first)";
	default:
		return "unspecified";
	}
}
void CSwapChain::UserOverrideDisplayProperties(DXGI_MODE_DESC& desc)
{
	if (gcpRendD3D->IsFullscreen())
	{
		if (CRenderer::CV_r_overrideRefreshRate > 0)
		{
			DXGI_RATIONAL& refreshRate = desc.RefreshRate;
			if (refreshRate.Denominator)
				gEnv->pLog->Log("Overriding refresh rate to %.2f Hz (was %.2f Hz).", (float)CRenderer::CV_r_overrideRefreshRate, (float)refreshRate.Numerator / (float)refreshRate.Denominator);
			else
				gEnv->pLog->Log("Overriding refresh rate to %.2f Hz (was undefined).", (float)CRenderer::CV_r_overrideRefreshRate);
			refreshRate.Numerator = (unsigned int)(CRenderer::CV_r_overrideRefreshRate * 1000.0f);
			refreshRate.Denominator = 1000;
		}

		if (CRenderer::CV_r_overrideScanlineOrder > 0)
		{
			DXGI_MODE_SCANLINE_ORDER old = desc.ScanlineOrdering;
			DXGI_MODE_SCANLINE_ORDER& so = desc.ScanlineOrdering;
			switch (CRenderer::CV_r_overrideScanlineOrder)
			{
			case 2:
				so = DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST;
				break;
			case 3:
				so = DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST;
				break;
			default:
				so = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
				break;
			}
			gEnv->pLog->Log("Overriding scanline order to %s (was %s).", GetScanlineOrderNaming(so), GetScanlineOrderNaming(old));
		}
	}
}
#endif

void CSwapChain::ResizeSwapChain(uint32_t width, uint32_t height, bool isMainContext, bool bResizeTarget)
{
#if !CRY_PLATFORM_DURANGO && !CRY_RENDERER_GNM
	// Wait for GPU to finish occupying the resources
	GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->ClearState(true);

	const int backbufferWidth = width;
	const int backbufferHeight = height;

	m_swapChainDesc.Width = backbufferWidth;
	m_swapChainDesc.Height = backbufferHeight;

#if (CRY_RENDERER_DIRECT3D >= 110) || (CRY_RENDERER_VULKAN >= 10)
	DXGI_SWAP_CHAIN_DESC scDesc;
	m_pSwapChain->GetDesc(&scDesc);

	scDesc.BufferDesc.Width = backbufferWidth;
	scDesc.BufferDesc.Height = backbufferHeight;

#if defined(SUPPORT_DEVICE_INFO_USER_DISPLAY_OVERRIDES)
	if (isMainContext)
		UserOverrideDisplayProperties(scDesc.BufferDesc);
#endif

	//	TODO: Activate once sRGB encoding is gone from the shader
	//	m_devInfo.SwapChainDesc().BufferDesc.Format =
	//		((nNewColDepth / 4) <=  8 ? DXGI_FORMAT_R8G8B8A8_UNORM :
	//		((nNewColDepth / 4) <= 10 ? DXGI_FORMAT_R10G10B10A2_UNORM :
	//		                            DXGI_FORMAT_R16G16B16A16_FLOAT));

	// Resize the hWnd's dimensions associated with the SwapChain (triggers EVENT)
	if (bResizeTarget)
	{
		HRESULT hr = m_pSwapChain->ResizeTarget(&scDesc.BufferDesc);
		CRY_ASSERT(SUCCEEDED(hr));
	}

	// Resize the Resources associated with the SwapChain
	{
		HRESULT hr = m_pSwapChain->ResizeBuffers(0, scDesc.BufferDesc.Width, scDesc.BufferDesc.Height, DXGI_FORMAT_UNKNOWN, scDesc.Flags);
		CRY_ASSERT(SUCCEEDED(hr));
	}
#endif

	ReadSwapChainSurfaceDesc();
#endif
}

#ifdef CRY_RENDERER_GNM
HRESULT CSwapChain::Present(CGnmGraphicsCommandList* pCommandList, CGnmSwapChain::EFlipMode flipMode)
{
	m_pSwapChain->GnmPresent(pCommandList, flipMode);
	return S_OK;
}
#else
HRESULT CSwapChain::Present(unsigned int syncInterval, unsigned int flags)
{
	return m_pSwapChain->Present(syncInterval, flags);
}
#endif