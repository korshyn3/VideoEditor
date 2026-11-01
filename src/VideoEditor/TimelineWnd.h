// TimelineWnd.h
// Capa: VIEW (MFC). Control propio que dibuja el timeline multipista ("Custom Control"
// en el .rc con la clase de ventana "VETimelineWnd"). Igual que el lienzo de CambiarBMP,
// se dibuja a mano en OnPaint y se maneja con el mouse.
//
//   +----------+------------------------------------------+
//   |          | regla de tiempo (clic/arrastre = cursor) |
//   +----------+------------------------------------------+
//   | V2       | [clip]        [clip]                     |   <- pistas de video,
//   | V1       | [clip][clip]                             |      la de arriba se ve encima
//   | A1       |   [clip de audio]                        |   <- pistas de audio
//   +----------+------------------------------------------+
//
// Regla MVC: lee el proyecto a través del controller (solo lectura) y pide los cambios
// (seleccionar, mover clip, mover cursor) al controller. Después avisa a la ventana padre
// con WM_APP_VISTA_CAMBIO para que actualice el resto de la interfaz.
#pragma once

#ifndef __AFXWIN_H__
#include <afxwin.h>
#endif

#include "EditorController/EditorController.h"
#include "EscalaTiempo.h"

#include <vector>

class CTimelineWnd : public CWnd {
public:
    static constexpr const wchar_t* kClase = L"VETimelineWnd";
    static BOOL RegistrarClase();

    void SetController(controller::EditorController* controller) {
        m_ctrl = controller;
        m_escala.setOrigenX(kAnchoCabecera);
    }
    // Llamar después de cualquier cambio del modelo: ajusta la barra de scroll y redibuja.
    void Refrescar();
    // Desplaza la vista si el cursor de reproducción salió del área visible.
    void SeguirCursor();

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnCaptureChanged(CWnd* pWnd);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    DECLARE_MESSAGE_MAP()

private:
    static constexpr int kAnchoCabecera = 80;  // columna con los nombres de pista
    static constexpr int kAltoRegla = 24;
    static constexpr int kAltoPista = 48;
    static constexpr int kUmbralArrastre = 4;  // px antes de considerar que es un arrastre

    enum class Arrastre { Nada, Cursor, Clip };

    // Pistas en el orden en que se dibujan de arriba a abajo: video de la más alta a V1,
    // después las de audio.
    std::vector<const model::Track*> PistasVisuales() const;
    // Fila (índice en PistasVisuales) bajo la coordenada y, o -1.
    int FilaEnY(int y) const;
    CRect RectFila(int fila) const;
    CRect RectClip(int fila, const model::Clip& clip) const;
    // Clip bajo el punto (o nullptr) y la fila en que está.
    const model::Clip* ClipEn(CPoint p, int* fila) const;

    void Dibujar(CDC& dc, const CRect& rc);
    void DibujarRegla(CDC& dc, const CRect& rc);
    void DibujarClip(CDC& dc, const CRect& rcClip, const model::Clip& clip, bool video, bool seleccionado);

    void MoverCursorA(int x);
    void ActualizarScroll();
    void NotificarCambio(bool cambioModelo);

    controller::EditorController* m_ctrl = nullptr;
    EscalaTiempo m_escala;

    // Estado del arrastre en curso.
    Arrastre m_arrastre = Arrastre::Nada;
    CPoint m_puntoInicio;
    bool m_seMovio = false;
    model::Id m_clipArrastrado = model::kSinId;
    model::TimeUs m_inicioOriginal = 0;
    model::TimeUs m_inicioFantasma = 0;      // posición propuesta durante el arrastre
    model::Id m_pistaFantasma = model::kSinId;
};
