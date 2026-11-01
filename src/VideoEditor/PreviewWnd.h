// PreviewWnd.h
// Capa: VIEW (MFC). Control propio para el preview del video ("Custom Control" en el .rc
// con la clase de ventana "VEPreviewWnd").
//
// Pinta el último cuadro recibido (BGRA) con StretchDIBits, ajustado al tamaño de la
// ventana conservando la relación de aspecto (barras negras). No decodifica nada: los
// cuadros los produce el engine en su hilo y llegan a través del controller.
#pragma once

#ifndef __AFXWIN_H__
#include <afxwin.h>
#endif

#include "EditorEngine/PlaybackEngine.h"

class CPreviewWnd : public CWnd {
public:
    static constexpr const wchar_t* kClase = L"VEPreviewWnd";
    // Registra la clase de ventana; debe llamarse antes de crear el diálogo.
    static BOOL RegistrarClase();

    void MostrarCuadro(engine::FrameBGRA&& cuadro);
    // Texto sobre fondo negro (p. ej. un error del preview). Vacío = sin mensaje.
    void MostrarMensaje(const CString& mensaje);

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    DECLARE_MESSAGE_MAP()

private:
    engine::FrameBGRA m_cuadro;
    CString m_mensaje;
};
