// EditorDlg.cpp
// Capa: VIEW (MFC). Ver EditorDlg.h.
#include "EditorDlg.h"
#include "MensajesApp.h"
#include "Utf8.h"

#include <afxdlgs.h>

#include <vector>

namespace {

CString TextoTiempo(model::TimeUs t) {
    return CStringDesdeUtf8(model::formatearTiempo(t));
}

// Información de un medio fuente (presentación pura).
CString TextoInfoMedio(const model::MediaInfo& info) {
    CString t;
    t += L"Archivo: " + CStringDesdeUtf8(info.path) + L"\r\n";
    t += L"Duración: " + TextoTiempo(info.duration) + L"\r\n";
    if (info.hasVideo) {
        CString v;
        v.Format(L"Video: %s, %d x %d, %.3f fps\r\n", CStringDesdeUtf8(info.videoCodec).GetString(), info.width,
                 info.height, info.fps.toDouble());
        t += v;
    } else {
        t += L"Video: (sin video)\r\n";
    }
    if (info.hasAudio) {
        CString a;
        a.Format(L"Audio: %s, %d Hz, %d canal(es)\r\n", CStringDesdeUtf8(info.audioCodec).GetString(),
                 info.sampleRate, info.channels);
        t += a;
    } else {
        t += L"Audio: (sin audio)\r\n";
    }
    return t;
}

} // namespace

BEGIN_MESSAGE_MAP(CEditorDlg, CDialogEx)
    ON_WM_CLOSE()
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_BN_CLICKED(IDC_BTN_IMPORTAR, &CEditorDlg::OnImportar)
    ON_COMMAND(ID_ARCHIVO_IMPORTAR, &CEditorDlg::OnImportar)
    ON_COMMAND(ID_ARCHIVO_SALIR, &CEditorDlg::OnSalir)
    ON_COMMAND(ID_PISTAS_AGREGAR_VIDEO, &CEditorDlg::OnAgregarPistaVideo)
    ON_COMMAND(ID_PISTAS_AGREGAR_AUDIO, &CEditorDlg::OnAgregarPistaAudio)
    ON_COMMAND(ID_PISTAS_ELIMINAR, &CEditorDlg::OnEliminarPista)
    ON_COMMAND(ID_AYUDA_ACERCA, &CEditorDlg::OnAcercaDe)
    ON_MESSAGE(WM_APP_VISTA_CAMBIO, &CEditorDlg::OnVistaCambio)
END_MESSAGE_MAP()

CEditorDlg::CEditorDlg(controller::EditorController& controller, CWnd* pParent)
    : CDialogEx(IDD_EDITOR, pParent), m_controller(controller) {
}

void CEditorDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
}

BOOL CEditorDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();

    m_aceleradores = ::LoadAccelerators(AfxGetResourceHandle(), MAKEINTRESOURCE(IDR_ACCEL_PRINCIPAL));

    // Los "Custom Control" del .rc ya existen como ventanas; se enlazan con sus clases
    // C++ (CPreviewWnd, CTimelineWnd) para que MFC les envíe los mensajes.
    m_preview.SubclassDlgItem(IDC_PREVIEW, this);
    m_timeline.SubclassDlgItem(IDC_TIMELINE, this);
    m_timeline.SetController(&m_controller);

    // Panel de propiedades: diálogo hijo colocado dentro del grupo "Propiedades".
    m_panelMedio.Create(IDD_PROP_MEDIO, this);
    m_panelMedio.ShowWindow(SW_SHOW);

    // El tamaño de diseño es el mínimo: más chico, los controles se encimarían.
    CRect rc;
    GetWindowRect(&rc);
    m_tamanoMinimo = rc.Size();

    ColocarPanelPropiedades();
    RefrescarVista();

    // El foco inicial va al timeline (para la rueda del mouse y los atajos). Se devuelve
    // FALSE porque el foco ya se fijó a mano.
    m_timeline.SetFocus();
    return FALSE;
}

BOOL CEditorDlg::PreTranslateMessage(MSG* pMsg) {
    // Los atajos de teclado (Ctrl+I, ...) se traducen antes de que los procese el control
    // que tiene el foco.
    if (m_aceleradores && ::TranslateAccelerator(m_hWnd, m_aceleradores, pMsg)) return TRUE;
    return CDialogEx::PreTranslateMessage(pMsg);
}

void CEditorDlg::OnClose() {
    // (Fase 10: aquí se preguntará por los cambios sin guardar.)
    EndDialog(IDOK);
}

void CEditorDlg::OnSalir() {
    PostMessage(WM_CLOSE);
}

void CEditorDlg::OnSize(UINT nType, int cx, int cy) {
    CDialogEx::OnSize(nType, cx, cy);  // aplica el diseño dinámico
    ColocarPanelPropiedades();
}

void CEditorDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI) {
    CDialogEx::OnGetMinMaxInfo(lpMMI);
    if (m_tamanoMinimo.cx > 0) {
        lpMMI->ptMinTrackSize.x = m_tamanoMinimo.cx;
        lpMMI->ptMinTrackSize.y = m_tamanoMinimo.cy;
    }
}

void CEditorDlg::ColocarPanelPropiedades() {
    CWnd* grupo = GetDlgItem(IDC_GRUPO_PROPIEDADES);
    if (!grupo || !m_panelMedio.GetSafeHwnd()) return;
    CRect rc;
    grupo->GetWindowRect(&rc);
    ScreenToClient(&rc);
    rc.DeflateRect(8, 20, 8, 8);  // dejar visible el borde y el título del grupo
    m_panelMedio.SetWindowPos(&wndTop, rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOACTIVATE);
}

// ---------------------------------------------------------------------------
// Acciones del usuario (View -> Controller)
// ---------------------------------------------------------------------------

void CEditorDlg::OnImportar() {
    CFileDialog dlg(TRUE, nullptr, nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_EXPLORER,
                    L"Archivos multimedia|*.mp4;*.mkv;*.mov;*.avi;*.webm;*.m4v;*.mp3;*.wav;*.aac;*.m4a|"
                    L"Video|*.mp4;*.mkv;*.mov;*.avi;*.webm;*.m4v|"
                    L"Audio|*.mp3;*.wav;*.aac;*.m4a|"
                    L"Todos los archivos (*.*)|*.*||",
                    this);
    // Búfer grande para que quepan muchas rutas en la selección múltiple.
    std::vector<wchar_t> bufer(64 * 1024, L'\0');
    dlg.GetOFN().lpstrFile = bufer.data();
    dlg.GetOFN().nMaxFile = static_cast<DWORD>(bufer.size());
    if (dlg.DoModal() != IDOK) return;

    std::vector<std::string> rutas;
    for (POSITION pos = dlg.GetStartPosition(); pos != nullptr;)
        rutas.push_back(Utf8DesdeCString(dlg.GetNextPathName(pos)));

    CWaitCursor espera;  // analizar archivos grandes puede tardar un momento
    const controller::ImportResult r = m_controller.importFiles(rutas);
    RefrescarVista();

    if (!r.errores.empty()) {
        CString msg;
        msg.Format(L"Se importaron %d de %d archivo(s). Problemas:\r\n\r\n", static_cast<int>(r.clips.size()),
                   static_cast<int>(rutas.size()));
        for (const std::string& e : r.errores) msg += L"• " + CStringDesdeUtf8(e) + L"\r\n";
        AfxMessageBox(msg, MB_ICONWARNING);
    }
}

void CEditorDlg::OnAgregarPistaVideo() {
    MostrarResultado(m_controller.addTrack(model::TrackKind::Video));
}

void CEditorDlg::OnAgregarPistaAudio() {
    MostrarResultado(m_controller.addTrack(model::TrackKind::Audio));
}

void CEditorDlg::OnEliminarPista() {
    const model::Track* pista = m_controller.project().timeline().findTrack(m_controller.seleccion().trackId);
    if (!pista) return;
    if (!pista->clips.empty()) {
        CString pregunta;
        pregunta.Format(L"La pista %s tiene %d clip(s). ¿Eliminarla junto con sus clips?",
                        CStringDesdeUtf8(pista->name).GetString(), static_cast<int>(pista->clips.size()));
        if (AfxMessageBox(pregunta, MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    }
    MostrarResultado(m_controller.removeTrack(pista->id));
}

void CEditorDlg::OnAcercaDe() {
    AfxMessageBox(L"Editor de video\r\nProyecto académico: C++20, MFC y FFmpeg (arquitectura MVC).",
                  MB_ICONINFORMATION);
}

LRESULT CEditorDlg::OnVistaCambio(WPARAM, LPARAM) {
    // El timeline ya se redibujó; se actualiza el resto de la interfaz.
    ActualizarPropiedades();
    return 0;
}

void CEditorDlg::MostrarResultado(const model::OpResult& r) {
    RefrescarVista();
    if (!r.ok) AfxMessageBox(CStringDesdeUtf8(r.error), MB_ICONWARNING);
}

// ---------------------------------------------------------------------------
// Actualización de la vista (Model -> View)
// ---------------------------------------------------------------------------

void CEditorDlg::RefrescarVista() {
    m_timeline.Refrescar();
    ActualizarPropiedades();
}

void CEditorDlg::ActualizarPropiedades() {
    const model::Project& p = m_controller.project();
    const controller::Seleccion& sel = m_controller.seleccion();

    // Tiempo del cursor / duración total.
    SetDlgItemText(IDC_STATIC_TIEMPO, TextoTiempo(m_controller.playhead()) + L" / " +
                                          TextoTiempo(p.timeline().duration()));

    CString t;
    const model::Track* pista = nullptr;
    if (const model::Clip* clip = p.timeline().findClip(sel.clipId, &pista)) {
        t += L"CLIP (pista " + CStringDesdeUtf8(pista->name) + L")\r\n";
        t += L"Posición: " + TextoTiempo(clip->timelineStart) + L" → " + TextoTiempo(clip->timelineEnd()) + L"\r\n";
        t += L"Tramo de la fuente: " + TextoTiempo(clip->sourceIn) + L" → " + TextoTiempo(clip->sourceOut) + L"\r\n";
        t += L"Duración: " + TextoTiempo(clip->duration()) + L"\r\n";
        if (pista->kind == model::TrackKind::Video)
            t += clip->linkedAudio ? L"Audio vinculado: sí\r\n" : L"Audio vinculado: no\r\n";
        t += L"\r\nMEDIO FUENTE\r\n";
        if (const model::MediaInfo* info = p.media().find(clip->mediaId)) t += TextoInfoMedio(*info);
    } else if ((pista = p.timeline().findTrack(sel.trackId)) != nullptr) {
        CString linea;
        linea.Format(L"PISTA %s (%s)\r\nClips: %d\r\nTermina en: %s\r\n", CStringDesdeUtf8(pista->name).GetString(),
                     pista->kind == model::TrackKind::Video ? L"video" : L"audio",
                     static_cast<int>(pista->clips.size()), TextoTiempo(pista->end()).GetString());
        t += linea;
    }

    CString proyecto;
    proyecto.Format(L"\r\nPROYECTO\r\nLienzo: %d x %d, %.3f fps\r\nDuración: %s\r\n", p.canvas().width,
                    p.canvas().height, p.canvas().fps.toDouble(), TextoTiempo(p.timeline().duration()).GetString());
    t += proyecto;

    m_panelMedio.MostrarTexto(t);
}
