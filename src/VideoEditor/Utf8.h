// Utf8.h
// Capa: VIEW (MFC).
//
// Conversión entre CString (UTF-16, lo que usa MFC en modo Unicode) y std::string UTF-8
// (lo que usan el Model, el Controller y FFmpeg). Es el ÚNICO punto donde se cruza esa
// frontera; así las rutas con tildes o ñ funcionan en todas las capas.
#pragma once

#ifndef __AFXWIN_H__
#include <afxwin.h>
#endif

#include <string>

std::string Utf8DesdeCString(const CString& texto);
CString CStringDesdeUtf8(const std::string& utf8);
