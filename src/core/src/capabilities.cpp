// SPDX-License-Identifier: Apache-2.0
#include "hyremote/core/capabilities.hpp"

#include <algorithm>

namespace hyremote {
namespace {

std::string formatList(const std::vector<PixelFormat> &formats)
{
    std::string text;
    for (std::size_t i = 0; i < formats.size(); ++i) {
        if (i != 0)
            text += ", ";
        text += pixelFormatName(formats[i]);
    }
    return text;
}

std::string domainList(const std::vector<std::string> &domains)
{
    std::string text;
    for (std::size_t i = 0; i < domains.size(); ++i) {
        if (i != 0)
            text += ", ";
        text += domains[i];
    }
    return text;
}

bool shareCpuFormat(const CaptureCapabilities &source, const FrameConsumerCapabilities &consumer)
{
    // An empty list means "unspecified", which interoperates with anything.
    if (source.cpuFormats.empty() || consumer.cpuFormats.empty())
        return true;

    return std::any_of(source.cpuFormats.begin(), source.cpuFormats.end(),
                       [&consumer](PixelFormat format) {
                           return std::find(consumer.cpuFormats.begin(), consumer.cpuFormats.end(),
                                            format)
                                  != consumer.cpuFormats.end();
                       });
}

bool shareExternalDomain(const CaptureCapabilities &source, const FrameConsumerCapabilities &consumer)
{
    if (consumer.externalDomains.empty())
        return true;
    if (source.externalDomains.empty())
        return false;

    return std::any_of(source.externalDomains.begin(), source.externalDomains.end(),
                       [&consumer](const std::string &domain) {
                           return std::find(consumer.externalDomains.begin(),
                                            consumer.externalDomains.end(),
                                            domain)
                                  != consumer.externalDomains.end();
                       });
}

}  // namespace

CompatibilityResult checkFrameCompatibility(const CaptureCapabilities &source,
                                            const FrameConsumerCapabilities &consumer)
{
    if (consumer.acceptsCpu && source.cpuReadable) {
        if (!shareCpuFormat(source, consumer)) {
            return {false, "no common CPU pixel format: capture source offers [" + formatList(source.cpuFormats)
                               + "], consumer accepts [" + formatList(consumer.cpuFormats) + "]"};
        }
        return {true, {}};
    }

    if (consumer.acceptsCpu && !source.cpuReadable) {
        // The consumer could take CPU frames, but this source never produces them: the only
        // possible bridge is a shared external storage domain.
        if (source.externalDomains.empty()) {
            return {false, "capture source is not CPU-readable and declares no external storage domain "
                           "the consumer could accept"};
        }
        if (!shareExternalDomain(source, consumer)) {
            return {false, "no shared external storage domain: capture source offers ["
                               + domainList(source.externalDomains) + "], consumer accepts ["
                               + domainList(consumer.externalDomains) + "]"};
        }
        return {true, {}};
    }

    // Consumer accepts no CPU frames at all: external storage is mandatory.
    if (source.externalDomains.empty()) {
        return {false, "consumer accepts external storage only, but the capture source declares no "
                       "external storage domain"};
    }
    if (!shareExternalDomain(source, consumer)) {
        return {false, "no shared external storage domain: capture source offers ["
                           + domainList(source.externalDomains) + "], consumer accepts ["
                           + domainList(consumer.externalDomains) + "]"};
    }

    return {true, {}};
}

}  // namespace hyremote
