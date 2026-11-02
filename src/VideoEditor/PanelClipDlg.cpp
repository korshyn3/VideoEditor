// PanelClipDlg.cpp
// Capa: VIEW (MFC). Ver PanelClipDlg.h.
#include "PanelClipDlg.h"
#include "MensajesApp.h"
#include "Utf8.h"

#include <algorithm>
#include <cmath>
#include <cwchar>

namespace {

using Parametro = CPanelClipDlg::Parametro;

CString TextoSegundos(model::TimeUs t) {
    CString s;
    s.Format(L"%.3f", model::tiempoASegundos(t));
    return s;
}

// Lee un número de un cuadro de texto (acepta coma o punto decimal).
bool LeerNumero(CWnd* dlg, int id, double& valor) {
    CString texto;
    dlg->GetDlgItemText(id, texto);
    texto.Trim();
    texto.Replace(L',', L'.');
    if (texto.IsEmpty()) return false;
    wchar_t* fin = nullptr;
    const double v = std::wcstod(texto.GetString(), &fin);
    if (fin == nullptr || *fin != L'\0' || !std::isfinite(v)) return false;
    valor = v;
    return true;
}

bool LeerSegundos(CWnd* dlg, int id, model::TimeUs& t) {
    double s = 0;
    if (!LeerNumero(dlg, id, s)) return false;
    t = model::segundosATiempo(s);
    return true;
}

// Parámetros editables de un efecto (0, 1 o 2). Los fades van en segundos, hasta la
// duración del clip.
int ParametrosDe(const model::Effect& e, model::TimeUs duracionClip, Parametro p[2]) {
    if (const auto* bc = std::get_if<model::BrightnessContrast>(&e)) {
        p[0] = {L"Brillo:", model::BrightnessContrast::kBrilloMin, model::BrightnessContrast::kBrilloMax, 100, bc->brightness};
        p[1] = {L"Contraste:", model::BrightnessContrast::kContrasteMin, model::BrightnessContrast::kContrasteMax, 100,
                bc->contrast};
        return 2;
    }
    if (const auto* b = std::get_if<model::Blur>(&e)) {
        p[0] = {L"Intensidad:", 0, model::Blur::kRadioMax, 10, b->radius};
        return 1;
    }
    if (const auto* f = std::get_if<model::Fade>(&e)) {
        const double dur = model::tiempoASegundos(duracionClip);
        p[0] = {L"Fade in (s):", 0, dur, 100, model::tiempoASegundos(f->fadeIn)};
        p[1] = {L"Fade out (s):", 0, dur, 100, model::tiempoASegundos(f->fadeOut)};
        return 2;
    }
    return 0;  // escala de grises: sin parámetros
}

// El efecto con su parámetro i cambiado a `valor`.
model::Effect ConValor(model::Effect e, int i, double valor) {
    if (auto* bc = std::get_if<model::BrightnessContrast>(&e)) {
        (i == 0 ? bc->brightness : bc->contrast) = valor;
    } else if (auto* b = std::get_if<model::Blur>(&e)) {
        b->radius = valor;
    } else if (auto* f = std::get_if<model::Fade>(&e)) {
        (i == 0 ? f->fadeIn : f->fadeOut) = model::segundosATiempo(valor);
    }
    return e;
}

// Texto de un efecto en la lista: "2. Brillo/contraste (0.30 / 1.00)".
CString TextoEfecto(size_t indice, const model::Effect& e, model::TimeUs duracionClip) {
    CString texto;
    texto.Format(L"%d. %s", static_cast<int>(indice + 1), CStringDesdeUtf8(model::nombreEfecto(e)).GetString());
    Parametro p[2];
    const int n = ParametrosDe(e, duracionClip, p);
    if (n == 1) texto.AppendFormat(L" (%.2f)", p[0].valor);
    if (n == 2) texto.AppendFormat(L" (%.2f / %.2f)", p[0].valor, p[1].valor);
    return texto;
}

// Sección PiP: un slider por valor, en centésimas.
const int kPipEtiqueta[4] = {IDC_PIP_ETQ1, IDC_PIP_ETQ2, IDC_PIP_ETQ3, IDC_PIP_ETQ4};
const int kPipSlider[4] = {IDC_PIP_SLIDER1, IDC_PIP_SLIDER2, IDC_PIP_SLIDER3, IDC_PIP_SLIDER4};
const int kPipValor[4] = {IDC_PIP_VALOR1, IDC_PIP_VALOR2, IDC_PIP_VALOR3, IDC_PIP_VALOR4};

double& ValorPip(model::PipTransform& t, int i) {
    return i == 0 ? t.x : i == 1 ? t.y : i == 2 ? t.scale : t.opacity;
}

const int kEtiqueta[2] = {IDC_FX_ETQ1, IDC_FX_ETQ2};
const int kSlider[2] = {IDC_FX_SLIDER1, IDC_FX_SLIDER2};
const int kValor[2] = {IDC_FX_VALOR1, IDC_FX_VALOR2};

} // namespace

BEGIN_MESSAGE_MAP(CPanelClipDlg, CDialogEx)
    ON_BN_CLICKED(IDC_CLIP_APLICAR, &CPanelClipDlg::OnAplicar)
    ON_BN_CLICKED(IDC_FX_AGREGAR, &CPanelClipDlg::OnAgregarEfecto)
    ON_BN_CLICKED(IDC_FX_SUBIR, &CPanelClipDlg::OnSubirEfecto)
    ON_BN_CLICKED(IDC_FX_BAJAR, &CPanelClipDlg::OnBajarEfecto)
    ON_BN_CLICKED(IDC_FX_QUITAR, &CPanelClipDlg::OnQuitarEfecto)
    ON_LBN_SELCHANGE(IDC_FX_LISTA, &CPanelClipDlg::OnCambioSeleccionEfecto)
    ON_WM_HSCROLL()
    ON_WM_SIZE()
END_MESSAGE_MAP()

void CPanelClipDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_FX_TIPO, m_comboTipo);
    DDX_Control(pDX, IDC_FX_LISTA, m_lista);
    DDX_Control(pDX, IDC_FX_SLIDER1, m_slider[0]);
    DDX_Control(pDX, IDC_FX_SLIDER2, m_slider[1]);
    for (int i = 0; i < 4; ++i) DDX_Control(pDX, kPipSlider[i], m_pip[i]);
}

BOOL CPanelClipDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();
    // Los tipos de efecto, en el orden del variant del modelo (índice = tipo).
    for (int tipo = 0; tipo < model::kTiposEfecto; ++tipo) m_comboTipo.AddString(CStringDesdeUtf8(model::nombreEfecto(tipo)));
    m_comboTipo.SetCurSel(0);
    MostrarEfectos();
    for (int i = 0; i < 4; ++i) m_pip[i].SetRange(i == 2 ? std::lround(model::PipTransform::kEscalaMin * 100) : 0, 100);
    MostrarPip(m_pipMostrado);
    ColocarInfo();
    return TRUE;
}

void CPanelClipDlg::OnSize(UINT nType, int cx, int cy) {
    CDialogEx::OnSize(nType, cx, cy);
    ColocarInfo();
}

void CPanelClipDlg::ColocarInfo() {
    CWnd* info = GetDlgItem(IDC_CLIP_INFO);
    if (!info) return;
    // Debajo de la sección PiP si se ve; si no, debajo de los parámetros de efectos.
    CWnd* ultimo = GetDlgItem(m_seccionPip ? IDC_PIP_SLIDER4 : IDC_FX_VALOR2);
    if (!ultimo) return;
    CRect rcUltimo, rc;
    ultimo->GetWindowRect(&rcUltimo);
    ScreenToClient(&rcUltimo);
    GetClientRect(&rc);
    rc.top = rcUltimo.bottom + 6;
    if (rc.bottom > rc.top) info->MoveWindow(rc);
}

void CPanelClipDlg::MostrarSeccionPip(bool mostrar) {
    if (mostrar == m_seccionPip) return;
    m_seccionPip = mostrar;
    for (int i = 0; i < 4; ++i)
        for (int id : {kPipEtiqueta[i], kPipSlider[i], kPipValor[i]}) GetDlgItem(id)->ShowWindow(mostrar ? SW_SHOW : SW_HIDE);
    ColocarInfo();
}

void CPanelClipDlg::MostrarPip(const model::PipTransform& t) {
    m_pipMostrado = t;
    for (int i = 0; i < 4; ++i) {
        const double v = ValorPip(m_pipMostrado, i);
        m_pip[i].SetPos(std::lround(v * 100));
        CString texto;
        texto.Format(L"%.2f", v);
        SetDlgItemText(kPipValor[i], texto);
    }
}

BOOL CPanelClipDlg::PreTranslateMessage(MSG* pMsg) {
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN) {
        const CWnd* foco = GetFocus();
        const int id = foco ? foco->GetDlgCtrlID() : 0;
        if (id == IDC_FX_VALOR1 || id == IDC_FX_VALOR2) {
            AplicarValorEscrito(id == IDC_FX_VALOR1 ? 0 : 1);
            return TRUE;
        }
        if (id == IDC_CLIP_IN || id == IDC_CLIP_OUT) {
            OnAplicar();
            return TRUE;
        }
    }
    return CDialogEx::PreTranslateMessage(pMsg);
}

void CPanelClipDlg::MostrarClip(const model::Clip& clip, const CString& info) {
    const bool clipNuevo = clip.id != m_clipId;
    if (clipNuevo || clip.sourceIn != m_inMostrado || clip.sourceOut != m_outMostrado) {
        m_clipId = clip.id;
        m_inMostrado = clip.sourceIn;
        m_outMostrado = clip.sourceOut;
        SetDlgItemText(IDC_CLIP_IN, TextoSegundos(clip.sourceIn));
        SetDlgItemText(IDC_CLIP_OUT, TextoSegundos(clip.sourceOut));
    }
    if (clipNuevo || clip.transform != m_pipMostrado) MostrarPip(clip.transform);
    if (clipNuevo || clip.effects != m_efectos || clip.duration() != m_duracionClip) {
        if (clipNuevo) m_efectoElegido = clip.effects.empty() ? -1 : 0;
        m_efectos = clip.effects;
        m_duracionClip = clip.duration();
        m_efectoElegido = std::min(m_efectoElegido, static_cast<int>(m_efectos.size()) - 1);
        MostrarEfectos();
    }
    CString actual;
    GetDlgItemText(IDC_CLIP_INFO, actual);
    if (actual != info) SetDlgItemText(IDC_CLIP_INFO, info);
}

void CPanelClipDlg::HabilitarRecorte(bool habilitar) {
    for (int id : {IDC_CLIP_IN, IDC_CLIP_OUT, IDC_CLIP_APLICAR}) GetDlgItem(id)->EnableWindow(habilitar);
}

void CPanelClipDlg::HabilitarEfectos(bool habilitar) {
    if (habilitar == m_efectosHabilitados) return;
    m_efectosHabilitados = habilitar;
    ActualizarBotonesEfectos();
}

void CPanelClipDlg::MostrarEfectos() {
    // Con la misma cantidad de efectos solo se reescriben los textos que cambiaron (así la
    // lista no parpadea mientras se arrastra un slider).
    if (m_lista.GetCount() != static_cast<int>(m_efectos.size())) {
        m_lista.ResetContent();
        for (size_t i = 0; i < m_efectos.size(); ++i) m_lista.AddString(TextoEfecto(i, m_efectos[i], m_duracionClip));
    } else {
        for (size_t i = 0; i < m_efectos.size(); ++i) {
            const CString texto = TextoEfecto(i, m_efectos[i], m_duracionClip);
            CString actual;
            m_lista.GetText(static_cast<int>(i), actual);
            if (actual != texto) {
                m_lista.DeleteString(static_cast<UINT>(i));
                m_lista.InsertString(static_cast<int>(i), texto);
            }
        }
    }
    m_lista.SetCurSel(m_efectoElegido);
    MostrarParametros();
    ActualizarBotonesEfectos();
}

void CPanelClipDlg::MostrarParametros() {
    Parametro p[2];
    const int n = (m_efectoElegido >= 0) ? ParametrosDe(m_efectos[m_efectoElegido], m_duracionClip, p) : 0;
    for (int i = 0; i < 2; ++i) {
        const int mostrar = (i < n) ? SW_SHOW : SW_HIDE;
        for (int id : {kEtiqueta[i], kSlider[i], kValor[i]}) GetDlgItem(id)->ShowWindow(mostrar);
        if (i >= n) continue;
        SetDlgItemText(kEtiqueta[i], p[i].etiqueta);
        m_slider[i].SetRange(std::lround(p[i].min * p[i].escala), std::lround(p[i].max * p[i].escala), TRUE);
        m_slider[i].SetPos(std::lround(p[i].valor * p[i].escala));
        CString valor;
        valor.Format(L"%.2f", p[i].valor);
        SetDlgItemText(kValor[i], valor);
    }
}

void CPanelClipDlg::ActualizarBotonesEfectos() {
    const bool h = m_efectosHabilitados;
    const int n = static_cast<int>(m_efectos.size());
    const int sel = m_efectoElegido;
    GetDlgItem(IDC_FX_TIPO)->EnableWindow(h);
    GetDlgItem(IDC_FX_AGREGAR)->EnableWindow(h);
    GetDlgItem(IDC_FX_LISTA)->EnableWindow(h);
    GetDlgItem(IDC_FX_SUBIR)->EnableWindow(h && sel > 0);
    GetDlgItem(IDC_FX_BAJAR)->EnableWindow(h && sel >= 0 && sel < n - 1);
    GetDlgItem(IDC_FX_QUITAR)->EnableWindow(h && sel >= 0);
    for (int i = 0; i < 2; ++i) {
        GetDlgItem(kSlider[i])->EnableWindow(h);
        GetDlgItem(kValor[i])->EnableWindow(h);
    }
}

void CPanelClipDlg::Notificar(const model::OpResult& r) {
    if (CWnd* padre = GetParent()) padre->SendMessage(WM_APP_VISTA_CAMBIO, r.ok ? 1 : 0);
    if (!r.ok) AfxMessageBox(CStringDesdeUtf8(r.error), MB_ICONWARNING);
}

void CPanelClipDlg::CambiarParametro(int i, double valor, bool ajustar) {
    if (!m_ctrl || m_efectoElegido < 0) return;
    const size_t indice = static_cast<size_t>(m_efectoElegido);
    model::Effect e = ConValor(m_efectos[indice], i, valor);
    if (auto* f = std::get_if<model::Fade>(&e); f && ajustar) {
        // Con el slider, el fade que se mueve cede ante el otro (nunca superan la duración).
        if (i == 0) f->fadeIn = std::min(f->fadeIn, m_duracionClip - f->fadeOut);
        else f->fadeOut = std::min(f->fadeOut, m_duracionClip - f->fadeIn);
    }
    if (e == m_efectos[indice]) {
        MostrarParametros();  // sin cambios: se vuelve a mostrar el valor actual
        return;
    }
    // View -> Controller. El modelo limita los valores a su rango y rechaza los fades
    // que no caben en el clip.
    Notificar(m_ctrl->setEffect(m_clipId, indice, e));
}

void CPanelClipDlg::AplicarValorEscrito(int i) {
    double valor = 0;
    if (!LeerNumero(this, kValor[i], valor)) {
        AfxMessageBox(L"Escribe un número (por ejemplo 0.5).", MB_ICONWARNING);
        return;
    }
    CambiarParametro(i, valor, false);
    MostrarParametros();  // el valor que quedó (limitado por el modelo, o el anterior si falló)
}

void CPanelClipDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) {
    const int id = pScrollBar ? pScrollBar->GetDlgCtrlID() : 0;
    for (int i = 0; i < 4; ++i) {
        if (id != kPipSlider[i]) continue;
        // View -> Controller, en vivo mientras se arrastra (el modelo limita los valores).
        model::PipTransform t = m_pipMostrado;
        ValorPip(t, i) = m_pip[i].GetPos() / 100.0;
        if (m_ctrl && t != m_pipMostrado) Notificar(m_ctrl->setPipTransform(m_clipId, t));
        return;
    }
    if ((id == IDC_FX_SLIDER1 || id == IDC_FX_SLIDER2) && m_efectoElegido >= 0) {
        const int i = (id == IDC_FX_SLIDER1) ? 0 : 1;
        Parametro p[2];
        if (i < ParametrosDe(m_efectos[m_efectoElegido], m_duracionClip, p))
            CambiarParametro(i, m_slider[i].GetPos() / p[i].escala, true);
        return;
    }
    CDialogEx::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CPanelClipDlg::OnAgregarEfecto() {
    if (!m_ctrl || m_clipId == model::kSinId) return;
    const model::OpResult r = m_ctrl->addEffect(m_clipId, m_comboTipo.GetCurSel());
    if (r.ok) m_efectoElegido = static_cast<int>(m_efectos.size());  // el nuevo, al final
    Notificar(r);
}

void CPanelClipDlg::OnSubirEfecto() {
    if (!m_ctrl || m_efectoElegido <= 0) return;
    const model::OpResult r = m_ctrl->moveEffect(m_clipId, static_cast<size_t>(m_efectoElegido), -1);
    if (r.ok) --m_efectoElegido;
    Notificar(r);
}

void CPanelClipDlg::OnBajarEfecto() {
    if (!m_ctrl || m_efectoElegido < 0) return;
    const model::OpResult r = m_ctrl->moveEffect(m_clipId, static_cast<size_t>(m_efectoElegido), +1);
    if (r.ok) ++m_efectoElegido;
    Notificar(r);
}

void CPanelClipDlg::OnQuitarEfecto() {
    if (!m_ctrl || m_efectoElegido < 0) return;
    Notificar(m_ctrl->removeEffect(m_clipId, static_cast<size_t>(m_efectoElegido)));
}

void CPanelClipDlg::OnCambioSeleccionEfecto() {
    m_efectoElegido = m_lista.GetCurSel();
    MostrarParametros();
    ActualizarBotonesEfectos();
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
    Notificar(r);
}
