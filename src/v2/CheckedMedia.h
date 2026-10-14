// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_cryptography/juce_cryptography.h>
namespace ndaw::v2
{
// Non-realtime streaming hash; cancellation/priority checks bound each read.
class CheckedMediaStream final : public juce::InputStream
{
public:
    CheckedMediaStream(const juce::File& f, std::function<void()> check) : stream(f), checkpoint(std::move(check))
    {
        if (!stream.openedOk())
            throw std::runtime_error("media cannot be opened");
    }
    int64_t getTotalLength() override
    {
        return stream.getTotalLength();
    }
    bool isExhausted() override
    {
        return stream.isExhausted();
    }
    int64_t getPosition() override
    {
        return stream.getPosition();
    }
    bool setPosition(int64_t n) override
    {
        return stream.setPosition(n);
    }
    int read(void* dst, int n) override
    {
        if (checkpoint)
            checkpoint();
        const int count = stream.read(dst, n);
        if (count == 0 && !stream.isExhausted())
            throw std::runtime_error("media read failed");
        return count;
    }

private:
    juce::FileInputStream stream;
    std::function<void()> checkpoint;
};
inline std::string checkedMediaHash(const juce::File& f, const std::function<void()>& checkpoint = {})
{
    CheckedMediaStream stream(f, checkpoint);
    return juce::SHA256(stream).toHexString().toStdString();
}
} // namespace ndaw::v2
