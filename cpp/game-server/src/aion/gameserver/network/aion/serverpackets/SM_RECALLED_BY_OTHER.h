#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * Opens the window which asks a player whether he wants to be teleported to the caster of a summon skill, and closes it again when the request is
 * no longer valid. The client answers with CM_RECALLED_BY_OTHER_ANSWER.
 *
 * @author SVDNESS
 */
class SM_RECALLED_BY_OTHER : public AionServerPacket {
private:
	std::optional<std::string> casterName{}; // fieldmap.toml: writeImpl compares the Java String with null (hub-headers.md §6)
	int32_t skillId{};
	int32_t seconds{};
public:
	/** Closes the window on the client. */
	SM_RECALLED_BY_OTHER();
	SM_RECALLED_BY_OTHER(std::optional<std::string_view> casterName, int32_t skillId, int32_t seconds);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
