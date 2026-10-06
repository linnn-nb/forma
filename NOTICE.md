# Forma Studio licensing

Forma Studio original source: AGPL-3.0-only, copyright 2026 NativeDAW contributors; see LICENSE. Tracktion Engine 3.5.0 uses its GPL-3.0-or-later option at commit 0d4d77c8c9defa6ec2aec6454f634e77bbd13f98. The build applies six recorded patches: user-parameter boundary, recording status, bus-only render output, initial MIDI scan, FourOsc flush and wet reverb tail. JUCE 8.0.13 is pinned at commit 37c894f83d379179b2070d437ccd0f1cd9af9576 under its AGPLv3 option; the build applies the recorded AU parameter-cache patch. nlohmann/json 3.11.3 and libebur128 1.2.6 are MIT-licensed. Third-party code retains its original notices and terms.

This public repository distributes source code, not an installer or production release. It is an early development snapshot; a completed release licensing audit is not claimed. No user audio, model weights, third-party plugin binaries or private presets are included.

The selected JUCE source contains the VST3 SDK and AU support; M0 does not claim hosted-plugin qualification. Tracktion/JUCE also include third-party codecs, graph libraries, stretch algorithms, graphics and fonts, whose notices are retained in the dependency source trees and local package. A full shipped-component SBOM remains a release task.

NativeDAW is the internal project and current application name; Forma Studio is the public product name. Pro Tools is only a workflow reference; no Avid affiliation, endorsement, proprietary code, branding or hardware equivalence is asserted.
