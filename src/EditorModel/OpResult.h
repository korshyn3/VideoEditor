// OpResult.h
// Capa: MODEL.
//
// Resultado de una operación de edición: o salió bien, o trae un mensaje para el usuario.
// (std::expected sería la alternativa moderna, pero es de C++23.)
#pragma once

#include <string>
#include <utility>

namespace model {

struct OpResult {
    bool ok = true;
    std::string error;  // mensaje legible si ok == false

    static OpResult exito() { return {}; }
    static OpResult fallo(std::string mensaje) { return {false, std::move(mensaje)}; }

    explicit operator bool() const { return ok; }
};

} // namespace model
