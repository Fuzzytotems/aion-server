#include "aion/gameserver/services/PvpService.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>
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
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/controllers/attack/KillCounter.h"
#include "aion/gameserver/controllers/attack/TeamDamageList.h"
#include "aion/gameserver/custom/pvpmap/PvpMapService.h"
#include "aion/gameserver/model/geometry/Area.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/Rates.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/services/abyss/AbyssPointsService.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/ConquerorAndProtectorService.h"
#include "aion/gameserver/services/event/EventService.h"
#include "aion/gameserver/utils/JavaMath.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/utils/stats/AbyssRankEnumInfo.h"
#include "aion/gameserver/utils/stats/StatFunctions.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"
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

	// The PvP half (PvpService.java:111-176; M5j stage 1 CP4, m5j-plan.md S-12)
	std::vector<runtime::Ptr<Player>> killers = findMembersToCountKillFor(*winner, victim);
	if (!killers.empty()) {
		for (const runtime::Ptr<Player>& killer : killers) {
			killer->getAbyssRank()->incrementAllKills();
			if (configs::main::CustomConfig::ENABLE_KILL_REWARD.load()) {
				int32_t kills = killer->getAbyssRank()->getAllKill();
				for (const model::templates::bounty::KillBountyTemplate* template_ : killBounties) {
					if (template_->getBountyType() == model::templates::bounty::BountyType::PER_X_KILLS) {
						int32_t killStep = template_->getKillCount();
						if (killStep == 0) // Java: kills % 0 is an ArithmeticException
							throw runtime::ArithmeticException("/ by zero");
						if (kills % killStep == 0)
							sendBountyReward(*killer, model::templates::bounty::BountyType::PER_X_KILLS, killStep);
					}
				}
			}
			std::shared_ptr<const std::unordered_set<int32_t>> headhuntingMaps = configs::main::EventsConfig::HEADHUNTING_MAPS.get();
			if (configs::main::EventsConfig::ENABLE_HEADHUNTING.load() && headhuntingMaps && headhuntingMaps->contains(victim.getWorldId())) {
				int32_t kills = getHeadhunterById(killer->getObjectId())->incrementAndGetKills();
				sendBountyReward(*killer, model::templates::bounty::BountyType::SEASONAL_KILLS, kills);
			}
		}
		updateKillQuests(killers, victim);
		if (std::ranges::find(killers, winner) != killers.end()) { // rewards for winner only (group members are ignored)
			conquerorAndProtectorSystem::ConquerorAndProtectorService::getInstance().onKill(*winner, victim);
			event::EventService::getInstance().onPvpKill(*winner, victim);
		}
	}

	logKill(*winner, victim, killers);

	// track how much of the total damage actually generated AP (ignoring Duels, Arena, NPCs), so the victim loses his AP based on that fraction
	int32_t apRelevantDamage = 0;
	int32_t totalDamage = damageList.getTotalDamage();

	// Distribute AP to groups and players that had damage.
	for (const controllers::attack::DamageInfo& damageInfo : damageList.toTeamDamages().getCreatureOrTeamDamages()) {
		std::vector<runtime::Ptr<Player>> teamMembers;
		runtime::Ptr<model::gameobjects::AionObject> attacker = damageInfo.getAttacker();
		runtime::Ptr<Player> player = runtime::as<Player>(attacker);
		runtime::Ptr<model::team::TemporaryPlayerTeam> team = runtime::as<model::team::TemporaryPlayerTeam>(attacker);
		if (player != nullptr && player->getRace() != victim.getRace())
			teamMembers.push_back(player);
		else if (team != nullptr && team->getLeaderObject()->getRace() != victim.getRace())
			for (const runtime::Ptr<model::gameobjects::AionObject>& member : team->getMembers())
				teamMembers.push_back(runtime::cast<Player>(member));

		// Add damage last, so we don't include damage from same race. (Duels, Arena)
		if (rewardPlayerTeam(teamMembers, victim, damageInfo.getDamage(), totalDamage, apWinMulti))
			apRelevantDamage += damageInfo.getDamage();
	}

	// Apply lost AP to defeated player
	const int32_t apLost = utils::stats::StatFunctions::calculatePvPApLost(victim, *winner);
	if (totalDamage == 0) // Java: apLost * apRelevantDamage / totalDamage, an ArithmeticException for a damage list without damage
		throw runtime::ArithmeticException("/ by zero");
	const int32_t apActuallyLost = static_cast<int32_t>(static_cast<uint32_t>(apLost) * static_cast<uint32_t>(apRelevantDamage)) / totalDamage;

	if (apActuallyLost > 0)
		abyss::AbyssPointsService::addAp(victim, -apActuallyLost);

	// Announce that player has died.
	if (victim.isInInstance() && !custom::pvpmap::PvpMapService::getInstance().isOnPvPMap(victim)) {
		utils::PacketSendUtility::broadcastPacketAndReceive(victim, SM_SYSTEM_MESSAGE::STR_MSG_COMBAT_FRIENDLY_DEATH_TO_B(victim.getName(), winner->getName()));
		utils::PacketSendUtility::sendPacket(victim, SM_SYSTEM_MESSAGE::STR_MSG_COMBAT_MY_DEATH());
	} else {
		utils::PacketSendUtility::sendPacket(*winner, SM_SYSTEM_MESSAGE::STR_MSG_COMBAT_HOSTILE_DEATH_TO_ME(victim.getName()));
		utils::PacketSendUtility::sendPacket(victim, SM_SYSTEM_MESSAGE::STR_MSG_COMBAT_MY_DEATH_TO_B(winner->getName()));
		utils::PacketSendUtility::broadcastPacket(victim, SM_SYSTEM_MESSAGE::STR_MSG_COMBAT_FRIENDLY_DEATH_TO_B(victim.getName(), winner->getName()), false,
			[&victim](Player& player) { return !player.isEnemy(victim); });
		utils::PacketSendUtility::broadcastPacket(*winner, SM_SYSTEM_MESSAGE::STR_MSG_COMBAT_HOSTILE_DEATH_TO_B(winner->getName(), victim.getName()), false,
			[&victim](Player& player) { return player.isEnemy(victim); });
		abyss::AbyssService::announceHighRankedDeath(victim);
	}
}

// Java PvpService.java:179-188
std::vector<runtime::Ptr<model::gameobjects::player::Player>> PvpService::findMembersToCountKillFor(model::gameobjects::player::Player& winner,
	model::gameobjects::player::Player& victim) {
	runtime::Ptr<model::team::TemporaryPlayerTeam> group = winner.getCurrentGroup();
	std::vector<runtime::Ptr<Player>> killers;
	if (group == nullptr)
		killers.push_back(runtime::Ptr<Player>(winner));
	else
		for (const runtime::Ptr<model::gameobjects::AionObject>& member : group->getMembers())
			killers.push_back(runtime::cast<Player>(member));
	std::erase_if(killers, [&winner, &victim](const runtime::Ptr<Player>& m) {
		return !m->isOnline() || m->getRace() == victim.getRace() || !m->equals(winner) && !utils::PositionUtil::isInRange(*m, victim, 50);
	});
	return killers;
}

// Java PvpService.java:190-212. A connection's MAC address is the empty string where Java's is null (AionConnection.macAddress)
void PvpService::logKill(model::gameobjects::player::Player& winner, model::gameobjects::player::Player& victim,
	const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& assistedGroup) {
	if (configs::main::LoggingConfig::LOG_KILL.load()) {
		bool winnerAssisted = std::ranges::find(assistedGroup, runtime::Ptr<Player>(winner)) != assistedGroup.end();
		if (assistedGroup.size() > 1 || assistedGroup.size() == 1 && !winnerAssisted) {
			std::string assistants;
			for (const runtime::Ptr<Player>& p : assistedGroup) {
				if (p->equals(winner))
					continue;
				if (!assistants.empty())
					assistants += ",";
				assistants += p->toString();
			}
			log.info("[KILL] " + winner.toString() + " killed " + victim.toString() + " assisted by " + assistants);
		} else
			log.info("[KILL] " + winner.toString() + " killed " + victim.toString());
	}

	if (configs::main::LoggingConfig::LOG_PL.load()) {
		std::shared_ptr<network::aion::AionConnection> winnerCon = winner.getClientConnection();
		std::shared_ptr<network::aion::AionConnection> victimCon = victim.getClientConnection();
		if (winnerCon == nullptr || victimCon == nullptr) // Java: getClientConnection().getIP() on null
			throw runtime::NullPointerException("Player.getClientConnection()");
		std::string ip1 = winnerCon->getIP();
		std::string mac1 = winnerCon->getMacAddress();
		std::string ip2 = victimCon->getIP();
		std::string mac2 = victimCon->getMacAddress();
		if (!mac1.empty() && !mac2.empty()) {
			if (commons::utils::StringUtils::equalsIgnoreCase(ip1, ip2) && commons::utils::StringUtils::equalsIgnoreCase(mac1, mac2)) {
				utils::audit::AuditLogger::log(winner,
					"possibly practicing AP sharing with " + victim.toString() + " same ip=" + ip1 + " and mac=" + mac1 + ".");
			} else if (commons::utils::StringUtils::equalsIgnoreCase(mac1, mac2)) {
				utils::audit::AuditLogger::log(winner, "possibly practicing AP sharing with " + victim.toString() + " same mac=" + mac1 + ".");
			}
		}
	}
}

// Java PvpService.java:214-260
bool PvpService::rewardPlayerTeam(const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& teamMember,
	model::gameobjects::player::Player& victim, int32_t damage, int32_t totalDamage, float apWinMulti) {
	using model::gameobjects::player::Rates;
	std::vector<runtime::Ptr<Player>> players;
	int32_t maxRank = 1;
	int32_t maxLevel = 0;

	for (const runtime::Ptr<Player>& member : teamMember) {
		if (!member->isOnline() || member->isDead() ||
			!utils::PositionUtil::isInRange(*member, victim, static_cast<float>(configs::main::GroupConfig::GROUP_MAX_DISTANCE.load())))
			continue;
		players.push_back(member);
		if (member->getLevel() > maxLevel)
			maxLevel = member->getLevel();
		if (utils::stats::id(member->getAbyssRank()->getRank()) > maxRank)
			maxRank = utils::stats::id(member->getAbyssRank()->getRank());
	}
	// They are all dead or out of range.
	if (players.empty())
		return false;

	float baseApReward = static_cast<float>(utils::stats::StatFunctions::calculatePvpApGained(victim, maxRank, maxLevel)) * apWinMulti;
	int32_t baseXpReward = utils::stats::StatFunctions::calculatePvpXpGained(victim, maxRank, maxLevel);
	int32_t baseDpReward = utils::stats::StatFunctions::calculatePvpDpGained(victim, maxRank, maxLevel);
	float groupDamagePercentage = static_cast<float>(damage) / static_cast<float>(totalDamage);
	const float size = static_cast<float>(players.size());
	int32_t apRewardPerMember = utils::JavaMath::round(baseApReward * groupDamagePercentage / size);
	int32_t xpRewardPerMember = utils::JavaMath::round(static_cast<float>(baseXpReward) * groupDamagePercentage / size);
	int32_t dpRewardPerMember = utils::JavaMath::round(static_cast<float>(baseDpReward) * groupDamagePercentage / size);

	for (const runtime::Ptr<Player>& member : players) {
		int32_t memberApGain = 1;
		int32_t memberXpGain = 1;
		int32_t memberDpGain = 1;
		if (controllers::attack::KillCounter::addKillFor(member->getObjectId(), victim.getObjectId()) <
			configs::main::CustomConfig::MAX_DAILY_PVP_KILLS.load()) {
			if (apRewardPerMember > 0)
				memberApGain = model::gameobjects::player::calcResult(Rates::AP_PVP, *member, apRewardPerMember);
			if (xpRewardPerMember > 0)
				memberXpGain = xpRewardPerMember; // rates are applied in addExp()
			if (dpRewardPerMember > 0) {
				memberDpGain = utils::stats::StatFunctions::adjustPvpDpGained(dpRewardPerMember, victim.getLevel(), member->getLevel());
				memberDpGain = model::gameobjects::player::calcResult(Rates::DP_PVP, *member, memberDpGain);
			}
		}
		abyss::AbyssPointsService::addAp(*member, victim, memberApGain);
		member->getCommonData()->addExp(memberXpGain, Rates::XP_PVP, victim.getName());
		member->getCommonData()->addDp(memberDpGain);
	}
	return true;
}

// Java PvpService.java:262-270
void PvpService::updateKillQuests(const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& killers,
	model::gameobjects::player::Player& victim) {
	std::vector<runtime::Ptr<world::zone::ZoneInstance>> zones = victim.findZones();
	for (const runtime::Ptr<Player>& p : killers) {
		for (const runtime::Ptr<world::zone::ZoneInstance>& zone : zones) {
			const world::zone::ZoneName* zoneName = zone->getAreaTemplate()->getZoneName();
			if (zoneName == nullptr) // Java: getZoneName().name() on null
				throw runtime::NullPointerException("Area.getZoneName()");
			questEngine::QuestEngine::getInstance().onKillInZone(*questEngine::model::QuestEnv::create(runtime::Ptr<model::gameobjects::VisibleObject>(victim), *p, 0),
				zoneName->name());
		}
		questEngine::QuestEngine::getInstance().onKillInWorld(*questEngine::model::QuestEnv::create(runtime::Ptr<model::gameobjects::VisibleObject>(victim), *p, 0),
			victim.getWorldId());
		questEngine::QuestEngine::getInstance().onKillRanked(*questEngine::model::QuestEnv::create(runtime::Ptr<model::gameobjects::VisibleObject>(victim), *p, 0),
			victim.getAbyssRank()->getRank());
	}
}

// Java PvpService.java:276-278
runtime::Ptr<model::event::Headhunter> PvpService::getHeadhunter(int32_t hunterId) {
	return headhunters.get(hunterId);
}

} // namespace aion::gameserver::services
