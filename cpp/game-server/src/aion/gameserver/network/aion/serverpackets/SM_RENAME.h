#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * Tells the game client about character or legion name changes. It will then rename in all places like friend list, legion, group, housing npcs, etc.
 *
 * @author Rhys2002
 */
class SM_RENAME : public AionServerPacket {
private:
	bool isLegion{};
	int32_t playerOrLegionId{};
	std::string oldName{};
	std::string newName{};
public:
	SM_RENAME(model::gameobjects::player::Player& player, std::string_view oldName);
	SM_RENAME(model::team::legion::Legion& legion, std::string_view oldName);
private:
	SM_RENAME(bool isLegion, int32_t playerOrLegionId, std::string_view oldName, std::string_view newName);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
