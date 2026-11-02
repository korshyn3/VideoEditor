// ExportarDlg.cpp
// Capa: VIEW (MFC). Ver ExportarDlg.h.
#include "ExportarDlg.h"
#include "MensajesApp.h"
#include "Utf8.h"

#include <afxdlgs.h>

#include <cwchar>

namespace {

// Opciones de los combos (en este orden).
const model::Container kContenedores[] = {model::Container::Mp4, model::Container::Mkv, model::Container::Mov};
const model::ResolutionPreset kPresets[] = {model::ResolutionPreset::P480, model::ResolutionPreset::P720,
                                            model::ResolutionPreset::P1080, model::ResolutionPreset::Custom};

bool LeerEntero(CWnd* dlg, int id, int& valor) {
    CString texto;
    dlg->GetDlgItemText(id, texto);
    texto.Trim();
    if (texto.IsEmpty()) return false;
    wchar_t* fin = nullptr;
    const long v = std::wcstol(texto.GetString(), &fin, 10);
    if (fin == nullptr || *fin != L'\0') return false;
    valor = static_cast<int>(v);
    return true;
}

} // namespace

BEGIN_MESSAGE_MAP(CExportarDlg, CDialogEx)
    ON_CBN_SELCHANGE(IDC_EXP_CONTENEDOR, &CExportarDlg::OnCambioContenedor)
    ON_CBN_SELCHANGE(IDC_EXP_PRESET, &CExportarDlg::OnCambioPreset)
    ON_BN_CLICKED(IDC_EXP_EXAMINAR, &CExportarDlg::OnExaminar)
    ON_WM_TIMER()
    ON_MESSAGE(WM_APP_EXPORT_DONE, &CExportarDlg::OnExportTerminada)
END_MESSAGE_MAP()

void CExportarDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EXP_CONTENEDOR, m_contenedor);
    DDX_Control(pDX, IDC_EXP_VCODEC, m_codecVideo);
    DDX_Control(pDX, IDC_EXP_ACODEC, m_codecAudio);
    DDX_Control(pDX, IDC_EXP_PRESET, m_preset);
    DDX_Control(pDX, IDC_EXP_PROGRESO, m_progreso);
}

BOOL CExportarDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();
    // Se parte de la última configuración (se guarda con el proyecto).
    const model::ExportSettings& s = m_ctrl.project().exportSettings();
    for (const wchar_t* c : {L"MP4 (.mp4)", L"Matroska (.mkv)", L"QuickTime (.mov)"}) m_contenedor.AddString(c);
    for (const wchar_t* p : {L"480p (854 x 480)", L"720p (1280 x 720)", L"1080p (1920 x 1080)", L"Personalizada"})
        m_preset.AddString(p);
    for (int i = 0; i < 3; ++i)
        if (kContenedores[i] == s.container) m_contenedor.SetCurSel(i);
    for (int i = 0; i < 4; ++i)
        if (kPresets[i] == s.preset) m_preset.SetCurSel(i);
    SetDlgItemInt(IDC_EXP_VBITRATE, s.videoBitrateKbps);
    SetDlgItemInt(IDC_EXP_ABITRATE, s.audioBitrateKbps);
    SetDlgItemInt(IDC_EXP_ANCHO, s.width);
    SetDlgItemInt(IDC_EXP_ALTO, s.height);
    SetDlgItemText(IDC_EXP_RUTA, CStringDesdeUtf8(s.outputPath));
    m_progreso.SetRange(0, 1000);

    CWaitCursor espera;  // la primera vez se prueban los encoders
    LlenarCodecs(s.videoCodec, s.audioCodec);
    OnCambioPreset();
    return TRUE;
}

model::Container CExportarDlg::ContenedorElegido() const {
    const int i = m_contenedor.GetCurSel();
    return (i >= 0 && i < 3) ? kContenedores[i] : model::Container::Mp4;
}

void CExportarDlg::LlenarCodecs(const std::string& video, const std::string& audio) {
    const model::Container c = ContenedorElegido();
    m_video = controller::EditorController::encodersPara(c, true);
    m_audio = controller::EditorController::encodersPara(c, false);
    auto llenar = [](CComboBox& combo, const std::vector<engine::EncoderInfo>& lista, const std::string& elegido) {
        combo.ResetContent();
        int sel = 0;
        for (size_t i = 0; i < lista.size(); ++i) {
            combo.AddString(CStringDesdeUtf8(lista[i].descripcion));
            if (lista[i].nombre == elegido) sel = static_cast<int>(i);
        }
        combo.SetCurSel(lista.empty() ? -1 : sel);
    };
    llenar(m_codecVideo, m_video, video);
    llenar(m_codecAudio, m_audio, audio);
    const bool hay = !m_video.empty() && !m_audio.empty();
    GetDlgItem(IDOK)->EnableWindow(hay);
    SetDlgItemText(IDC_EXP_ESTADO, hay ? L"" : L"No hay ninguna combinación de códecs disponible para este contenedor.");
}

void CExportarDlg::OnCambioContenedor() {
    const int v = m_codecVideo.GetCurSel(), a = m_codecAudio.GetCurSel();
    LlenarCodecs(v >= 0 ? m_video[v].nombre : std::string(), a >= 0 ? m_audio[a].nombre : std::string());
    // La extensión del archivo sigue al contenedor.
    CString ruta;
    GetDlgItemText(IDC_EXP_RUTA, ruta);
    if (!ruta.IsEmpty())
        SetDlgItemText(IDC_EXP_RUTA, CStringDesdeUtf8(controller::EditorController::conExtension(
                                         Utf8DesdeCString(ruta), ContenedorElegido())));
}

void CExportarDlg::OnCambioPreset() {
    const bool personalizada = m_preset.GetCurSel() == 3;
    GetDlgItem(IDC_EXP_ANCHO)->EnableWindow(personalizada);
    GetDlgItem(IDC_EXP_ALTO)->EnableWindow(personalizada);
}

void CExportarDlg::OnExaminar() {
    const model::Container c = ContenedorElegido();
    const wchar_t* filtros[] = {L"Video MP4 (*.mp4)|*.mp4||", L"Video Matroska (*.mkv)|*.mkv||",
                                L"Video QuickTime (*.mov)|*.mov||"};
    const wchar_t* ext[] = {L"mp4", L"mkv", L"mov"};
    const int i = (c == model::Container::Mkv) ? 1 : (c == model::Container::Mov) ? 2 : 0;
    CString actual;
    GetDlgItemText(IDC_EXP_RUTA, actual);
    // OFN_OVERWRITEPROMPT no: la confirmación se pide al exportar (también si se escribe a mano).
    CFileDialog dlg(FALSE, ext[i], actual.IsEmpty() ? L"salida" : actual.GetString(), OFN_HIDEREADONLY | OFN_EXPLORER,
                    filtros[i], this);
    if (dlg.DoModal() == IDOK) SetDlgItemText(IDC_EXP_RUTA, dlg.GetPathName());
}

bool CExportarDlg::LeerConfiguracion(model::ExportSettings& s) {
    s = m_ctrl.project().exportSettings();
    s.container = ContenedorElegido();
    const int v = m_codecVideo.GetCurSel(), a = m_codecAudio.GetCurSel();
    if (v < 0 || a < 0) return false;
    s.videoCodec = m_video[v].nombre;
    s.audioCodec = m_audio[a].nombre;
    s.preset = kPresets[std::max(0, m_preset.GetCurSel())];
    if (!LeerEntero(this, IDC_EXP_VBITRATE, s.videoBitrateKbps) || !LeerEntero(this, IDC_EXP_ABITRATE, s.audioBitrateKbps)) {
        AfxMessageBox(L"Escribe los bitrates en kbps (números enteros).", MB_ICONWARNING);
        return false;
    }
    if (s.preset == model::ResolutionPreset::Custom &&
        (!LeerEntero(this, IDC_EXP_ANCHO, s.width) || !LeerEntero(this, IDC_EXP_ALTO, s.height))) {
        AfxMessageBox(L"Escribe el ancho y el alto en píxeles.", MB_ICONWARNING);
        return false;
    }
    CString ruta;
    GetDlgItemText(IDC_EXP_RUTA, ruta);
    ruta.Trim();
    s.outputPath = controller::EditorController::conExtension(Utf8DesdeCString(ruta), s.container);
    return true;
}

void CExportarDlg::OnOK() {
    if (m_ctrl.exportando()) return;
    model::ExportSettings s;
    if (!LeerConfiguracion(s)) return;
    SetDlgItemText(IDC_EXP_RUTA, CStringDesdeUtf8(s.outputPath));
    if (const model::OpResult r = m_ctrl.validarExportacion(s); !r) {
        AfxMessageBox(CStringDesdeUtf8(r.error), MB_ICONWARNING);
        return;
    }
    if (controller::EditorController::salidaExiste(s.outputPath) &&
        AfxMessageBox(L"El archivo ya existe:\r\n" + CStringDesdeUtf8(s.outputPath) + L"\r\n\r\n¿Reemplazarlo?",
                      MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    // El aviso llega desde el hilo de exportación: solo PostMessage, nunca tocar la ventana.
    const HWND ventana = GetSafeHwnd();
    const model::OpResult r = m_ctrl.startExport(s, [ventana] { ::PostMessage(ventana, WM_APP_EXPORT_DONE, 0, 0); });
    if (!r) {
        AfxMessageBox(CStringDesdeUtf8(r.error), MB_ICONWARNING);
        return;
    }
    ModoExportando(true);
    SetTimer(kTimerProgreso, 100, nullptr);
}

void CExportarDlg::ModoExportando(bool exportando) {
    for (int id : {IDC_EXP_CONTENEDOR, IDC_EXP_VCODEC, IDC_EXP_ACODEC, IDC_EXP_VBITRATE, IDC_EXP_ABITRATE, IDC_EXP_PRESET,
                   IDC_EXP_RUTA, IDC_EXP_EXAMINAR, IDOK})
        GetDlgItem(id)->EnableWindow(!exportando);
    if (!exportando) OnCambioPreset();
    else {
        GetDlgItem(IDC_EXP_ANCHO)->EnableWindow(FALSE);
        GetDlgItem(IDC_EXP_ALTO)->EnableWindow(FALSE);
    }
    SetDlgItemText(IDCANCEL, exportando ? L"Cancelar" : L"Cerrar");
    SetDlgItemText(IDC_EXP_ESTADO, exportando ? L"Exportando…" : L"");
    if (exportando) m_progreso.SetPos(0);
}

void CExportarDlg::OnTimer(UINT_PTR nIDEvent) {
    if (nIDEvent == kTimerProgreso) {
        const double p = m_ctrl.progresoExport();
        m_progreso.SetPos(static_cast<int>(p * 1000));
        CString t;
        t.Format(L"Exportando… %d %%", static_cast<int>(p * 100));
        SetDlgItemText(IDC_EXP_ESTADO, t);
        return;
    }
    CDialogEx::OnTimer(nIDEvent);
}

LRESULT CExportarDlg::OnExportTerminada(WPARAM, LPARAM) {
    KillTimer(kTimerProgreso);
    const engine::ExportResult r = m_ctrl.resultadoExport();
    ModoExportando(false);
    switch (r.estado) {
    case engine::ExportEstado::Terminado:
        m_progreso.SetPos(1000);
        AfxMessageBox(L"Exportación terminada:\r\n" + CStringDesdeUtf8(m_ctrl.project().exportSettings().outputPath),
                      MB_ICONINFORMATION);
        EndDialog(IDOK);
        break;
    case engine::ExportEstado::Cancelado:
        SetDlgItemText(IDC_EXP_ESTADO, L"Exportación cancelada (el archivo parcial se borró).");
        AfxMessageBox(L"La exportación se canceló y el archivo parcial se borró.", MB_ICONINFORMATION);
        break;
    default:
        SetDlgItemText(IDC_EXP_ESTADO, L"La exportación falló.");
        AfxMessageBox(L"La exportación falló:\r\n" + CStringDesdeUtf8(r.error), MB_ICONERROR);
        break;
    }
    return 0;
}

void CExportarDlg::OnCancel() {
    // Durante la exportación, Cancelar (o Esc, o la X) la detiene; el diálogo se queda
    // abierto hasta que llega el aviso de que terminó.
    if (m_ctrl.exportando()) {
        m_ctrl.cancelExport();
        SetDlgItemText(IDC_EXP_ESTADO, L"Cancelando…");
        return;
    }
    CDialogEx::OnCancel();
}
