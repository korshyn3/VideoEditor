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

void CPreviewWnd::MostrarCuadro(engine::FrameBGRA&& cuadro) {
    m_cuadro = std::move(cuadro);
    m_mensaje.Empty();
    Invalidate(FALSE);
}

void CPreviewWnd::MostrarMensaje(const CString& mensaje) {
    if (m_mensaje == mensaje) return;
    m_mensaje = mensaje;
    Invalidate(FALSE);
}

BOOL CPreviewWnd::OnEraseBkgnd(CDC*) {
    return TRUE;  // todo se pinta en OnPaint (evita parpadeo)
}

void CPreviewWnd::OnPaint() {
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(&rc);

    if (m_cuadro.width <= 0 || !m_mensaje.IsEmpty()) {
        dc.FillSolidRect(rc, RGB(0, 0, 0));
        if (!m_mensaje.IsEmpty()) {
            CRect txt = rc;
            txt.DeflateRect(16, 16);
            dc.SetBkMode(TRANSPARENT);
            dc.SetTextColor(RGB(255, 120, 120));
            CFont* anterior = dc.SelectObject(GetParent()->GetFont());
            dc.DrawText(m_mensaje, txt, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
            dc.SelectObject(anterior);
        }
        return;
    }

    // Rectángulo destino: el más grande que cabe con la relación de aspecto del cuadro.
    const double escala = std::min(static_cast<double>(rc.Width()) / m_cuadro.width,
                                   static_cast<double>(rc.Height()) / m_cuadro.height);
    const int w = static_cast<int>(m_cuadro.width * escala);
    const int h = static_cast<int>(m_cuadro.height * escala);
    const CRect destino(rc.left + (rc.Width() - w) / 2, rc.top + (rc.Height() - h) / 2, 0, 0);
    const CRect imagen(destino.left, destino.top, destino.left + w, destino.top + h);

    // Barras negras: solo lo que queda fuera de la imagen (pintar todo de negro y después
    // la imagen encima haría parpadear el preview).
    dc.FillSolidRect(rc.left, rc.top, rc.Width(), imagen.top - rc.top, RGB(0, 0, 0));
    dc.FillSolidRect(rc.left, imagen.bottom, rc.Width(), rc.bottom - imagen.bottom, RGB(0, 0, 0));
    dc.FillSolidRect(rc.left, imagen.top, imagen.left - rc.left, h, RGB(0, 0, 0));
    dc.FillSolidRect(imagen.right, imagen.top, rc.right - imagen.right, h, RGB(0, 0, 0));

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = m_cuadro.width;
    bmi.bmiHeader.biHeight = -m_cuadro.height;  // negativo = filas de arriba a abajo
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;              // BGRA
    bmi.bmiHeader.biCompression = BI_RGB;

    dc.SetStretchBltMode(HALFTONE);  // mejor calidad al escalar
    ::SetBrushOrgEx(dc.GetSafeHdc(), 0, 0, nullptr);
    ::StretchDIBits(dc.GetSafeHdc(), imagen.left, imagen.top, w, h, 0, 0, m_cuadro.width, m_cuadro.height,
                    m_cuadro.pixels.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
}
