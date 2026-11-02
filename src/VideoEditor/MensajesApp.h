// MensajesApp.h
// Capa: VIEW (MFC).
//
// Mensajes propios de la aplicación (a partir de WM_APP). Los controles hijos los envían
// a la ventana principal para que actualice el resto de la interfaz.
#pragma once

#include <windows.h>

// El timeline cambió la selección, el cursor o el modelo (wParam = 1 si cambió el modelo).
constexpr UINT WM_APP_VISTA_CAMBIO = WM_APP + 1;

// El hilo de render del preview tiene novedades (cuadro en pausa, fin, error). Lo envía
// con PostMessage, porque los hilos de trabajo nunca tocan las ventanas directamente.
constexpr UINT WM_APP_PREVIEW_AVISO = WM_APP + 2;
// La exportación terminó (bien, mal o cancelada). La envía el hilo de exportación con
// PostMessage al diálogo Exportar.
constexpr UINT WM_APP_EXPORT_DONE = WM_APP + 3;
