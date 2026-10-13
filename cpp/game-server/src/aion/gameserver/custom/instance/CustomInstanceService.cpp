#include "aion/gameserver/custom/instance/CustomInstanceService.h"

#include <algorithm>
#include <chrono>
#include <string>

#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/cache/HTMLCache.h"
#include "aion/gameserver/custom/instance/CustomInstanceRankEnumInfo.h"
#include "aion/gameserver/custom/instance/CustomInstanceRankedPlayer.h"
#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModelEntry.h"
#include "aion/gameserver/dao/CustomInstanceDAO.h"
#include "aion/gameserver/dao/CustomInstancePlayerModelEntryDAO.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/HTMLService.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/utils/time/ServerTime.h"

namespace aion::gameserver::custom::instance {

namespace {

using utils::time::ServerTime;

/** Java: ZonedDateTime now = ServerTime.now(); reUseTime = now.with(LocalTime.of(RESET_HOUR, 0)); if (now.isBefore(reUseTime)) reUseTime =
 * reUseTime.minusDays(1); - as reUseTime.toEpochSecond() */
int64_t reUseEpochSecond(int32_t resetHour) {
	ServerTime::ZonedDateTime now = ServerTime::now();
	const std::chrono::local_days today = std::chrono::floor<std::chrono::days>(now.get_local_time());
	ServerTime::LocalDateTime reUseLocal = today + std::chrono::hours(resetHour);
	ServerTime::ZonedDateTime reUseTime = ServerTime::of(reUseLocal);
	if (now.get_sys_time() < reUseTime.get_sys_time())
		reUseTime = ServerTime::of(reUseLocal - std::chrono::days(1));
	return std::chrono::floor<std::chrono::seconds>(reUseTime.get_sys_time()).time_since_epoch().count();
}

} // namespace

CustomInstanceService::CustomInstanceService() {
	// Java's static initializer runs before the first constructor: the leaderboard window id is allocated here (class comment)
	static_cast<void>(leaderboardWindowObjectId());
}

CustomInstanceService::~CustomInstanceService() = default;

int32_t CustomInstanceService::leaderboardWindowObjectId() {
	static const int32_t id = utils::idfactory::IDFactory::getInstance().nextId();
	return id;
}

// Java CustomInstanceService.java:44-53
bool CustomInstanceService::canEnter(int32_t playerId) {
	std::optional<CustomInstanceRank> playerRankObject = dao::CustomInstanceDAO::loadPlayerRankObject(playerId);
	if (!playerRankObject)
		return true;
	return playerRankObject->getLastEntry() < reUseEpochSecond(RESET_HOUR) * 1000;
}

void CustomInstanceService::onEnter(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

// Java CustomInstanceService.java:66-71
CustomInstanceRank CustomInstanceService::loadOrCreateRank(int32_t playerId) {
	std::optional<CustomInstanceRank> customInstanceRank = dao::CustomInstanceDAO::loadPlayerRankObject(playerId);
	if (!customInstanceRank)
		customInstanceRank.emplace(playerId, 0, commons::utils::currentTimeMillis(), 0, 0);
	return *customInstanceRank;
}

// Java CustomInstanceService.java:73-88
bool CustomInstanceService::resetEntryCooldown(int32_t playerId) {
	std::optional<CustomInstanceRank> rankObj = dao::CustomInstanceDAO::loadPlayerRankObject(playerId);
	if (!rankObj)
		return false;
	int64_t reUseTime = reUseEpochSecond(RESET_HOUR);
	if (rankObj->getLastEntry() < reUseTime * 1000) {
		return false;
	} else {
		reUseTime = reUseTime - 1; // Java: reUseTime.minusSeconds(1)
		rankObj->setLastEntry(reUseTime * 1000);
		return dao::CustomInstanceDAO::storePlayer(*rankObj);
	}
}

// Java CustomInstanceService.java:90-94
bool CustomInstanceService::updateLastEntry(int32_t playerId, int64_t newEntryTime) {
	CustomInstanceRank rankObj = loadOrCreateRank(playerId);
	rankObj.setLastEntry(newEntryTime);
	return dao::CustomInstanceDAO::storePlayer(rankObj);
}

// Java CustomInstanceService.java:96-101
bool CustomInstanceService::changePlayerRank(int32_t playerId, int32_t newRank, int32_t achievedDps) {
	CustomInstanceRank rankObj = loadOrCreateRank(playerId);
	changeRank(rankObj, newRank);
	rankObj.setDps(achievedDps);
	return storeNewRankData(rankObj);
}

// Java CustomInstanceService.java:103-105
bool CustomInstanceService::storeNewRankData(CustomInstanceRank& rankObj) {
	return dao::CustomInstanceDAO::storePlayer(rankObj);
}

// Java CustomInstanceService.java:107-111
void CustomInstanceService::changeRank(CustomInstanceRank& rankObj, int32_t newRank) {
	rankObj.setRank(newRank);
	if (newRank > rankObj.getMaxRank())
		rankObj.setMaxRank(newRank);
}

// Java CustomInstanceService.java:113-120
void CustomInstanceService::recordPlayerModelEntry(model::gameobjects::player::Player& player, skillengine::model::Skill& skill,
	runtime::Ptr<model::gameobjects::VisibleObject> target) {
	// FILTER: Only record roah custom instance skills for the moment
	if (std::find(restrictedSkills.begin(), restrictedSkills.end(), skill.getSkillId()) != restrictedSkills.end())
		return;

	runtime::Ptr<runtime::RcArrayList<runtime::Ref<neuralnetwork::PlayerModelEntry>>> entries = getPlayerModelEntries(player.getObjectId());
	entries->add(neuralnetwork::PlayerModelEntry::create(player, skill.getSkillId(), runtime::as<model::gameobjects::Creature>(target)));
}

// Java CustomInstanceService.java:122-124
std::vector<runtime::Ref<neuralnetwork::PlayerModelEntry>> CustomInstanceService::loadPlayerModelEntries(int32_t playerId) {
	return dao::CustomInstancePlayerModelEntryDAO::loadPlayerModelEntries(playerId);
}

// Java CustomInstanceService.java:126-132
void CustomInstanceService::saveNewPlayerModelEntries(int32_t playerId) {
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<neuralnetwork::PlayerModelEntry>>> pmes = playerModelEntriesCache.remove(playerId);
	if (pmes == nullptr)
		return;
	std::vector<runtime::Ptr<neuralnetwork::PlayerModelEntry>> filteredEntries;
	for (runtime::Ptr<neuralnetwork::PlayerModelEntry> pme : *pmes)
		if (model::gameobjects::Persistable::NEW(*pme))
			filteredEntries.push_back(pme);
	dao::CustomInstancePlayerModelEntryDAO::insertNewRecords(filteredEntries);
}

// Java CustomInstanceService.java:134-136
runtime::Ptr<runtime::RcArrayList<runtime::Ref<neuralnetwork::PlayerModelEntry>>> CustomInstanceService::getPlayerModelEntries(int32_t playerId) {
	return playerModelEntriesCache.computeIfAbsent(playerId, [] {
		return runtime::RcArrayList<runtime::Ref<neuralnetwork::PlayerModelEntry>>::create(
			AION_LOCK_CLASS(CustomInstanceService::playerModelEntriesCache#list));
	});
}

// Java CustomInstanceService.java:138-173
void CustomInstanceService::openLeaderboard(model::gameobjects::player::Player& player, model::Race race) {
	std::vector<CustomInstanceRankedPlayer> rankedPlayers = dao::CustomInstanceDAO::loadTop10(race);
	// Java: the text block, its common indentation stripped
	std::string content = "<br><br><br>\n"
						  "<font color='3E2601' size='4'>Eternal Challenge Leaderboard</font><br>\n"
						  "<br>\n"
						  "<img src='textures/ui/basic_sep1.dds' width='300' height='2'><br>\n"
						  "<br><br>\n"
						  "<table>\n"
						  "\t<tr>\n"
						  "\t\t<th align='right'><font color='3E2601'>#</font></th>\n"
						  "\t\t<th>&nbsp;&nbsp;</th>\n"
						  "\t\t<th colspan='2' align='center'><font color='3E2601'>Name</font></th>\n"
						  "\t\t<th>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</th>\n"
						  "\t\t<th align='center'><font color='3E2601'>Rank</font></th>\n"
						  "\t</tr>\n";
	int32_t rank = 1;
	for (const CustomInstanceRankedPlayer& p : rankedPlayers) {
		content += "<tr>";
		content += "  <td align='right'><font color='3E2601'>" + std::to_string(rank++) + "</font></td>";
		content += "  <td></td>";
		content += "  <td background='textures/black_smoke2.DDS'><img src='" + std::string(model::getIconImage(p.getPlayerClass())) + "' width='24'></td>";
		content += "  <td><font color='3E2601'>" + p.getName() + "</font></td>";
		content += "  <td></td>";
		content += "  <td><font color='3E2601'>" + getRankDescription(p.getRank()) + "</font></td>";
		content += "</tr>";
	}
	content += "</table>";
	std::optional<std::string> page = cache::HTMLCache::getInstance().getHTML("simplePageTemplate.xhtml");
	if (!page) // Java: page.replace of a null page
		throw runtime::NullPointerException("simplePageTemplate.xhtml is not cached");
	services::HTMLService::sendData(player, leaderboardWindowObjectId(), commons::utils::StringUtils::replace(*page, "%content%", content));
}

CustomInstanceService& CustomInstanceService::getInstance() {
	static CustomInstanceService instance; // Java: SingletonHolder
	return instance;
}

} // namespace aion::gameserver::custom::instance
