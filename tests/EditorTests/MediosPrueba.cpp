// MediosPrueba.cpp
// Capa: TESTS. Ver MediosPrueba.h.
#include "MediosPrueba.h"

#include <windows.h>

#include <filesystem>

std::string medioPrueba(const std::string& nombre) {
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    // `nombre` está en UTF-8: se convierte con u8string. Construir el path desde un
    // std::string usaría la página de códigos de Windows (p. ej. CP1252/CP1251) y
    // corrompería las tildes y la ñ.
    const auto ruta = std::filesystem::path(exe).parent_path() / L"medios_prueba" /
                      std::filesystem::path(std::u8string(nombre.begin(), nombre.end()));
    const std::u8string u8 = ruta.u8string();
    return std::string(u8.begin(), u8.end());
}
