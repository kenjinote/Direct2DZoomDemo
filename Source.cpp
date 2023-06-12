#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")

#include <windows.h>
#include <d2d1_3.h>
#include <dwrite.h>

#define DEFAULT_DPI 96

TCHAR szClassName[] = TEXT("Window");

template<class Interface> inline void SafeRelease(Interface** ppInterfaceToRelease)
{
	if (*ppInterfaceToRelease != NULL)
	{
		(*ppInterfaceToRelease)->Release();
		(*ppInterfaceToRelease) = NULL;
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static ID2D1Factory* m_pD2DFactory;
	static IWICImagingFactory* m_pWICFactory;
	static IDWriteFactory* m_pDWriteFactory;
	static ID2D1HwndRenderTarget* m_pRenderTarget;
	static IDWriteTextFormat* m_pTextFormat;
	static ID2D1SolidColorBrush* m_pNormalBrush;
	static ID2D1DeviceContext* m_pDeviceContext;
	static double zoom = 1.0;
	static double x = 100.0; // 中心点
	static double y = 100.0; // 中心点

	switch (msg)
	{
	case WM_CREATE:
		{
			static const FLOAT msc_fontSize = 25;

			HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
			if (SUCCEEDED(hr))
				hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(m_pDWriteFactory), reinterpret_cast<IUnknown**>(&m_pDWriteFactory));
			if (SUCCEEDED(hr))
				hr = m_pDWriteFactory->CreateTextFormat(L"Segoe UI", 0, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, msc_fontSize, L"", &m_pTextFormat);
			if (SUCCEEDED(hr))
				hr = m_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
			if (SUCCEEDED(hr))
				hr = m_pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
			if (FAILED(hr)) {
				MessageBox(hWnd, L"Direct2D の初期化に失敗しました。", 0, 0);
			}

			RECT rect;
			GetClientRect(hWnd, &rect);

			x = rect.right / 2.0;
			y = rect.bottom / 2.0;

		}
		break;
	case WM_MOUSEWHEEL:
		{
			zoom += GET_WHEEL_DELTA_WPARAM(wParam) / 120.0f;


			POINT p = { 0 };
			GetCursorPos(&p);
			ScreenToClient(hWnd, &p);
			x = p.x;
			y = p.y;

			InvalidateRect(hWnd, 0, 0);
		}
		break;
	case WM_PAINT:
		{
			ValidateRect(hWnd, NULL);
			HRESULT hr = S_OK;
			if (!m_pRenderTarget)
			{
				RECT rect;
				GetClientRect(hWnd, &rect);
				D2D1_SIZE_U CSize = D2D1::SizeU(rect.right, rect.bottom);
				hr = m_pD2DFactory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(), DEFAULT_DPI, DEFAULT_DPI, D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT), D2D1::HwndRenderTargetProperties(hWnd, CSize), &m_pRenderTarget);
				if (SUCCEEDED(hr))
					hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &m_pNormalBrush);
				if (SUCCEEDED(hr))
					hr = m_pRenderTarget->QueryInterface(&m_pDeviceContext);
			}
			if (SUCCEEDED(hr))
			{
				m_pRenderTarget->BeginDraw();
			}

			m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
			//m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Translation(x, y));

			m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

			D2D1_POINT_2F p1;
			p1.x = -x;
			p1.y = -y;
			m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Scale(zoom,zoom,p1));

			D2D1_POINT_2F s = { (float)0.0f, (float)0.0f };
			D2D1_POINT_2F e = { (float)100.0f, (float)100.0f };
			m_pRenderTarget->DrawLine(s, e, m_pNormalBrush, 10.0f);
			hr = m_pRenderTarget->EndDraw();
			if (hr == D2DERR_RECREATE_TARGET)
			{
				SafeRelease(&m_pRenderTarget);
				SafeRelease(&m_pNormalBrush);
				SafeRelease(&m_pDeviceContext);
			}
		}
		break;
	case WM_SIZE:
		if (m_pRenderTarget)
		{
			RECT rect;
			GetClientRect(hWnd, &rect);
			D2D1_SIZE_U CSize = { (UINT32)rect.right, (UINT32)rect.bottom };
			m_pRenderTarget->Resize(CSize);
		}
		break;
	case WM_DESTROY:
		SafeRelease(&m_pD2DFactory);
		SafeRelease(&m_pDWriteFactory);
		SafeRelease(&m_pRenderTarget);
		SafeRelease(&m_pTextFormat);
		SafeRelease(&m_pNormalBrush);
		SafeRelease(&m_pDeviceContext);
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nShowCmd)
{
	MSG msg;
	WNDCLASS wndclass = {
		CS_HREDRAW | CS_VREDRAW,
		WndProc,
		0,
		0,
		hInstance,
		0,
		LoadCursor(0,IDC_ARROW),
		(HBRUSH)(COLOR_WINDOW + 1),
		0,
		szClassName
	};
	RegisterClass(&wndclass);
	HWND hWnd = CreateWindow(
		szClassName,
		TEXT("Window"),
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT,
		0,
		CW_USEDEFAULT,
		0,
		0,
		0,
		hInstance,
		0
	);
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);
	while (GetMessage(&msg, 0, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return (int)msg.wParam;
}
