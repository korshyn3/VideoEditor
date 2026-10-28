// PreviewWnd.cpp
// Capa: VIEW (MFC). Ver PreviewWnd.h.
#include "PreviewWnd.h"

BEGIN_MESSAGE_MAP(CPreviewWnd, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

BOOL CPreviewWnd::RegistrarClase() {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = ::DefWindowProcW;  // MFC lo reemplaza al hacer SubclassDlgItem
    wc.hInstance = AfxGetInstanceHandle();
    wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kClase;
    return AfxRegisterClass(&wc);
}

BOOL CPreviewWnd::OnEraseBkgnd(CDC*) {
    return TRUE;  // todo se pinta en OnPaint (evita parpadeo)
}

void CPreviewWnd::OnPaint() {
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(&rc);
    dc.FillSolidRect(rc, RGB(0, 0, 0));
}
