// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Session.h"
#include <array>

namespace ndaw {
// Prepared clip-local integer ramps. Only bounded arithmetic/libm runs in RT.
struct GainEnvelope {
    static constexpr std::size_t capacity=64;
    struct Ramp {Frame begin{},end{};bool rising{},equalPower{};};
    std::array<Ramp,capacity> ramps{};std::size_t count{};
    Frame legacyIn{},legacyOut{},length{};
    GainEnvelope()=default;
    explicit GainEnvelope(const Json& clip); // validated NRT compilation
    double at(Frame clipTime) const noexcept;
};
const Json& validateCompiledEnvelopeBudget(const Json& session);
void validateGainEnvelope(const Json& clip);
Json sliceClip(const Json& clip,Frame begin,Frame end); // preserve actual envelope/source phase
Json remapClipBounds(const Json& clip,Frame start,Frame sourceStart,Frame length);
void setClipEdgeFades(Json& clip,Frame in,Frame out);
void addTransitionRamp(Json& clip,Frame begin,Frame end,bool rising,const std::string& curve,const std::string& id);
}
