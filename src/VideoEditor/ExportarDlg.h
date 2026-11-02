// ExportarDlg.h
// Capa: VIEW (MFC). Diálogo modal "Exportar" (IDD_EXPORTAR).
//
// El usuario elige el contenedor (mp4, mkv, mov), los códecs (solo los que tienen encoder
// en este equipo y admite el contenedor), los bitrates, la resolución (presets o
// personalizada) y el archivo de salida. "Exportar" valida (y pide confirmación si el
// archivo existe) y arranca la exportación en segundo plano a través del controller:
//   - un SetTimer lee el progreso y lo muestra en la barra (la ventana sigue respondiendo);
//   - "Cancelar" detiene la exportación (el archivo parcial se borra);
//   - al terminar, el hilo de exportación avisa con PostMessage(WM_APP_EXPORT_DONE).
#pragma once

#ifndef __AFXWIN_H__
#include <afxwin.h>
#endif
#include <afxcmn.h>
#include <afxdialogex.h>

#include "resource.h"
#include "EditorController/EditorController.h"

#include <vector>

class CExportarDlg : public CDialogEx {
public:
    CExportarDlg(controller::EditorController& controller, CWnd* pParent = nullptr)
        : CDialogEx(IDD_EXPORTAR, pParent), m_ctrl(controller) {}
    enum { IDD = IDD_EXPORTAR };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void OnOK();      // "Exportar"
    virtual void OnCancel();  // "Cerrar" o, durante la exportación, "Cancelar"

    afx_msg void OnCambioContenedor();
    afx_msg void OnCambioPreset();
    afx_msg void OnExaminar();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg LRESULT OnExportTerminada(WPARAM wParam, LPARAM lParam);
    DECLARE_MESSAGE_MAP()

private:
    // Llena los combos de códecs con los disponibles para el contenedor elegido,
    // conservando la elección si sigue siendo válida.
    void LlenarCodecs(const std::string& video, const std::string& audio);
    model::Container ContenedorElegido() const;
    // Lee los controles. false (con mensaje) si un número no es válido.
    bool LeerConfiguracion(model::ExportSettings& s);
    // Durante la exportación solo se puede cancelar.
    void ModoExportando(bool exportando);

    static constexpr UINT_PTR kTimerProgreso = 1;

    controller::EditorController& m_ctrl;
    std::vector<engine::EncoderInfo> m_video, m_audio;  // lo que muestran los combos
    CComboBox m_contenedor, m_codecVideo, m_codecAudio, m_preset;
    CProgressCtrl m_progreso;
};
