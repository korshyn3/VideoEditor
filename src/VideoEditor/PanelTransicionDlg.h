// PanelTransicionDlg.h
// Capa: VIEW (MFC). Panel de propiedades "Transición" (diálogo hijo IDD_PROP_TRANSICION,
// sin borde, que se coloca dentro del grupo "Propiedades" cuando hay una transición
// seleccionada en el timeline).
//
// Permite cambiar el estilo (fundido, barrido, deslizamiento, círculo) y la dirección
// (izquierda/derecha, solo para barrido y deslizamiento), que se aplican al elegirlos;
// la duración en segundos ("Aplicar" o Enter), y quitar la transición. Cada cambio se
// pide al controller y después se avisa a la ventana principal con WM_APP_VISTA_CAMBIO.
#pragma once

#ifndef __AFXWIN_H__
#include <afxwin.h>
#endif
#include <afxdialogex.h>

#include "resource.h"
#include "EditorController/EditorController.h"

class CPanelTransicionDlg : public CDialogEx {
public:
    CPanelTransicionDlg() : CDialogEx(IDD_PROP_TRANSICION) {}
    enum { IDD = IDD_PROP_TRANSICION };

    void SetController(controller::EditorController* controller) { m_ctrl = controller; }
    // Muestra la transición que sale de `fromClipId`. Los controles solo se reescriben
    // si la transición cambió, para no borrar lo que el usuario está escribiendo.
    void MostrarTransicion(model::Id fromClipId, const model::Transition& tr, const CString& info);

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    // Enter en el cuadro de duración la aplica.
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    // Un diálogo hijo no debe cerrarse con Enter/Esc.
    virtual void OnOK() {}
    virtual void OnCancel() {}

    afx_msg void OnCambioEstilo();
    afx_msg void OnAplicar();
    afx_msg void OnQuitar();
    DECLARE_MESSAGE_MAP()

private:
    // Estilo del modelo según los dos combos (y al revés).
    model::TransitionStyle EstiloElegido() const;
    void MostrarEstilo(model::TransitionStyle estilo);
    // Pide el cambio con el estilo de los combos y la duración dada.
    void Aplicar(model::TimeUs duracion);
    void Notificar(const model::OpResult& r);

    controller::EditorController* m_ctrl = nullptr;
    model::Id m_fromClipId = model::kSinId;
    model::Transition m_mostrada;  // lo que muestran los controles

    CComboBox m_estilo;
    CComboBox m_direccion;
};
