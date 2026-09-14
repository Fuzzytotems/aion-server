#include "aion/gameserver/network/aion/serverpackets/SM_UPGRADE_ARCADE.h"

#include "aion/gameserver/model/event/ArcadeProgress.h"
#include "aion/gameserver/model/templates/event/upgradearcade/ArcadeRewards.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_UPGRADE_ARCADE::SM_UPGRADE_ARCADE()
	: AionServerPacket(opcodeOf<SM_UPGRADE_ARCADE>), action(2) {
}

SM_UPGRADE_ARCADE::SM_UPGRADE_ARCADE(bool showIconValue)
	: AionServerPacket(opcodeOf<SM_UPGRADE_ARCADE>), action(0), showIcon(showIconValue) {
}

SM_UPGRADE_ARCADE::SM_UPGRADE_ARCADE(model::event::ArcadeProgress& progressValue, int32_t sessionIdValue)
	: AionServerPacket(opcodeOf<SM_UPGRADE_ARCADE>), action(1), progress(progressValue), sessionId(sessionIdValue) {
}

SM_UPGRADE_ARCADE::SM_UPGRADE_ARCADE(bool successValue, model::event::ArcadeProgress& progressValue)
	: AionServerPacket(opcodeOf<SM_UPGRADE_ARCADE>), action(3), progress(progressValue), success(successValue) {
}

SM_UPGRADE_ARCADE::SM_UPGRADE_ARCADE(model::event::ArcadeProgress& progressValue)
	: AionServerPacket(opcodeOf<SM_UPGRADE_ARCADE>), action(4), progress(progressValue) {
}

SM_UPGRADE_ARCADE::SM_UPGRADE_ARCADE(model::event::ArcadeProgress& progressValue, bool resumeAllowed)
	: AionServerPacket(opcodeOf<SM_UPGRADE_ARCADE>), action(5), progress(progressValue) {
}

SM_UPGRADE_ARCADE::SM_UPGRADE_ARCADE(int32_t itemId, int64_t count)
	: AionServerPacket(opcodeOf<SM_UPGRADE_ARCADE>), action(6), rewardItemId(itemId), rewardItemCount(count) {
}

SM_UPGRADE_ARCADE::SM_UPGRADE_ARCADE(int32_t frenzyDurationSecondsValue)
	: AionServerPacket(opcodeOf<SM_UPGRADE_ARCADE>), action(7), frenzyDurationSeconds(frenzyDurationSecondsValue) {
}

SM_UPGRADE_ARCADE::SM_UPGRADE_ARCADE(int32_t actionValue, bool disableWindowValue)
	: AionServerPacket(opcodeOf<SM_UPGRADE_ARCADE>), action(actionValue), disableWindow(disableWindowValue) {
}

SM_UPGRADE_ARCADE::SM_UPGRADE_ARCADE(const std::vector<const model::templates::event::upgradearcade::ArcadeRewards*>& rewards)
	: AionServerPacket(opcodeOf<SM_UPGRADE_ARCADE>), action(10), arcadeRewards(rewards) {
}

SM_UPGRADE_ARCADE::~SM_UPGRADE_ARCADE() = default;

void SM_UPGRADE_ARCADE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
