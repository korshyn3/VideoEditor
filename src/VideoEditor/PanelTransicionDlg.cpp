// PanelTransicionDlg.cpp
// Capa: VIEW (MFC). Ver PanelTransicionDlg.h.
#include "PanelTransicionDlg.h"
#include "MensajesApp.h"
#include "Utf8.h"

#include <cmath>
#include <cwchar>

namespace {

using model::TransitionStyle;

// Opciones de los combos (en este orden).
enum { kFundido, kBarrido, kDeslizamiento, kCirculo };
enum { kIzquierda, kDerecha };

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

BEGIN_MESSAGE_MAP(CPanelTransicionDlg, CDialogEx)
    ON_CBN_SELCHANGE(IDC_TR_ESTILO, &CPanelTransicionDlg::OnCambioEstilo)
    ON_CBN_SELCHANGE(IDC_TR_DIRECCION, &CPanelTransicionDlg::OnCambioEstilo)
    ON_BN_CLICKED(IDC_TR_APLICAR, &CPanelTransicionDlg::OnAplicar)
    ON_BN_CLICKED(IDC_TR_QUITAR, &CPanelTransicionDlg::OnQuitar)
END_MESSAGE_MAP()

void CPanelTransicionDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TR_ESTILO, m_estilo);
    DDX_Control(pDX, IDC_TR_DIRECCION, m_direccion);
}

BOOL CPanelTransicionDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();
    for (const wchar_t* e : {L"Fundido", L"Barrido", L"Deslizamiento", L"Círculo"}) m_estilo.AddString(e);
    for (const wchar_t* d : {L"Hacia la izquierda", L"Hacia la derecha"}) m_direccion.AddString(d);
    MostrarEstilo(TransitionStyle::Dissolve);
    return TRUE;
}

BOOL CPanelTransicionDlg::PreTranslateMessage(MSG* pMsg) {
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN) {
        const CWnd* foco = GetFocus();
        if (foco && foco->GetDlgCtrlID() == IDC_TR_DURACION) {
            OnAplicar();
            return TRUE;
        }
    }
    return CDialogEx::PreTranslateMessage(pMsg);
}

void CPanelTransicionDlg::MostrarTransicion(model::Id fromClipId, const model::Transition& tr, const CString& info) {
    if (fromClipId != m_fromClipId || tr.style != m_mostrada.style || tr.duration != m_mostrada.duration) {
        m_fromClipId = fromClipId;
        m_mostrada = tr;
        MostrarEstilo(tr.style);
        CString d;
        d.Format(L"%.3f", model::tiempoASegundos(tr.duration));
        SetDlgItemText(IDC_TR_DURACION, d);
    }
    CString actual;
    GetDlgItemText(IDC_TR_INFO, actual);
    if (actual != info) SetDlgItemText(IDC_TR_INFO, info);
}

model::TransitionStyle CPanelTransicionDlg::EstiloElegido() const {
    const bool derecha = m_direccion.GetCurSel() == kDerecha;
    switch (m_estilo.GetCurSel()) {
    case kBarrido: return derecha ? TransitionStyle::WipeRight : TransitionStyle::WipeLeft;
    case kDeslizamiento: return derecha ? TransitionStyle::SlideRight : TransitionStyle::SlideLeft;
    case kCirculo: return TransitionStyle::Circle;
    default: return TransitionStyle::Dissolve;
    }
}

void CPanelTransicionDlg::MostrarEstilo(model::TransitionStyle estilo) {
    int e = kFundido, d = kIzquierda;
    switch (estilo) {
    case TransitionStyle::WipeLeft: e = kBarrido; break;
    case TransitionStyle::WipeRight: e = kBarrido; d = kDerecha; break;
    case TransitionStyle::SlideLeft: e = kDeslizamiento; break;
    case TransitionStyle::SlideRight: e = kDeslizamiento; d = kDerecha; break;
    case TransitionStyle::Circle: e = kCirculo; break;
    default: break;
    }
    m_estilo.SetCurSel(e);
    m_direccion.SetCurSel(d);
    // La dirección solo tiene sentido en barrido y deslizamiento.
    m_direccion.EnableWindow(e == kBarrido || e == kDeslizamiento);
}

void CPanelTransicionDlg::Notificar(const model::OpResult& r) {
    if (CWnd* padre = GetParent()) padre->SendMessage(WM_APP_VISTA_CAMBIO, r.ok ? 1 : 0);
    if (!r.ok) AfxMessageBox(CStringDesdeUtf8(r.error), MB_ICONWARNING);
}

void CPanelTransicionDlg::Aplicar(model::TimeUs duracion) {
    if (!m_ctrl || m_fromClipId == model::kSinId) return;
    // View -> Controller. El modelo valida la duración y corre los clips siguientes.
    const model::OpResult r = m_ctrl->setTransition(m_fromClipId, EstiloElegido(), duracion);
    m_mostrada.duration = -1;  // volver a mostrar lo que quedó (también si falló)
    Notificar(r);
}

void CPanelTransicionDlg::OnCambioEstilo() {
    // Al elegir un estilo se aplica enseguida, con la duración que ya tiene.
    m_direccion.EnableWindow(m_estilo.GetCurSel() == kBarrido || m_estilo.GetCurSel() == kDeslizamiento);
    if (EstiloElegido() != m_mostrada.style) Aplicar(m_mostrada.duration);
}

void CPanelTransicionDlg::OnAplicar() {
    model::TimeUs duracion = 0;
    if (!LeerSegundos(this, IDC_TR_DURACION, duracion)) {
        AfxMessageBox(L"Escribe la duración en segundos (por ejemplo 1.5).", MB_ICONWARNING);
        return;
    }
    Aplicar(duracion);
}

void CPanelTransicionDlg::OnQuitar() {
    if (!m_ctrl) return;
    Notificar(m_ctrl->removeSelectedTransition());
}
