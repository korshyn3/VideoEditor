// Ids.h
// Capa: MODEL.
//
// Identificadores de las entidades del proyecto (medios, pistas, clips). Son números
// únicos dentro de un proyecto; 0 significa "ninguno".
#pragma once

#include <cstdint>

namespace model {

using Id = std::uint64_t;
constexpr Id kSinId = 0;

} // namespace model
