// PanelClipDlg.cpp
// Capa: VIEW (MFC). Ver PanelClipDlg.h.
#include "PanelClipDlg.h"
#include "MensajesApp.h"
#include "Utf8.h"

#include <cmath>
#include <cwchar>

namespace {

CString TextoSegundos(model::TimeUs t) {
    CString s;
    s.Format(L"%.3f", model::tiempoASegundos(t));
    return s;
}

// Lee un número de segundos de un cuadro de texto (acepta coma o punto decimal).
bool LeerSegundos(CWnd* dlg, int id, model::TimeUs& t) {
    CString texto;
    dlg->GetDlgItemText(id, texto);
    texto.Trim();
    texto.Replace(L',', L'.');
    if (texto.IsEmpty()) return false;
    wchar_t* fin = nullptr;
    const double s = std::wcstod(texto.GetString(), &fin);
    if (fin == nullptr || *fin != L'\0' || !std::isfinite(s)) return false;
    t = model::segundosATiempo(s);
    return true;
}

} // namespace

BEGIN_MESSAGE_MAP(CPanelClipDlg, CDialogEx)
    ON_BN_CLICKED(IDC_CLIP_APLICAR, &CPanelClipDlg::OnAplicar)
END_MESSAGE_MAP()

void CPanelClipDlg::MostrarClip(const model::Clip& clip, const CString& info) {
    if (clip.id != m_clipId || clip.sourceIn != m_inMostrado || clip.sourceOut != m_outMostrado) {
        m_clipId = clip.id;
        m_inMostrado = clip.sourceIn;
        m_outMostrado = clip.sourceOut;
        SetDlgItemText(IDC_CLIP_IN, TextoSegundos(clip.sourceIn));
        SetDlgItemText(IDC_CLIP_OUT, TextoSegundos(clip.sourceOut));
    }
    CString actual;
    GetDlgItemText(IDC_CLIP_INFO, actual);
    if (actual != info) SetDlgItemText(IDC_CLIP_INFO, info);
}

void CPanelClipDlg::HabilitarRecorte(bool habilitar) {
    for (int id : {IDC_CLIP_IN, IDC_CLIP_OUT, IDC_CLIP_APLICAR}) GetDlgItem(id)->EnableWindow(habilitar);
}

void CPanelClipDlg::OnAplicar() {
    if (!m_ctrl || m_clipId == model::kSinId) return;
    model::TimeUs in = 0, out = 0;
    if (!LeerSegundos(this, IDC_CLIP_IN, in) || !LeerSegundos(this, IDC_CLIP_OUT, out)) {
        AfxMessageBox(L"Escribe la entrada y la salida en segundos (por ejemplo 2.5).", MB_ICONWARNING);
        return;
    }
    // View -> Controller. El modelo limita los valores a la fuente y rechaza solapes.
    const model::OpResult r = m_ctrl->trimClip(m_clipId, in, out);
    m_inMostrado = m_outMostrado = -1;  // volver a mostrar los valores que quedaron
    if (CWnd* padre = GetParent()) padre->SendMessage(WM_APP_VISTA_CAMBIO, r.ok ? 1 : 0);
    if (!r.ok) AfxMessageBox(CStringDesdeUtf8(r.error), MB_ICONWARNING);
}
