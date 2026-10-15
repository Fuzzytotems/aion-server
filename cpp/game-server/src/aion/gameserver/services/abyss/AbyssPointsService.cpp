#include "aion/gameserver/services/abyss/AbyssPointsService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANK.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANK_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_EDIT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/abyss/AbyssSkillService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"

namespace aion::gameserver::services::abyss {

using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.abyss.AbyssPointsService");

// Java AbyssPointsService.java:25-31 (the kill and PvP variant; M5j stage 1 CP4: PvpService.rewardPlayerTeam and NpcController.doReward)
void AbyssPointsService::addAp(model::gameobjects::player::Player& player, model::gameobjects::VisibleObject& obj, int32_t value) {
	if (value > 30000) {
		log.warn("WARN BIG COUNT AP: " + std::to_string(value) + " for " + player.toString() + " from " + obj.toString());
	}
	addAp(player, value);
	SiegeService::getInstance().onAbyssPointsAdded(player, obj, value);
}

// Java AbyssPointsService.java:33-35
void AbyssPointsService::addAp(model::gameobjects::player::Player& player, int32_t amount) {
	addAp(runtime::Ptr<Player>(player), amount, [](int32_t added) { return SM_SYSTEM_MESSAGE::STR_MSG_COMBAT_MY_ABYSS_POINT_GAIN(added); });
}

// Java AbyssPointsService.java:37-53
void AbyssPointsService::addAp(runtime::Ptr<model::gameobjects::player::Player> player, int32_t amount, const std::function<network::aion::serverpackets::SM_SYSTEM_MESSAGE(int32_t)>& gainMessage) {
	if (!player)
		return;

	// java-race: AbyssRank.addAp is an unsynchronized read-modify-write (AbyssRank.cpp), so `added` can include another thread's grant
	int32_t oldAp = player->getAbyssRank()->getAp();
	utils::stats::AbyssRankEnum oldAbyssRank = player->getAbyssRank()->getRank();
	player->getAbyssRank()->addAp(amount);
	int32_t added = player->getAbyssRank()->getAp() - oldAp;

	PacketSendUtility::sendPacket(*player, amount >= 0 ? gainMessage(added) : SM_SYSTEM_MESSAGE::STR_MSG_USE_ABYSSPOINT(-added));
	onRankChanged(*player, added != 0, oldAbyssRank != player->getAbyssRank()->getRank(), std::nullopt);
	if (player->isLegionMember() && added > 0) {
		// Legion.addContributionPoints stays unported (P5-11, M5h): a legion member's AP gain throws here, after the rank packets
		runtime::Ptr<model::team::legion::Legion> legion = player->getLegion();
		legion->addContributionPoints(added);
		PacketSendUtility::broadcastToLegion(*legion, network::aion::serverpackets::SM_LEGION_EDIT(0x03, *legion));
	}
}

// Java AbyssPointsService.java:55-63
void AbyssPointsService::onRankChanged(model::gameobjects::player::Player& player, bool abyssPointChanged, bool abyssRankChanged, std::optional<int32_t> newRankingListPosition) {
	if (abyssPointChanged || abyssRankChanged || newRankingListPosition.has_value())
		PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_ABYSS_RANK(player, newRankingListPosition));
	if (abyssRankChanged) {
		PacketSendUtility::broadcastPacket(player, network::aion::serverpackets::SM_ABYSS_RANK_UPDATE(0, player));
		player.getEquipment().checkRankLimitItems();
		AbyssSkillService::updateSkills(player);
	}
}

} // namespace aion::gameserver::services::abyss
