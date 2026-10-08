#include "aion/gameserver/services/PvpService.h"

#include <map>
#include <optional>
#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/templates/bounty/BountyTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/services/item/ItemService.h"
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
#include "aion/gameserver/utils/collections/Predicates.h"

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

// Java PvpService.java:68-84
void PvpService::sendBountyReward(model::gameobjects::player::Player& player, model::templates::bounty::BountyType type, int32_t killScore) {
	for (const model::templates::bounty::KillBountyTemplate* template_ : killBounties) {
		if (template_->getBountyType() != type || template_->getKillCount() != killScore)
			continue;
		if (template_->getRaceCondition() != model::Race::PC_ALL && template_->getRaceCondition() != player.getRace())
			continue;
		std::vector<const model::templates::bounty::BountyTemplate*> bounties;
		if (template_->isRandomReward())
			bounties.push_back(commons::utils::Rnd::get(template_->getBounties())); // Java: Rnd.get(list), null for an empty list
		else
			for (const model::templates::bounty::BountyTemplate& bounty : template_->getBounties())
				bounties.push_back(&bounty);

		for (const model::templates::bounty::BountyTemplate* bounty : bounties) {
			if (bounty == nullptr) // Java: bounty.getItemId() on the null of an empty random list
				throw runtime::NullPointerException("BountyTemplate");
			item::ItemService::addItem(player, bounty->getItemId(), bounty->getCount(), true,
				*item::ItemService::ItemUpdatePredicate::create(item::ItemPacketService_ItemAddType::ITEM_COLLECT,
					item::ItemPacketService_ItemUpdateType::INC_CASH_ITEM));
		}
	}
}

// Java PvpService.java:86-88
void PvpService::finalizeHeadhuntingSeason() {
	headhunters.clear();
}

void PvpService::doReward(Player& victim) {
	doReward(victim, 1);
}

// Java PvpService.java:94-97 (synchronized)
runtime::Ptr<model::event::Headhunter> PvpService::getHeadhunterById(int32_t objId) {
	SYNCHRONIZED(*this) {
		runtime::Ptr<model::event::Headhunter> headhunter = headhunters.putIfAbsent(objId,
			model::event::Headhunter::create(objId, 0, commons::utils::currentTimeMillis(), model::gameobjects::Persistable::PersistentState::UPDATE_REQUIRED));
		return headhunter != nullptr ? headhunter : headhunters.get(objId);
	}
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
			// (PvpService.java:104); closed by the M5g parties lane (m5g-plan.md D11, W-03)
			const auto allExcept = utils::collections::Predicates::Players::allExcept(victim);
			SM_SYSTEM_MESSAGE friendlyDeath = SM_SYSTEM_MESSAGE::STR_MSG_COMBAT_FRIENDLY_DEATH(victim.getName());
			team->sendPacket([&allExcept](model::gameobjects::AionObject& member) { return allExcept(*runtime::cast<Player>(member)); },
				{friendlyDeath});
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

// Java PvpService.java:276-278
runtime::Ptr<model::event::Headhunter> PvpService::getHeadhunter(int32_t hunterId) {
	return headhunters.get(hunterId);
}

} // namespace aion::gameserver::services
