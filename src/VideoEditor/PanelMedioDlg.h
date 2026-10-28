// PanelMedioDlg.h
// Capa: VIEW (MFC). Panel de propiedades "Medio" (diálogo hijo IDD_PROP_MEDIO, sin
// borde, que se coloca dentro del grupo "Propiedades" de la ventana principal).
#pragma once

#ifndef __AFXWIN_H__
#include <afxwin.h>
#endif
#include <afxdialogex.h>

#include "resource.h"

class CPanelMedioDlg : public CDialogEx {
public:
    CPanelMedioDlg() : CDialogEx(IDD_PROP_MEDIO) {}
    enum { IDD = IDD_PROP_MEDIO };

    void MostrarTexto(const CString& texto);

protected:
    // Un diálogo hijo no debe cerrarse con Enter/Esc.
    virtual void OnOK() {}
    virtual void OnCancel() {}
};
