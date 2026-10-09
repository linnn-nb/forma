#include <nativedaw/v2/EngineCommands.h>
#include <charconv>
#include <cmath>
#include <regex>

namespace ndaw::v2
{
namespace
{
constexpr double rate = 48000.;
tracktion::TimePosition time(int64_t sample)
{
    return tracktion::TimePosition::fromSeconds(double(sample) / rate);
}
int64_t maximum()
{
    return std::llround(te::Edit::maximumLength * rate);
}
void validate(int64_t anchor, const std::string& unit, int fps)
{
    if (anchor < 0 || anchor > maximum() ||
        (unit != "samples" && unit != "min_sec" && unit != "timecode" && unit != "bars_beats") ||
        (fps != 24 && fps != 25 && fps != 30))
        throw std::runtime_error("invalid roll duration context");
}
int64_t checked(double samples)
{
    if (!std::isfinite(samples) || samples < 0 || samples > double(maximum()))
        throw std::runtime_error("时长超出工程范围");
    return std::llround(samples);
}
} // namespace
std::string Commands::formatRollDuration(int64_t duration, int64_t anchor, bool pre, const std::string& unit,
                                         int fps) const
{
    checkThread();
    validate(anchor, unit, fps);
    checked(double(duration));
    if (unit == "samples")
        return std::to_string(duration);
    if (unit == "bars_beats")
    {
        const auto endpoint = time(anchor + (pre ? -duration : duration));
        const auto beats = std::abs(edit->tempoSequence.toBeats(endpoint).inBeats() -
                                    edit->tempoSequence.toBeats(time(anchor)).inBeats());
        return juce::String(beats, 9).toStdString();
    }
    if (unit == "timecode")
    {
        const auto frame = duration / (48000 / fps);
        return juce::String::formatted("%02lld:%02lld:%02lld:%02lld", frame / (fps * 3600), (frame / (fps * 60)) % 60,
                                       (frame / fps) % 60, frame % fps)
            .toStdString();
    }
    return (juce::String(duration / 2880000) + ":" +
            juce::String(double(duration % 2880000) / rate, 6).paddedLeft('0', 9))
        .toStdString();
}
int64_t Commands::parseRollDuration(const std::string& raw, int64_t anchor, bool pre, const std::string& unit,
                                    int fps) const
{
    checkThread();
    validate(anchor, unit, fps);
    const auto input = juce::String(raw).trim().toStdString();
    if (input.empty() || input.size() > 64)
        throw std::runtime_error("请输入完整非负时长");
    if (unit == "samples")
    {
        int64_t n = 0;
        const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), n);
        if (error != std::errc{} || end != input.data() + input.size() || n < 0 || n > maximum())
            throw std::runtime_error("请输入完整非负样本整数");
        return n;
    }
    std::smatch m;
    if (unit == "timecode")
    {
        if (!std::regex_match(input, m, std::regex(R"((\d{1,4}):(\d{2}):(\d{2}):(\d{2}))")))
            throw std::runtime_error("时间码时长格式：HH:MM:SS:FF（NDF）");
        const auto h = std::stoi(m[1]), minute = std::stoi(m[2]), second = std::stoi(m[3]), frame = std::stoi(m[4]);
        if (minute >= 60 || second >= 60 || frame >= fps)
            throw std::runtime_error("时间码字段超出帧率或分钟范围");
        return checked(double(((int64_t(h) * 60 + minute) * 60 + second) * fps + frame) * (48000 / fps));
    }
    if (!std::regex_match(input, m,
                          std::regex(unit == "min_sec" ? R"((?:(\d{1,8}):)?(\d{1,12}(?:\.\d{1,9})?))"
                                                       : R"((\d{1,12}(?:\.\d{1,9})?))")))
        throw std::runtime_error(unit == "min_sec" ? "时长格式：分:秒 或秒数" : "请输入非负拍数（支持小数）");
    if (unit == "min_sec")
    {
        const double seconds = std::stod(m[2]);
        if (m[1].matched && seconds >= 60)
            throw std::runtime_error("分:秒格式中的秒必须小于60");
        return checked((seconds + (m[1].matched ? std::stod(m[1]) * 60 : 0)) * rate);
    }
    const auto beats = std::stod(m[1]);
    // Native TempoSequence maps in both directions, including initial-tempo extrapolation before zero.
    const auto at = edit->tempoSequence.toBeats(time(anchor)).inBeats();
    const auto endpoint = edit->tempoSequence.toTime(tracktion::BeatPosition::fromBeats(at + (pre ? -beats : beats)));
    return checked(std::abs(endpoint.inSeconds() * rate - double(anchor)));
}
} // namespace ndaw::v2
