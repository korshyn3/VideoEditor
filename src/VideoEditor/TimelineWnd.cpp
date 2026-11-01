// TimelineWnd.cpp
// Capa: VIEW (MFC). Ver TimelineWnd.h.
#include "TimelineWnd.h"
#include "MensajesApp.h"
#include "Utf8.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>

namespace {

// Paleta de colores del timeline.
const COLORREF kFondo = RGB(40, 40, 44);
const COLORREF kFondoFila = RGB(50, 50, 55);
const COLORREF kFondoFilaSel = RGB(62, 62, 70);
const COLORREF kCabecera = RGB(60, 60, 66);
const COLORREF kCabeceraSel = RGB(85, 85, 100);
const COLORREF kRegla = RGB(30, 30, 33);
const COLORREF kTexto = RGB(225, 225, 225);
const COLORREF kTextoTenue = RGB(150, 150, 150);
const COLORREF kClipVideo = RGB(64, 104, 168);
const COLORREF kClipAudio = RGB(60, 140, 88);
const COLORREF kBordeSel = RGB(255, 210, 60);
const COLORREF kCursor = RGB(230, 60, 60);

// Nombre de archivo (sin carpeta) de una ruta UTF-8.
CString NombreArchivo(const std::string& rutaUtf8) {
    const auto p = std::filesystem::path(std::u8string(rutaUtf8.begin(), rutaUtf8.end()));
    return CString(p.filename().wstring().c_str());
}

// Intervalo entre marcas de la regla: el menor de la lista que deje al menos ~80 px.
double IntervaloMarcas(double pxPorSegundo) {
    static const double kIntervalos[] = {0.1, 0.25, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 1800, 3600};
    for (double s : kIntervalos)
        if (s * pxPorSegundo >= 80.0) return s;
    return 3600;
}

} // namespace

BEGIN_MESSAGE_MAP(CTimelineWnd, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_WM_CAPTURECHANGED()
    ON_WM_MOUSEWHEEL()
    ON_WM_HSCROLL()
END_MESSAGE_MAP()

BOOL CTimelineWnd::RegistrarClase() {
    WNDCLASSW wc = {};
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = ::DefWindowProcW;  // MFC lo reemplaza al hacer SubclassDlgItem
    wc.hInstance = AfxGetInstanceHandle();
    wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kClase;
    return AfxRegisterClass(&wc);
}

void CTimelineWnd::Refrescar() {
    if (!GetSafeHwnd()) return;
    ActualizarScroll();
    Invalidate(FALSE);
}

void CTimelineWnd::SeguirCursor() {
    if (!m_ctrl || !GetSafeHwnd()) return;
    CRect rc;
    GetClientRect(&rc);
    const int x = m_escala.tiempoAX(m_ctrl->playhead());
    if (x < kAnchoCabecera || x > rc.right - 20) {
        // El cursor queda a un cuarto del ancho visible.
        const int visible = rc.Width() - kAnchoCabecera;
        m_escala.setScrollPx(m_escala.anchoContenido(m_ctrl->playhead()) - visible / 4);
        ActualizarScroll();
    }
    Invalidate(FALSE);
}

// ---------------------------------------------------------------------------
// Geometría
// ---------------------------------------------------------------------------

std::vector<const model::Track*> CTimelineWnd::PistasVisuales() const {
    std::vector<const model::Track*> filas;
    if (!m_ctrl) return filas;
    const auto& tracks = m_ctrl->project().timeline().tracks;
    // Video: de la última (más alta) a V1; así la pista que se ve "encima" está arriba.
    for (auto it = tracks.rbegin(); it != tracks.rend(); ++it)
        if (it->kind == model::TrackKind::Video) filas.push_back(&*it);
    for (const model::Track& t : tracks)
        if (t.kind == model::TrackKind::Audio) filas.push_back(&t);
    return filas;
}

int CTimelineWnd::FilaEnY(int y) const {
    if (y < kAltoRegla) return -1;
    const int fila = (y - kAltoRegla) / kAltoPista;
    return fila < static_cast<int>(PistasVisuales().size()) ? fila : -1;
}

CRect CTimelineWnd::RectFila(int fila) const {
    CRect rc;
    GetClientRect(&rc);
    const int top = kAltoRegla + fila * kAltoPista;
    return CRect(0, top, rc.right, top + kAltoPista);
}

CRect CTimelineWnd::RectClip(int fila, const model::Clip& clip) const {
    const CRect f = RectFila(fila);
    return CRect(m_escala.tiempoAX(clip.timelineStart), f.top + 4, m_escala.tiempoAX(clip.timelineEnd()), f.bottom - 4);
}

const model::Clip* CTimelineWnd::ClipEn(CPoint p, int* filaOut) const {
    if (p.x < kAnchoCabecera) return nullptr;
    const int fila = FilaEnY(p.y);
    if (fila < 0) return nullptr;
    const model::Track* pista = PistasVisuales()[fila];
    for (const model::Clip& c : pista->clips) {
        if (RectClip(fila, c).PtInRect(p)) {
            if (filaOut) *filaOut = fila;
            return &c;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Dibujo (con doble buffer: se dibuja en un bitmap en memoria y se copia de una vez)
// ---------------------------------------------------------------------------

BOOL CTimelineWnd::OnEraseBkgnd(CDC*) {
    return TRUE;  // todo se pinta en OnPaint; borrar aquí causaría parpadeo
}

void CTimelineWnd::OnPaint() {
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(&rc);
    if (rc.IsRectEmpty()) return;

    CDC mem;
    mem.CreateCompatibleDC(&dc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* bmpAnterior = mem.SelectObject(&bmp);
    CFont* fuenteAnterior = mem.SelectObject(GetParent()->GetFont());

    Dibujar(mem, rc);
    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);

    mem.SelectObject(fuenteAnterior);
    mem.SelectObject(bmpAnterior);
}

void CTimelineWnd::Dibujar(CDC& dc, const CRect& rc) {
    dc.FillSolidRect(rc, kFondo);
    dc.SetBkMode(TRANSPARENT);
    if (!m_ctrl) return;

    const controller::Seleccion& sel = m_ctrl->seleccion();
    const auto filas = PistasVisuales();
    m_escala.setOrigenX(kAnchoCabecera);

    // Filas de pistas.
    for (int i = 0; i < static_cast<int>(filas.size()); ++i) {
        const model::Track* pista = filas[i];
        const bool pistaSel = pista->id == sel.trackId;
        const bool video = pista->kind == model::TrackKind::Video;
        CRect f = RectFila(i);
        f.bottom -= 1;  // línea separadora
        dc.FillSolidRect(f, pistaSel ? kFondoFilaSel : kFondoFila);

        // Clips (recortados al área a la derecha de la cabecera).
        dc.SaveDC();
        dc.IntersectClipRect(kAnchoCabecera, f.top, rc.right, f.bottom);
        for (const model::Clip& c : pista->clips) {
            CRect rcClip = RectClip(i, c);
            if (rcClip.right < kAnchoCabecera || rcClip.left > rc.right) continue;
            DibujarClip(dc, rcClip, c, video, c.id == sel.clipId);
        }
        dc.RestoreDC(-1);

        // Cabecera con el nombre de la pista.
        CRect cab(0, f.top, kAnchoCabecera, f.bottom);
        dc.FillSolidRect(cab, pistaSel ? kCabeceraSel : kCabecera);
        dc.SetTextColor(kTexto);
        CString nombre = CStringDesdeUtf8(pista->name);
        CRect txt = cab;
        txt.DeflateRect(8, 0);
        dc.DrawText(nombre, txt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // Fantasma del clip que se está arrastrando.
    if (m_arrastre == Arrastre::Clip && m_seMovio) {
        const model::Clip* c = m_ctrl->project().timeline().findClip(m_clipArrastrado);
        for (int i = 0; c && i < static_cast<int>(filas.size()); ++i) {
            if (filas[i]->id != m_pistaFantasma) continue;
            const CRect f = RectFila(i);
            CRect g(m_escala.tiempoAX(m_inicioFantasma), f.top + 3,
                    m_escala.tiempoAX(m_inicioFantasma + c->duration()), f.bottom - 3);
            CPen pen(PS_DOT, 1, RGB(255, 255, 255));
            CPen* penAnterior = dc.SelectObject(&pen);
            CBrush* brochaAnterior = static_cast<CBrush*>(dc.SelectStockObject(NULL_BRUSH));
            dc.SetBkMode(TRANSPARENT);
            dc.Rectangle(g);
            dc.SelectObject(brochaAnterior);
            dc.SelectObject(penAnterior);
        }
    }

    DibujarRegla(dc, rc);

    // Cursor de reproducción (línea vertical roja con un triángulo en la regla).
    const int xc = m_escala.tiempoAX(m_ctrl->playhead());
    if (xc >= kAnchoCabecera && xc <= rc.right) {
        dc.FillSolidRect(xc, 0, 1, rc.bottom, kCursor);
        POINT tri[3] = {{xc - 6, 0}, {xc + 6, 0}, {xc, 9}};
        CBrush brocha(kCursor);
        CPen pen(PS_SOLID, 1, kCursor);
        CBrush* b0 = dc.SelectObject(&brocha);
        CPen* p0 = dc.SelectObject(&pen);
        dc.Polygon(tri, 3);
        dc.SelectObject(b0);
        dc.SelectObject(p0);
    }
}

void CTimelineWnd::DibujarRegla(CDC& dc, const CRect& rc) {
    dc.FillSolidRect(0, 0, rc.right, kAltoRegla, kRegla);
    dc.FillSolidRect(0, 0, kAnchoCabecera, kAltoRegla, kCabecera);

    const double intervalo = IntervaloMarcas(m_escala.pxPorSegundo());
    const model::TimeUs paso = model::segundosATiempo(intervalo);
    model::TimeUs t = (m_escala.xATiempo(kAnchoCabecera) / paso) * paso;
    if (t < 0) t = 0;

    dc.SetTextColor(kTextoTenue);
    for (; m_escala.tiempoAX(t) <= rc.right; t += paso) {
        const int x = m_escala.tiempoAX(t);
        if (x < kAnchoCabecera) continue;
        dc.FillSolidRect(x, kAltoRegla - 8, 1, 8, kTextoTenue);
        // Marcas menores (4 por intervalo).
        for (int k = 1; k < 4; ++k) {
            const int xm = m_escala.tiempoAX(t + paso * k / 4);
            if (xm <= rc.right) dc.FillSolidRect(xm, kAltoRegla - 4, 1, 4, kTextoTenue);
        }
        CString etiqueta = CStringDesdeUtf8(model::formatearTiempo(t));
        // Sin milisegundos si el intervalo es de 1 s o más (la etiqueta queda más limpia).
        if (intervalo >= 1.0 && etiqueta.GetLength() > 4) etiqueta = etiqueta.Left(etiqueta.GetLength() - 4);
        dc.TextOut(x + 3, 2, etiqueta);
    }
}

void CTimelineWnd::DibujarClip(CDC& dc, const CRect& rcClip, const model::Clip& clip, bool video, bool seleccionado) {
    dc.FillSolidRect(rcClip, video ? kClipVideo : kClipAudio);

    // Borde (amarillo y doble si está seleccionado).
    CBrush borde(seleccionado ? kBordeSel : RGB(20, 20, 20));
    dc.FrameRect(rcClip, &borde);
    if (seleccionado) {
        CRect interior = rcClip;
        interior.DeflateRect(1, 1);
        dc.FrameRect(interior, &borde);
    }

    // Nombre del archivo fuente (+ indicador de audio vinculado).
    const model::MediaInfo* info = m_ctrl->project().media().find(clip.mediaId);
    CString texto = info ? NombreArchivo(info->path) : CString(L"(medio desconocido)");
    if (video && clip.linkedAudio) texto += L"  ♪";
    CRect txt = rcClip;
    txt.DeflateRect(6, 0);
    dc.SetTextColor(kTexto);
    dc.DrawText(texto, txt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

// ---------------------------------------------------------------------------
// Scroll y zoom
// ---------------------------------------------------------------------------

void CTimelineWnd::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    ActualizarScroll();
}

void CTimelineWnd::ActualizarScroll() {
    if (!m_ctrl) return;
    CRect rc;
    GetClientRect(&rc);
    const int visible = std::max(1, static_cast<int>(rc.Width()) - kAnchoCabecera);
    // Se deja medio ancho extra al final para poder colocar clips después del último.
    const int contenido = m_escala.anchoContenido(m_ctrl->project().timeline().duration()) + visible / 2;

    SCROLLINFO si = {sizeof(si)};
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = std::max(contenido, visible);
    si.nPage = static_cast<UINT>(visible);
    if (m_escala.scrollPx() > si.nMax - visible) m_escala.setScrollPx(si.nMax - visible);
    si.nPos = m_escala.scrollPx();
    SetScrollInfo(SB_HORZ, &si, TRUE);
}

void CTimelineWnd::OnHScroll(UINT nSBCode, UINT /*nPos*/, CScrollBar* /*pScrollBar*/) {
    SCROLLINFO si = {sizeof(si)};
    si.fMask = SIF_ALL;
    GetScrollInfo(SB_HORZ, &si);
    int pos = si.nPos;
    switch (nSBCode) {
    case SB_LINELEFT: pos -= 30; break;
    case SB_LINERIGHT: pos += 30; break;
    case SB_PAGELEFT: pos -= static_cast<int>(si.nPage); break;
    case SB_PAGERIGHT: pos += static_cast<int>(si.nPage); break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: pos = si.nTrackPos; break;  // nTrackPos admite 32 bits
    case SB_LEFT: pos = si.nMin; break;
    case SB_RIGHT: pos = si.nMax; break;
    default: return;
    }
    pos = std::clamp(pos, si.nMin, std::max(si.nMin, si.nMax - static_cast<int>(si.nPage) + 1));
    m_escala.setScrollPx(pos);
    ActualizarScroll();
    Invalidate(FALSE);
}

BOOL CTimelineWnd::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) {
    ScreenToClient(&pt);
    if (nFlags & MK_CONTROL) {
        // Ctrl + rueda: zoom alrededor del mouse.
        m_escala.zoomAlrededorDe(std::max<int>(pt.x, kAnchoCabecera), zDelta > 0 ? 1.25 : 0.8);
    } else {
        // Rueda sola: desplazamiento horizontal.
        m_escala.setScrollPx(m_escala.scrollPx() - zDelta / 2);
    }
    ActualizarScroll();
    Invalidate(FALSE);
    return TRUE;
}

// ---------------------------------------------------------------------------
// Mouse: cursor, selección y arrastre de clips
// ---------------------------------------------------------------------------

void CTimelineWnd::MoverCursorA(int x) {
    m_ctrl->setPlayhead(m_escala.xATiempo(std::max(x, static_cast<int>(kAnchoCabecera))));
}

void CTimelineWnd::NotificarCambio(bool cambioModelo) {
    Refrescar();
    if (CWnd* padre = GetParent()) padre->SendMessage(WM_APP_VISTA_CAMBIO, cambioModelo ? 1 : 0);
}

void CTimelineWnd::OnLButtonDown(UINT /*nFlags*/, CPoint point) {
    SetFocus();  // para recibir la rueda del mouse
    if (!m_ctrl) return;
    m_puntoInicio = point;
    m_seMovio = false;

    // 1) Regla: mueve el cursor y empieza a arrastrarlo.
    if (point.y < kAltoRegla && point.x >= kAnchoCabecera) {
        m_arrastre = Arrastre::Cursor;
        SetCapture();
        MoverCursorA(point.x);
        NotificarCambio(false);
        return;
    }

    const int fila = FilaEnY(point.y);
    if (fila < 0) return;
    const model::Track* pista = PistasVisuales()[fila];

    // 2) Cabecera: selecciona la pista.
    if (point.x < kAnchoCabecera) {
        m_ctrl->seleccionarPista(pista->id);
        NotificarCambio(false);
        return;
    }

    // 3) Clip: lo selecciona y prepara un posible arrastre.
    if (const model::Clip* clip = ClipEn(point, nullptr)) {
        m_ctrl->seleccionarClip(clip->id);
        m_arrastre = Arrastre::Clip;
        m_clipArrastrado = clip->id;
        m_inicioOriginal = clip->timelineStart;
        m_inicioFantasma = clip->timelineStart;
        m_pistaFantasma = pista->id;
        SetCapture();
        NotificarCambio(false);
        return;
    }

    // 4) Zona vacía de una pista: selecciona la pista y mueve el cursor allí.
    m_ctrl->seleccionarPista(pista->id);
    MoverCursorA(point.x);
    NotificarCambio(false);
}

void CTimelineWnd::OnMouseMove(UINT /*nFlags*/, CPoint point) {
    if (m_arrastre == Arrastre::Nada || !m_ctrl) return;

    if (m_arrastre == Arrastre::Cursor) {
        MoverCursorA(point.x);
        NotificarCambio(false);
        return;
    }

    // Arrastre de clip: solo empieza tras superar un pequeño umbral (evita mover por error).
    if (!m_seMovio && std::abs(point.x - m_puntoInicio.x) < kUmbralArrastre &&
        std::abs(point.y - m_puntoInicio.y) < kUmbralArrastre)
        return;
    m_seMovio = true;

    m_inicioFantasma = std::max<model::TimeUs>(0, m_inicioOriginal + m_escala.pxADuracion(point.x - m_puntoInicio.x));

    // La pista destino cambia solo si la fila bajo el mouse es del mismo tipo.
    const model::Track* origen = nullptr;
    m_ctrl->project().timeline().findClip(m_clipArrastrado, &origen);
    const int fila = FilaEnY(point.y);
    if (origen && fila >= 0) {
        const model::Track* bajoMouse = PistasVisuales()[fila];
        if (bajoMouse->kind == origen->kind) m_pistaFantasma = bajoMouse->id;
    }
    Invalidate(FALSE);
}

void CTimelineWnd::OnLButtonUp(UINT /*nFlags*/, CPoint /*point*/) {
    const Arrastre arrastre = m_arrastre;
    m_arrastre = Arrastre::Nada;
    if (GetCapture() == this) ReleaseCapture();

    if (arrastre == Arrastre::Clip && m_seMovio && m_ctrl) {
        // View -> Controller. Si el destino se solapa, el modelo lo rechaza y el clip
        // conserva su posición original (spec timeline-editing).
        const model::OpResult r = m_ctrl->moveClip(m_clipArrastrado, m_pistaFantasma, m_inicioFantasma);
        if (!r.ok) MessageBeep(MB_ICONWARNING);
        NotificarCambio(r.ok);
    }
    m_seMovio = false;
    Invalidate(FALSE);
}

void CTimelineWnd::OnCaptureChanged(CWnd* pWnd) {
    // Si otra ventana roba la captura (p. ej. Alt+Tab), se cancela el arrastre.
    if (pWnd != this && m_arrastre != Arrastre::Nada) {
        m_arrastre = Arrastre::Nada;
        m_seMovio = false;
        Invalidate(FALSE);
    }
    CWnd::OnCaptureChanged(pWnd);
}
