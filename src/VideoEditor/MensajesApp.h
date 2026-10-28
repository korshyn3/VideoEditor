// MensajesApp.h
// Capa: VIEW (MFC).
//
// Mensajes propios de la aplicación (a partir de WM_APP). Los controles hijos los envían
// a la ventana principal para que actualice el resto de la interfaz.
#pragma once

#include <windows.h>

// El timeline cambió la selección, el cursor o el modelo (wParam = 1 si cambió el modelo).
constexpr UINT WM_APP_VISTA_CAMBIO = WM_APP + 1;
