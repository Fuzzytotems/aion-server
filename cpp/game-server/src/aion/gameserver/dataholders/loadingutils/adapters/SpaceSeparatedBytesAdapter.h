#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/dataholders/loadingutils/XmlBindingFwd.h"

namespace aion::gameserver::xml::adapters {

/**
 * Java: SpaceSeparatedBytesAdapter.unmarshal ("1 2 3" -> byte[]), @author Neon. Exact String.split(" ") + Byte.parseByte semantics:
 * the value is split at every single space, trailing empty parts are dropped, so "1 2 " is {1, 2} and "   " is {}; an empty value, a
 * leading space or a double space yields an empty part and fails like Byte.parseByte(""). Parts accept an optional sign and ASCII digits in
 * [-128, 127], no other whitespace.
 * @throws XmlValueException
 */
std::vector<int8_t> parseSpaceSeparatedBytes(std::string_view value);
/** Same, with the location of the attribute or element being bound on errors. @throws StaticDataException */
std::vector<int8_t> parseSpaceSeparatedBytes(BindContext& context, std::string_view value);
/** Java: SpaceSeparatedBytesAdapter.marshal */
std::string printSpaceSeparatedBytes(std::span<const int8_t> bytes);

} // namespace aion::gameserver::xml::adapters
