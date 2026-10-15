// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <tracktion_engine/tracktion_engine.h>
namespace ndaw::v2
{
class NativeFaderManager
{
public:
    NativeFaderManager();
    ~NativeFaderManager();
    std::unique_ptr<tracktion::engine::VolumeAndPanAutomationProcessor> create(tracktion::engine::VolumeAndPanPlugin&);
    struct Impl;

private:
    std::shared_ptr<Impl> impl;
};
} // namespace ndaw::v2
