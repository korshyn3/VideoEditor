// EditorDlg.h
// Capa: VIEW (MFC). Ventana principal del editor (diálogo IDD_EDITOR del .rc).
//
// Contiene la barra de botones, el preview (CPreviewWnd), el panel de propiedades (un
// diálogo hijo: CPanelClipDlg si hay un clip seleccionado, CPanelMedioDlg si no) y el
// timeline (CTimelineWnd). El diseño se edita en el Editor de
// recursos; el "diseño dinámico" (AFX_DIALOG_LAYOUT) reacomoda los controles al cambiar
// el tamaño de la ventana.
//
// Regla MVC: los handlers de esta clase solo convierten datos de los controles y llaman
// al controller. Nunca modifican el modelo ni llaman a FFmpeg.
#pragma once

#ifndef __AFXWIN_H__
#include <afxwin.h>
#endif
#include <afxdialogex.h>

#include "resource.h"
#include "EditorController/EditorController.h"
#include "PanelClipDlg.h"
#include "PanelMedioDlg.h"
#include "PreviewWnd.h"
#include "TimelineWnd.h"

class CEditorDlg : public CDialogEx {
public:
    explicit CEditorDlg(controller::EditorController& controller, CWnd* pParent = nullptr);

    enum { IDD = IDD_EDITOR };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    // Enter y Esc NO deben cerrar el editor: el cierre pasa siempre por OnClose.
    virtual void OnOK() {}
    virtual void OnCancel() {}

    afx_msg void OnClose();
    afx_msg void OnDestroy();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnReproducir();
    afx_msg LRESULT OnPreviewAviso(WPARAM wParam, LPARAM lParam);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg void OnImportar();
    afx_msg void OnSalir();
    afx_msg void OnAgregarPistaVideo();
    afx_msg void OnAgregarPistaAudio();
    afx_msg void OnEliminarPista();
    afx_msg void OnDividir();
    afx_msg void OnEliminarClip();
    afx_msg void OnEliminarClipRipple();
    afx_msg void OnAcercaDe();
    afx_msg LRESULT OnVistaCambio(WPARAM wParam, LPARAM lParam);
    DECLARE_MESSAGE_MAP()

private:
    // Actualiza todo lo que depende del modelo / selección (timeline, propiedades, tiempo).
    void RefrescarVista();
    void ActualizarPropiedades();
    void ColocarPanelPropiedades();
    void MostrarResultado(const model::OpResult& r);
    // Como MostrarResultado, pero un fallo solo suena (p. ej. dividir con el cursor fuera
    // del clip: la spec dice que no se hace nada).
    void ResultadoSilencioso(const model::OpResult& r);
    // El modelo cambió: el preview se actualiza 100 ms después del ÚLTIMO cambio
    // (SetTimer con el mismo id reinicia la cuenta), así se agrupan los cambios seguidos.
    void ProgramarSincronizacion();
    // Trae del controller el cuadro que toca mostrar y actualiza cursor y botones.
    void ActualizarReproduccion();

    static constexpr UINT_PTR kTimerReproduccion = 1;  // ~60 Hz mientras se reproduce
    static constexpr UINT_PTR kTimerSincronizar = 2;   // debounce de cambios del modelo

    controller::EditorController& m_controller;
    HACCEL m_aceleradores = nullptr;
    CPreviewWnd m_preview;
    CTimelineWnd m_timeline;
    CPanelMedioDlg m_panelMedio;
    CPanelClipDlg m_panelClip;
    CSize m_tamanoMinimo;
};
