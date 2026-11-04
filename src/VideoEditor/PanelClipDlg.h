// PanelClipDlg.h
// Capa: VIEW (MFC). Panel de propiedades "Clip" (diálogo hijo IDD_PROP_CLIP, sin borde,
// que se coloca dentro del grupo "Propiedades" cuando hay un clip seleccionado).
//
// Muestra la información del clip y permite:
//   - recortarlo con valores numéricos: punto de entrada y de salida dentro de la fuente,
//     en segundos ("Aplicar" o Enter);
//   - editar su lista de efectos: agregar (combo + "Agregar"), subir, bajar y quitar, y
//     cambiar los parámetros del efecto elegido con un slider (en vivo) o escribiendo el
//     valor y pulsando Enter;
//   - en cualquier pista de video, cambiar su PiP (posición x/y, escala y opacidad)
//     con sliders, en vivo.
// Cada cambio se pide al controller y después se avisa a la ventana principal con
// WM_APP_VISTA_CAMBIO (que redibuja el timeline y actualiza el preview).
#pragma once

#ifndef __AFXWIN_H__
#include <afxwin.h>
#endif
#include <afxcmn.h>
#include <afxdialogex.h>

#include "resource.h"
#include "EditorController/EditorController.h"

#include <vector>

class CPanelClipDlg : public CDialogEx {
public:
    CPanelClipDlg() : CDialogEx(IDD_PROP_CLIP) {}
    enum { IDD = IDD_PROP_CLIP };

    void SetController(controller::EditorController* controller) { m_ctrl = controller; }
    // Muestra el clip. Los cuadros de entrada/salida y los controles de efectos solo se
    // reescriben si el clip cambió, para no borrar lo que el usuario está escribiendo.
    void MostrarClip(const model::Clip& clip, const CString& info);
    // Con varios clips seleccionados el recorte no está disponible.
    void HabilitarRecorte(bool habilitar);
    // Los efectos solo están disponibles para un clip de video.
    void HabilitarEfectos(bool habilitar);
    // La sección PiP solo se ve con un clip de video (de cualquier pista).
    void MostrarSeccionPip(bool mostrar);

    // Un parámetro editable de un efecto (para el slider y el cuadro de texto).
    struct Parametro {
        const wchar_t* etiqueta = L"";
        double min = 0, max = 1;
        double escala = 100;  // pasos del slider por unidad
        double valor = 0;
    };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    // Enter en un cuadro de texto aplica su valor.
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    // Un diálogo hijo no debe cerrarse con Enter/Esc.
    virtual void OnOK() {}
    virtual void OnCancel() {}

    afx_msg void OnAplicar();
    afx_msg void OnAgregarEfecto();
    afx_msg void OnSubirEfecto();
    afx_msg void OnBajarEfecto();
    afx_msg void OnQuitarEfecto();
    afx_msg void OnCambioSeleccionEfecto();
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    DECLARE_MESSAGE_MAP()

private:
    // Vuelve a llenar la lista y los controles de parámetros con m_efectos.
    void MostrarEfectos();
    void MostrarParametros();
    void ActualizarBotonesEfectos();
    // Cambia el parámetro i del efecto elegido y lo pide al controller. Con `ajustar`
    // (slider), un fade se limita a lo que permite la duración del clip en vez de fallar.
    void CambiarParametro(int i, double valor, bool ajustar);
    void AplicarValorEscrito(int i);
    // Avisa a la ventana principal; si la operación falló, muestra el mensaje.
    void Notificar(const model::OpResult& r);
    void MostrarPip(const model::PipTransform& t);
    // El cuadro de información ocupa desde debajo de la última sección visible hasta el
    // final del panel.
    void ColocarInfo();

    controller::EditorController* m_ctrl = nullptr;
    model::Id m_clipId = model::kSinId;
    model::TimeUs m_inMostrado = -1;
    model::TimeUs m_outMostrado = -1;
    model::TimeUs m_duracionClip = 0;

    std::vector<model::Effect> m_efectos;  // los que se están mostrando
    int m_efectoElegido = -1;              // índice en m_efectos (o -1)
    bool m_efectosHabilitados = true;

    CComboBox m_comboTipo;
    CListBox m_lista;
    CSliderCtrl m_slider[2];

    model::PipTransform m_pipMostrado;
    bool m_seccionPip = true;
    CSliderCtrl m_pip[4];  // x, y, escala, opacidad (en centésimas)
};
