// VideoEditorApp.cpp
// Capa: VIEW (MFC). Crea el controller y muestra la ventana principal (un diálogo).
#include "VideoEditorApp.h"
#include "EditorDlg.h"

#include <afxcmn.h>

// Estilo visual moderno de los controles de Windows (Common Controls v6).
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

CVideoEditorApp theApp;

BOOL CVideoEditorApp::InitInstance() {
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icc);

    CWinApp::InitInstance();

    // Las clases de ventana de los "Custom Control" del .rc deben existir antes de crear
    // el diálogo; si no, la creación del diálogo falla.
    CPreviewWnd::RegistrarClase();
    CTimelineWnd::RegistrarClase();

    // El controller vive mientras dure la app; la View solo guarda una referencia.
    controller::EditorController controller;

    CEditorDlg dlg(controller);
    m_pMainWnd = &dlg;
    dlg.DoModal();

    // La app termina al cerrar el diálogo (no hay otra ventana principal).
    return FALSE;
}
