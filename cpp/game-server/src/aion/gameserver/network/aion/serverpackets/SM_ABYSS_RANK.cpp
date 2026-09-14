#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANK.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ABYSS_RANK::SM_ABYSS_RANK(model::gameobjects::player::Player& player) : SM_ABYSS_RANK(player, std::nullopt) {
}

SM_ABYSS_RANK::SM_ABYSS_RANK(model::gameobjects::player::Player& player, std::optional<int32_t> rankingListPositionValue)
	: AionServerPacket(opcodeOf<SM_ABYSS_RANK>) {
	AION_UNPORTED();
}

SM_ABYSS_RANK::~SM_ABYSS_RANK() = default;

void SM_ABYSS_RANK::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
