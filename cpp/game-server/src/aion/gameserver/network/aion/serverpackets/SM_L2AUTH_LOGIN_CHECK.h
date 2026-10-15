#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * S0c declaration header (hub-headers.md §12). C++ difference: the two retail server id tables are constexpr arrays computed like the Java
 * static initializer block (they are never written after class initialization).
 *
 * @author -Nemesiss-
 */
class SM_L2AUTH_LOGIN_CHECK : public AionServerPacket {
private:
	/** Java: static byte[128] filled by the static initializer (retail data, don't question it) */
	static constexpr std::array<int8_t, 128> serverIdByIndex = [] {
		std::array<int8_t, 128> table{};
		for (int8_t i = 1; i <= 60; i++)
			table[static_cast<size_t>(i)] = i;
		table[66] = 61;
		return table;
	}(); // fieldmap.toml: constexpr table of the Java static initializer block
	/** Java: static byte[64] filled by the static initializer */
	static constexpr std::array<int8_t, 64> serverIndexById = [] {
		std::array<int8_t, 64> table{};
		for (int8_t i = 1; i <= 60; i++)
			table[static_cast<size_t>(i)] = i;
		table[61] = 66;
		return table;
	}(); // fieldmap.toml: constexpr table of the Java static initializer block
	/** True if client is authed. */
	bool ok{};
	std::string accountName{};

public:
	SM_L2AUTH_LOGIN_CHECK(bool ok, std::string_view accountName);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
