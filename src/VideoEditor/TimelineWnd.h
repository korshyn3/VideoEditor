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
// Debajo de las pistas de video están sus pistas de audio ("Audio V1"...) y al final las
// pistas de audio generales (A1...). Las pistas de audio tienen en su cabecera un botón
// "M" para silenciarlas (mute). Los clips vinculados (video <-> su audio) llevan la marca
// "↔" y se mueven y recortan juntos: el arrastre muestra el fantasma de los dos.
//
// Selección múltiple: Ctrl+clic agrega o quita un clip; arrastrar un clip seleccionado
// mueve todo el grupo (cada clip en su pista). Soltar con Shift = sobrescribir: los clips
// de debajo se recortan para dejar lugar.
//
// Pegado (imán): al mover o recortar, un clip a menos de EscalaTiempo::kImanPx píxeles del
// borde de otro clip (o del 0) se pega a él y se dibuja una línea en ese punto. Su propia
// pista tiene prioridad; si ahí no hay nada cerca, se alinea con las demás pistas.
//
// Con el mouse: clic en la regla = mover el cursor; arrastrar un clip = moverlo;
// arrastrar el borde izquierdo o derecho de un clip = recortarlo (trim); clic derecho en
// un clip = menú contextual (dividir / eliminar / agregar transición), cuyos comandos
// llegan a la ventana padre.
//
// Transiciones: dos clips con transición se dibujan solapados y en el centro del solape
// hay un rombo; clic en el rombo = seleccionar la transición (se edita en el panel de
// propiedades), clic derecho = menú para quitarla. La transición espejo de los audios
// vinculados también lleva rombo y selecciona la misma transición.
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

#include <optional>
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
    // Clip de salida para "Agregar transición": según dónde fue el último clic derecho
    // sobre un clip (mitad derecha = con el siguiente, mitad izquierda = el anterior con
    // él). Si ese clic no fue sobre el clip seleccionado, el clip seleccionado.
    model::Id ClipParaTransicion() const;

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnCaptureChanged(CWnd* pWnd);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    DECLARE_MESSAGE_MAP()

private:
    static constexpr int kAnchoCabecera = 90;  // columna con los nombres de pista
    static constexpr int kAltoRegla = 24;
    static constexpr int kAltoPista = 48;
    static constexpr int kUmbralArrastre = 4;  // px antes de considerar que es un arrastre
    static constexpr int kAnchoBorde = 5;      // px a cada lado del borde de un clip para el trim

    enum class Arrastre { Nada, Cursor, Clip, BordeInicio, BordeFin };

    // Pistas en el orden en que se dibujan de arriba a abajo: video de la más alta a V1,
    // después las de audio.
    std::vector<const model::Track*> PistasVisuales() const;
    // Fila (índice en PistasVisuales) bajo la coordenada y, o -1.
    int FilaEnY(int y) const;
    CRect RectFila(int fila) const;
    CRect RectClip(int fila, const model::Clip& clip) const;
    // Botón de mute en la cabecera de una pista de audio.
    CRect RectMute(int fila) const;
    // Clip bajo el punto (o nullptr) y la fila en que está.
    const model::Clip* ClipEn(CPoint p, int* fila) const;
    // Rombo de una transición de la pista en la fila (vacío si sus clips no están).
    CRect RectTransicion(int fila, const model::Transition& tr) const;
    // Transición cuyo rombo está bajo el punto: devuelve el clip de video del que sale
    // (o kSinId).
    model::Id TransicionEn(CPoint p) const;
    // Si el punto está sobre el borde izquierdo o derecho de un clip, devuelve ese clip y
    // en `inicio` qué borde es (true = izquierdo).
    const model::Clip* BordeEn(CPoint p, bool* inicio) const;
    // Tramo propuesto [in, out) de la fuente al arrastrar un borde dx píxeles, limitado a
    // la fuente, a los clips vecinos y a un cuadro como mínimo.
    void CalcularTrim(int dx);
    // Pista donde quedaría el clip vinculado de `clip` si este se suelta en `pistaDestino`.
    model::Id PistaVinculadoEn(const model::Clip& clip, const model::Track& pistaClip, model::Id pistaDestino) const;
    // Ajusta la posición propuesta del movimiento si queda cerca del borde de otro clip.
    void AplicarIman();
    // Tramos de todos los clips salvo `excluidos` (los que se están arrastrando).
    std::vector<TramoIman> TramosFijos(const std::vector<model::Id>& excluidos) const;

    void Dibujar(CDC& dc, const CRect& rc);
    void DibujarRegla(CDC& dc, const CRect& rc);
    // Resaltado de un clip: el seleccionado lleva borde doble amarillo y su clip vinculado,
    // borde simple.
    enum class Resaltado { Nada, Vinculado, Seleccionado };
    void DibujarClip(CDC& dc, const CRect& rcClip, const model::Clip& clip, bool video, Resaltado resaltado,
                     bool silenciado);

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
    bool m_arrastreGrupo = false;  // se arrastra una selección múltiple
    model::TimeUs m_trimIn = 0;   // tramo propuesto durante un trim
    model::TimeUs m_trimOut = 0;
    std::optional<model::TimeUs> m_lineaIman;  // instante donde se pegó el arrastre en curso

    // Último clic derecho sobre un clip (para ClipParaTransicion).
    model::Id m_clipMenu = model::kSinId;
    model::Id m_fromMenu = model::kSinId;
};
