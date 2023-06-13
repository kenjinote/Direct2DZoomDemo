#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")

#include <windows.h>
#include <windowsx.h>
#include <d2d1_3.h>
#include <dwrite.h>

#define DEFAULT_DPI 96
#define ZOOM_MAX 100.0
#define ZOOM_MIN 0.1
#define ZOOM_STEP 1.20

TCHAR szClassName[] = TEXT("Window");

template<class Interface> inline void SafeRelease(Interface** ppInterfaceToRelease)
{
	if (*ppInterfaceToRelease != NULL)
	{
		(*ppInterfaceToRelease)->Release();
		(*ppInterfaceToRelease) = NULL;
	}
}

struct CPoint {
	double x;
	double y;
};

struct CSize {
	double w;
	double h;
};

class CTrans {
public:
	CTrans() : c{ 0,0 }, z(1), l{ 0,0 }, p{ 0.0, 0.0 } {}

	CPoint p; // クライアント左上座標
	CSize c; // クライアントのサイズ
	// ズーム
	double z;
	//　論理座標の注視点。これが画面上の中央に表示
	CPoint l;
	void CTrans::l2d(const CPoint* p1, CPoint* p2) const
	{
		p2->x = p.x + c.w / 2 + (p1->x - l.x) * z;
		p2->y = p.y + c.h / 2 + (p1->y - l.y) * z;
	}
	void CTrans::d2l(const CPoint* p1, CPoint* p2) const
	{
		p2->x = l.x + (p1->x - p.x - c.w / 2) / z;
		p2->y = l.y + (p1->y - p.y - c.h / 2) / z;
	}
	void CTrans::settransfromrect(const CPoint* p1, const CPoint* p2, const int margin)
	{
		l.x = p1->x + (p2->x - p1->x) / 2;
		l.y = p1->y + (p2->y - p1->y) / 2;
		if (p2->x - p1->x == 0.0 || p2->y - p1->y == 0.0)
			z = 1.0;
		else
			z = min((c.w - margin * 2) / (p2->x - p1->x), (c.h - margin * 2) / (p2->y - p1->y));
	}
	void CTrans::settransfrompoint(const CPoint* p)
	{
		l.x = p->x;
		l.y = p->y;
	};
};

void OnZoom(HWND hWnd, int x, int y, CTrans& trans)
{
	// ここバグっている
	CPoint p1{ (double)x, (double)y };
	CPoint p2;
	trans.d2l(&p1, &p2);


	double oldz = trans.z;
	trans.z *= ZOOM_STEP;
	if (trans.z > ZOOM_MAX) {
		trans.z = ZOOM_MAX;
	}

	CPoint p3;

	p3.x = trans.l.x - p2.x * (oldz / trans.z - 1);
	p3.y = trans.l.y - p2.y * (oldz / trans.z  - 1);

	trans.settransfrompoint(&p3);

	if (oldz != trans.z) {
		InvalidateRect(hWnd, NULL, FALSE);
	}
	InvalidateRect(hWnd, 0, 0);
}

void OnShrink(HWND hWnd, int x, int y, CTrans &trans)
{
	double oldz = trans.z;
	trans.z *= 1.0 / ZOOM_STEP;
	if (trans.z < ZOOM_MIN) {
		trans.z = ZOOM_MIN;
	}

	// ここバグっている
	CPoint p1{ (double)x, (double)y };
	CPoint p2;
	trans.d2l(&p1, &p2);
	trans.settransfrompoint(&p2);

	if (oldz != trans.z) {
		InvalidateRect(hWnd, NULL, FALSE);
	}
	InvalidateRect(hWnd, 0, 0);
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
	static D2D1::Matrix3x2F matrix;
	static CTrans trans;
	switch (msg)
	{
	case WM_CREATE:
		{
			matrix = D2D1::Matrix3x2F::Identity();
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

			trans.c.w = rect.right;
			trans.c.h = rect.bottom;
			trans.p.x = 0;
			trans.p.y = 0;
			trans.z = 1.0;
		}
		break;
	case WM_MOUSEWHEEL:
		{
			int x = GET_X_LPARAM(lParam);
			int y = GET_Y_LPARAM(lParam);
			CPoint p1;
			p1.x = x;
			p1.y = y;
			CPoint p2;
			trans.d2l(&p1, &p2);
		
			int delta = GET_WHEEL_DELTA_WPARAM(wParam);
			if (delta < 0) {
				trans.z *= 1.0 / ZOOM_STEP;
				if (trans.z < ZOOM_MIN) {
					trans.z = ZOOM_MIN;
				}
			}
			else {
				double oldz = trans.z;
				trans.z *= ZOOM_STEP;
				if (trans.z > ZOOM_MAX) {
					trans.z = ZOOM_MAX;
				}
			}


			matrix = D2D1::Matrix3x2F::Translation((FLOAT)(p2.x), (FLOAT)(p2.y)) *
				D2D1::Matrix3x2F::Scale((FLOAT)trans.z, (FLOAT)trans.z) *
				D2D1::Matrix3x2F::Translation((FLOAT)(-p2.x), (FLOAT)(-p2.y));
			InvalidateRect(hWnd, 0, 0);

			//if (delta < 0) {
			//	OnShrink(hWnd, x, y, trans);
			//}
			//else {
			//	OnZoom(hWnd, x, y, trans);
			//}
		
		//double oldzoom = zoom;

			//POINT p = { 0 };
			//GetCursorPos(&p);
			//ScreenToClient(hWnd, &p);

			//D2D1_POINT_2F p1;
			//p1.x = (float)p.x;
			//p1.y = (float)p.y;

			//D2D1_POINT_2F p2;

			//DisplayPoint2LogicPoint(&p1, &p2);

			//zoom += GET_WHEEL_DELTA_WPARAM(wParam) / 120.0f;

			//LookAtPoint.x = LookAtPoint.x - zoom * p2.x + p2.x;
			//LookAtPoint.y = LookAtPoint.y - zoom * p2.y + p2.y;

			//InvalidateRect(hWnd, 0, 0);
		}
		break;
	case WM_LBUTTONDOWN:
		{
			D2D1_POINT_2F p1;
			p1.x = (float)GET_X_LPARAM(lParam);
			p1.y = (float)GET_Y_LPARAM(lParam);
			//LookAtPoint.x = p1.x;
			//LookAtPoint.y = p1.y;
			trans.l.x = p1.x;
			trans.l.y = p1.y;
			InvalidateRect(hWnd, 0, 0);
		}
		break;
	case WM_RBUTTONDOWN:
		{
			CPoint p1;
			p1.x = (float)GET_X_LPARAM(lParam);
			p1.y = (float)GET_Y_LPARAM(lParam);
			CPoint p2;
			trans.d2l(&p1, &p2);
			trans.l.x = p2.x;
			trans.l.y = p2.y;
			InvalidateRect(hWnd, 0, 0);
		}
		break;
	case WM_KEYDOWN:
		{
			if (wParam == VK_SPACE)
			{
				RECT rect;
				GetClientRect(hWnd, &rect);
				trans.l.x = 0;
				trans.l.y = 0;
				trans.c.w = rect.right;
				trans.c.h = rect.bottom;
				trans.z = 1.0;
				InvalidateRect(hWnd, 0, 0);
			}
			else if (wParam == VK_UP) {
				trans.z += 1.0;
				InvalidateRect(hWnd, 0, 0);
			}
			else if (wParam == VK_DOWN) {
				trans.z -= 1.0;
				InvalidateRect(hWnd, 0, 0);
			}
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

			m_pRenderTarget->SetTransform(matrix);

			m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

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
