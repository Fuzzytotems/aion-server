#include "aion/gameserver/services/PvpService.h"

#include <map>
#include <optional>
#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/DamageInfo.h"
#include "aion/gameserver/controllers/attack/DamageList.h"
#include "aion/gameserver/dao/HeadhuntingDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/KillBountyData.h"
#include "aion/gameserver/model/event/Headhunter.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/templates/bounty/KillBountyTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/abyss/AbyssService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services {

using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

static const auto log = commons::logging::LoggerFactory::getLogger("KILL_LOG");

PvpService::PvpService() {
	// Java: killBounties = DataManager.KILL_BOUNTY_DATA.getKillBounties(). The holder owns the templates for the life of the process, so the
	// C++ list holds pointers into it instead of copying them (hub-headers.md §8.3, static template pointers).
	for (const model::templates::bounty::KillBountyTemplate& template_ : dataholders::DataManager::KILL_BOUNTY_DATA->getKillBounties())
		killBounties.add(&template_);
	// Java: headhunters = HeadhuntingDAO.loadHeadhunters()
	for (auto& [hunterId, hunter] : dao::HeadhuntingDAO::loadHeadhunters())
		headhunters.put(hunterId, std::move(hunter));
}

PvpService& PvpService::getInstance() {
	static PvpService instance; // Java SingletonHolder
	return instance;
}

void PvpService::sendBountyReward(model::gameobjects::player::Player& player, model::templates::bounty::BountyType type, int32_t killScore) {
	AION_UNPORTED();
}

void PvpService::finalizeHeadhuntingSeason() {
	AION_UNPORTED();
}

void PvpService::doReward(Player& victim) {
	doReward(victim, 1);
}

runtime::Ptr<model::event::Headhunter> PvpService::getHeadhunterById(int32_t objId) {
	AION_UNPORTED();
}

void PvpService::doReward(Player& victim, float apWinMulti) {
	controllers::attack::DamageList damageList = victim.getAggroList().getFinalDamageList();
	std::optional<controllers::attack::DamageInfo> mostDamage = damageList.getMostDamage();
	// Java: `if (mostDamage == null || !(mostDamage.getAttacker() instanceof Player winner))` (PvpService.java:100). `winner` is the attacker
	// downcast; runtime::as answers null for an attacker that is not a Player, so one null test covers both arms of Java's `||`.
	runtime::Ptr<Player> winner = mostDamage ? runtime::as<Player>(mostDamage->getAttacker()) : runtime::Ptr<Player>();
	if (!winner) {
		utils::PacketSendUtility::sendPacket(victim, SM_SYSTEM_MESSAGE::STR_MSG_COMBAT_MY_DEATH());
		runtime::Ptr<model::team::TemporaryPlayerTeam> team = victim.getCurrentTeam();
		if (team) {
			// Java: team.sendPacket(Predicates.Players.allExcept(victim), SM_SYSTEM_MESSAGE.STR_MSG_COMBAT_FRIENDLY_DEATH(victim.getName()))
			// (PvpService.java:104). TemporaryPlayerTeam::sendPacket is AION_UNPORTED (model/team/TemporaryPlayerTeam.cpp:32-35, chunk P5-10),
			// so the unported call sits inside the arm a solo character never enters, exactly as PlayerLifeStats::sendGroupPacketUpdate does
			// (m5b-plan.md B-07). M5b-2 / P5-10 closes it, together with the team packet updaters.
			AION_UNPORTED();
		}
		abyss::AbyssService::announceHighRankedDeath(victim);
		return;
	}

	// The PvP half of doReward (PvpService.java:111-171): findMembersToCountKillFor, the kill counters and the kill bounties, the AP/XP/DP
	// distribution over the team damages, the AP the victim loses and the kill announcements. It is reached only when the most-damage attacker
	// IS a player, and no player can kill a player at M5b-1: m5b-plan.md C-04 keeps this path and AbyssPointsService::addAp at need **W** and
	// names the milestone that closes it - the first PvP or Abyss milestone after M5b-2.
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::gameobjects::player::Player>> PvpService::findMembersToCountKillFor(model::gameobjects::player::Player& winner,
	model::gameobjects::player::Player& victim) {
	AION_UNPORTED();
}

void PvpService::logKill(model::gameobjects::player::Player& winner, model::gameobjects::player::Player& victim,
	const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& assistedGroup) {
	AION_UNPORTED();
}

bool PvpService::rewardPlayerTeam(const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& teamMember,
	model::gameobjects::player::Player& victim, int32_t damage, int32_t totalDamage, float apWinMulti) {
	AION_UNPORTED();
}

void PvpService::updateKillQuests(const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& killers,
	model::gameobjects::player::Player& victim) {
	AION_UNPORTED();
}

runtime::Ptr<model::event::Headhunter> PvpService::getHeadhunter(int32_t hunterId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
