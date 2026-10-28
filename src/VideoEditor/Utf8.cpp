// Utf8.cpp
// Capa: VIEW. Ver Utf8.h.
#include "Utf8.h"

std::string Utf8DesdeCString(const CString& texto) {
    if (texto.IsEmpty()) return {};
    // Primera llamada: tamaño necesario; segunda: conversión real.
    const int largo = WideCharToMultiByte(CP_UTF8, 0, texto.GetString(), texto.GetLength(), nullptr, 0, nullptr, nullptr);
    std::string resultado(static_cast<size_t>(largo), '\0');
    WideCharToMultiByte(CP_UTF8, 0, texto.GetString(), texto.GetLength(), resultado.data(), largo, nullptr, nullptr);
    return resultado;
}

CString CStringDesdeUtf8(const std::string& utf8) {
    if (utf8.empty()) return CString();
    const int largo = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    CString resultado;
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), resultado.GetBuffer(largo), largo);
    resultado.ReleaseBuffer(largo);
    return resultado;
}
