#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_PACKET_PacketElementType.h"

namespace aion::gameserver::network::aion::serverpackets {

/** Companion of the generated nested enum SM_CUSTOM_PACKET.PacketElementType (docs/design/static-data.md §2.5): Java's constructor data (ADL). */

namespace detail {
/** PacketElementType codes in ordinal order: D('d'), B('b'), H('h'), C('c'), F('f'), DF('e'), Q('q'), S('s') */
inline constexpr std::array<char16_t, 8> PACKET_ELEMENT_TYPE_CODES{{u'd', u'b', u'h', u'c', u'f', u'e', u'q', u's'}};
static_assert(static_cast<size_t>(SM_CUSTOM_PACKET_PacketElementType::S) + 1 == PACKET_ELEMENT_TYPE_CODES.size(), "one code per constant");
} // namespace detail

/** Java: the private field PacketElementType.code */
constexpr char16_t getCode(SM_CUSTOM_PACKET_PacketElementType type) noexcept {
	return detail::PACKET_ELEMENT_TYPE_CODES[static_cast<size_t>(type)];
}

/** Java: PacketElementType.getByCode(code) - the first constant with the code, null (std::nullopt) if there is none */
constexpr std::optional<SM_CUSTOM_PACKET_PacketElementType> getByCode(char16_t code) noexcept {
	for (size_t i = 0; i < detail::PACKET_ELEMENT_TYPE_CODES.size(); i++) {
		if (detail::PACKET_ELEMENT_TYPE_CODES[i] == code)
			return static_cast<SM_CUSTOM_PACKET_PacketElementType>(i);
	}
	return std::nullopt;
}

} // namespace aion::gameserver::network::aion::serverpackets
