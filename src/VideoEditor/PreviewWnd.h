// PreviewWnd.h
// Capa: VIEW (MFC). Control propio para el preview del video ("Custom Control" en el .rc
// con la clase de ventana "VEPreviewWnd").
//
// Fase 2: solo dibuja el fondo negro. En la fase 3 pinta los cuadros del timeline.
#pragma once

#ifndef __AFXWIN_H__
#include <afxwin.h>
#endif

class CPreviewWnd : public CWnd {
public:
    static constexpr const wchar_t* kClase = L"VEPreviewWnd";
    // Registra la clase de ventana; debe llamarse antes de crear el diálogo.
    static BOOL RegistrarClase();

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    DECLARE_MESSAGE_MAP()
};
