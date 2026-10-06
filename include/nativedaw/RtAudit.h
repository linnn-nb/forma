// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Session.h"
namespace ndaw {
#if NATIVEDAW_RT_AUDIT
extern "C" void ndaw_rt_audit_enter() noexcept;
extern "C" void ndaw_rt_audit_leave() noexcept;
extern "C" void ndaw_rt_audit_read(std::uint64_t* values) noexcept;
inline Json rtAuditMetrics() {std::uint64_t values[6]{};ndaw_rt_audit_read(values);return {{"allocation",values[0]},{"free",values[1]},{"blocking_lock_wait",values[2]},{"file_network",values[3]},{"device_property_control",values[4]},{"scopes",values[5]}};}
struct RtAuditScope {RtAuditScope(){ndaw_rt_audit_enter();}~RtAuditScope(){ndaw_rt_audit_leave();}};
#else
struct RtAuditScope {};
#endif
}
