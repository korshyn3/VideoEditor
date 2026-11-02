// PanelClipDlg.h
// Capa: VIEW (MFC). Panel de propiedades "Clip" (diálogo hijo IDD_PROP_CLIP, sin borde,
// que se coloca dentro del grupo "Propiedades" cuando hay un clip seleccionado).
//
// Muestra la información del clip y permite recortarlo con valores numéricos: punto de
// entrada y de salida dentro de la fuente, en segundos. "Aplicar" convierte el texto y
// llama al controller; después avisa a la ventana principal con WM_APP_VISTA_CAMBIO.
// (Las fases 6 y 8 agregan aquí las secciones de efectos y de PiP.)
#pragma once

#ifndef __AFXWIN_H__
#include <afxwin.h>
#endif
#include <afxdialogex.h>

#include "resource.h"
#include "EditorController/EditorController.h"

class CPanelClipDlg : public CDialogEx {
public:
    CPanelClipDlg() : CDialogEx(IDD_PROP_CLIP) {}
    enum { IDD = IDD_PROP_CLIP };

    void SetController(controller::EditorController* controller) { m_ctrl = controller; }
    // Muestra el clip. Los cuadros de entrada/salida solo se reescriben si el clip o su
    // tramo cambiaron, para no borrar lo que el usuario está escribiendo.
    void MostrarClip(const model::Clip& clip, const CString& info);
    // Con varios clips seleccionados el recorte no está disponible.
    void HabilitarRecorte(bool habilitar);

protected:
    // Un diálogo hijo no debe cerrarse con Enter/Esc.
    virtual void OnOK() {}
    virtual void OnCancel() {}

    afx_msg void OnAplicar();
    DECLARE_MESSAGE_MAP()

private:
    controller::EditorController* m_ctrl = nullptr;
    model::Id m_clipId = model::kSinId;
    model::TimeUs m_inMostrado = -1;
    model::TimeUs m_outMostrado = -1;
};
