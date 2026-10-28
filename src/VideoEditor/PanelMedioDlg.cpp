// PanelMedioDlg.cpp
// Capa: VIEW (MFC). Ver PanelMedioDlg.h.
#include "PanelMedioDlg.h"

void CPanelMedioDlg::MostrarTexto(const CString& texto) {
    CString actual;
    GetDlgItemText(IDC_PROP_INFO, actual);
    if (actual != texto) {  // evita parpadeo y que se pierda el scroll si no cambió
        SetDlgItemText(IDC_PROP_INFO, texto);
        SendDlgItemMessage(IDC_PROP_INFO, EM_SETSEL, 0, 0);  // sin texto seleccionado
    }
}
