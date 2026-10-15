// SPDX-License-Identifier: AGPL-3.0-only
#include "ProjectFixture.h"
#include <juce_cryptography/juce_cryptography.h>
#include <json.hpp>
#include <fstream>
#include <iostream>
#if defined(__APPLE__) || defined(__linux__)
#include <sys/resource.h>
#endif
using Json = nlohmann::json;
namespace
{
int checks = 0;
void check(bool ok, const char* message)
{
    if (!ok)
        throw std::runtime_error(message);
    ++checks;
    std::cout << "PASS " << message << std::endl;
}
std::string hash(const juce::String& value)
{
    return juce::SHA256(value.toRawUTF8(), size_t(value.getNumBytesAsUTF8())).toHexString().toStdString();
}
Json bytes(const juce::String& value)
{
    // Malformed numeric entities in the pinned parser can yield invalid UTF-8.
    // Preserve those bytes exactly; JSON replacement characters would hide a change.
    std::string hex;
    for (unsigned char c : value.toStdString())
    {
        hex += "0123456789abcdef"[c >> 4];
        hex += "0123456789abcdef"[c & 15];
    }
    return {{"utf8_hex", hex}};
}
Json image(const juce::XmlElement& element)
{
    Json attrs = Json::object(), children = Json::array();
    for (int i = 0; i < element.getNumAttributes(); ++i)
        attrs[element.getAttributeName(i).toStdString()] = bytes(element.getAttributeValue(i));
    for (auto* child = element.getFirstChildElement(); child; child = child->getNextElement())
        children.push_back(image(*child));
    return {{"tag", element.getTagName().toStdString()},
            {"attributes", attrs},
            {"text", bytes(element.isTextElement() ? element.getText() : juce::String{})},
            {"children", children}};
}
Json semantics()
{
    const std::vector<std::pair<std::string, std::string>> positive{
        {"<EDIT value=\"\"/>", ""},
        {"<EDIT value='plain \"quoted\" text'/>", "plain \"quoted\" text"},
        {"<EDIT value=\"&amp;&quot;&apos;&lt;&gt;\"/>", "&\"'<>"},
        {"<EDIT value=\"&AMP;&QuOt;&APOS;&LT;&GT;\"/>", "&\"'<>"},
        {"<EDIT value=\"before&amp;middle&#34;after\"/>", "before&middle\"after"},
        {"<EDIT value=\"&#65;&#x42;&#X43;&#10;&#x1F3B5;\"/>", "ABC\n🎵"},
        {"<EDIT value=\"中文 é é 🎵\"/>", "中文 é é 🎵"},
        {"<EDIT value=\"raw\tline\r\nnext\"/>", "raw\tline\r\nnext"},
        {"<EDIT value=\"&amp;tail\"/>", "&tail"},
        {"<EDIT value=\"head&amp;\"/>", "head&"},
        {"<EDIT value=\"&amp;&amp;&amp;\"/>", "&&&"},
        {"<?xml version='1.0'?><!-- comment --><EDIT ndaw:v='1' value='&lt;node&gt;'><CHILD/><![CDATA[x<y]]></EDIT>",
         "<node>"}};
    Json result = Json::array();
    auto observe = [&](const std::string& text)
    {
        juce::XmlDocument document(juce::String::fromUTF8(text.data(), int(text.size())));
        auto xml = document.getDocumentElement();
        result.push_back({{"input", text},
                          {"parsed", bool(xml)},
                          {"error", document.getLastParseError().toStdString()},
                          {"tree", xml ? image(*xml) : Json(nullptr)}});
        return xml;
    };
    for (const auto& [text, expected] : positive)
    {
        auto xml = observe(text);
        check(xml && xml->getStringAttribute("value").toStdString() == expected,
              "actual parser preserves independently specified attribute content");
    }
    // Additional probes record pinned-parser compatibility, including internal
    // DTD expansion, partial trees and errors, without introducing a validator.
    for (const char* text :
         {"<EDIT value=\"unterminated", "<EDIT value=\"prefix&amp;unterminated", "<EDIT value=\"a&unknown;b\"/>",
          "<EDIT value=\"a&missing\"/>", "<EDIT value=\"&#xGG;\"/>", "<EDIT value=\"&#;\"/>",
          "<EDIT value=\"&#123456789012345;\"/>", "<EDIT value='a&quot;b' other='tail'/>", "<EDIT value=\"&#0;x\"/>",
          "<EDIT value=\"a&amp;\" second=\"&lt;x&gt;\"/>",
          "<!DOCTYPE EDIT [<!ENTITY word 'solo &amp; mic'>]><EDIT value=\"&word;\"/>"})
        observe(text);
    return result;
}
Json benchmark(int repeats)
{
    const std::string unit = "<POINT t=\"1.25\" v=\"0.5\" l=\"声&\"/>\n";
    std::string expected;
    expected.reserve(unit.size() * size_t(repeats));
    for (int i = 0; i < repeats; ++i)
        expected += unit;
    juce::XmlElement fixture("EDIT");
    fixture.setAttribute("value", juce::String::fromUTF8(expected.data(), int(expected.size())));
    fixture.setAttribute("following", "independent following attribute");
    fixture.createNewChildElement("TRACK")->setAttribute("id", "stable-track");
    const auto text = fixture.toString(juce::XmlElement::TextFormat{}.singleLine());
    const auto begin = juce::Time::getMillisecondCounterHiRes();
    auto xml = juce::XmlDocument::parse(text);
    const auto milliseconds = juce::Time::getMillisecondCounterHiRes() - begin;
    check(xml && xml->getStringAttribute("value").toStdString() == expected &&
              xml->getStringAttribute("following") == "independent following attribute" &&
              xml->getChildByName("TRACK") && xml->getChildByName("TRACK")->getStringAttribute("id") == "stable-track",
          "full fixed repeated-entity attribute, following attribute and child decode exactly");
    return {{"repetitions", repeats},
            {"encoded_bytes", text.getNumBytesAsUTF8()},
            {"decoded_bytes", expected.size()},
            {"decoded_sha256", hash(xml->getStringAttribute("value"))},
            {"parse_ms", milliseconds}};
}
Json realProject(const juce::File& file)
{
    const auto originalHash = juce::SHA256(file).toHexString();
    const auto begin = juce::Time::getMillisecondCounterHiRes();
    auto xml = juce::XmlDocument::parse(file);
    const auto parse = juce::Time::getMillisecondCounterHiRes() - begin;
    check(bool(xml), "same owned production project parses without dropping history");
    const auto decoded = ndaw::v2::project_document::read(*xml);
    const auto all = juce::Time::getMillisecondCounterHiRes() - begin;
    check(decoded.edit.hasType("EDIT") && juce::SHA256(file).toHexString() == originalHash,
          "actual production document decoder accepts unchanged original project bytes");
    return {{"file", file.getFullPathName().toStdString()},
            {"file_sha256", originalHash.toStdString()},
            {"file_bytes", file.getSize()},
            {"parse_ms", parse},
            {"parse_and_document_decode_ms", all},
            {"parsed_semantic_sha256", hash(xml->toString(juce::XmlElement::TextFormat{}.singleLine()))}};
}
Json peakBytes()
{
#if defined(__APPLE__) || defined(__linux__)
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0)
#if defined(__APPLE__)
        return usage.ru_maxrss;
#else
        return uint64_t(usage.ru_maxrss) * 1024;
#endif
#endif
    return nullptr;
}
} // namespace
int main(int argc, char** argv)
{
    Json report;
    const bool baseline = argc > 3 && std::string(argv[3]) == "baseline";
    try
    {
        report["semantics"] = semantics();
        if (!baseline)
        {
            std::ifstream input(argc > 2 ? argv[2] : "tests/fixtures/xml-attribute-semantics.json");
            const auto expected = Json::parse(input);
            check(report["semantics"] == expected,
                  "all pinned valid and malformed parser observations match reviewed compatibility fixture");
        }
        report["small"] = benchmark(8192);
        report["large"] = benchmark(32768);
        if (argc > 4)
            report["owned_project"] = realProject(juce::File(juce::String::fromUTF8(argv[4])));
        report["peak_resident_bytes"] = peakBytes();
        report["budgets"] = {{"maximum_large_parse_ms", 500}, {"maximum_process_peak_bytes", 256 * 1024 * 1024}};
        report["budget_evaluated"] = !baseline;
        if (!baseline)
        {
            check(report["large"]["parse_ms"].get<double>() <= 500, "fixed 32768-record attribute parses within 500ms");
            if (!report["peak_resident_bytes"].is_null())
                check(report["peak_resident_bytes"].get<uint64_t>() <= 256 * 1024 * 1024,
                      "actual test process peak resident memory stays within fixed 256MiB");
        }
        report["result"] = baseline ? "diagnostic_baseline_complete" : "passed";
    }
    catch (const std::exception& error)
    {
        report["result"] = "failed";
        report["error"] = error.what();
        std::cerr << "FAIL " << error.what() << std::endl;
    }
    report["checks"] = checks;
    report["scope"] = "actual pinned JUCE parser and production project decoder; not audio callback or disk cold-open";
    if (argc > 1)
        std::ofstream(argv[1]) << report.dump(2);
    std::cout << report.dump(2) << std::endl;
    return report["result"] == "failed" ? 1 : 0;
}
