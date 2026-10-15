#include "aion/gameserver/questEngine/QuestSpawnAnalyzer.h"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/EventData.h"
#include "aion/gameserver/dataholders/NpcFactionsData.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/dataholders/TownSpawnsData.h"
#include "aion/gameserver/dataholders/XMLQuests.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/factions/NpcFactionTemplate.h"
#include "aion/gameserver/model/templates/quest/FinishedQuestCond.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/model/templates/quest/XMLStartCondition.h"
#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"
#include "aion/gameserver/questEngine/handlers/models/XMLQuest.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::questEngine {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.questEngine.QuestSpawnAnalyzer");

namespace {

using gameserver::model::templates::QuestTemplate;
using gameserver::model::templates::factions::NpcFactionTemplate;
using gameserver::model::templates::quest::FinishedQuestCond;
using gameserver::model::templates::quest::QuestNpc;
using gameserver::model::templates::quest::XMLStartCondition;

/** Java: DataManager.QUEST_DATA.getQuestById(questId), dereferenced by the caller (NullPointerException for an unknown quest id) */
const QuestTemplate& questTemplateOf(int32_t questId) {
	const QuestTemplate* qt = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
	if (qt == nullptr)
		throw runtime::NullPointerException("qt (no quest template " + std::to_string(questId) + ")");
	return *qt;
}

/** Java: stream().sorted().map(String::valueOf).collect(Collectors.joining(delimiter)) over integers (natural order) */
template <class Range>
std::string joinSorted(const Range& ids, const std::string& delimiter) {
	std::vector<int32_t> sorted(ids.begin(), ids.end());
	std::ranges::sort(sorted);
	std::string joined;
	for (int32_t id : sorted) {
		if (!joined.empty())
			joined += delimiter;
		joined += std::to_string(id);
	}
	return joined;
}

} // namespace

void QuestSpawnAnalyzer::run(const std::vector<handlers::AbstractQuestHandler*>& questHandlers, const std::vector<runtime::Ptr<QuestNpc>>& questNpcs,
	bool ignoreEventQuests) {
	log.info(std::string("Analyzing quest handlers (ignoreEventQuests=") + (ignoreEventQuests ? "true" : "false") + ")...");
	int64_t timeMillis = commons::utils::currentTimeMillis();
	std::unordered_set<int32_t> unobtainableQuests;
	std::unordered_set<int32_t> factionIds;
	std::unordered_set<int32_t> allSpawns = loadNpcIdsSpawnedByHandlers();
	dataholders::DataManager::SPAWNS_DATA->addAllNpcIdsToSet(allSpawns);
	dataholders::DataManager::TOWN_SPAWNS_DATA->addAllNpcIdsToSet(allSpawns);
	dataholders::DataManager::EVENT_DATA->addAllNpcIdsToSet(allSpawns);
	for (const NpcFactionTemplate* nft : dataholders::DataManager::NPC_FACTIONS_DATA->getNpcFactionsData()) {
		const std::optional<std::vector<int32_t>>& npcIds = nft->getNpcIds();
		if (!npcIds || std::ranges::any_of(*npcIds, [&allSpawns](int32_t npcId) { return allSpawns.contains(npcId); }))
			factionIds.insert(nft->getId());
	}
	for (handlers::AbstractQuestHandler* qh : questHandlers) {
		const QuestTemplate& qt = questTemplateOf(qh->getQuestId());
		if (qt.getMinlevelPermitted() == 99 || (qt.getNpcFactionId() > 0 && !factionIds.contains(qt.getNpcFactionId())))
			unobtainableQuests.insert(qh->getQuestId()); // players can still have these quests from before an update
	}
	// Java: a HashMap<Set<Integer>, List<Integer>>, keys compared as sets. Its iteration order reaches no output: every line is built from
	// sorted ids and the lines are sorted before they are joined
	std::map<std::set<int32_t>, std::vector<int32_t>> missingSpawnsByQuests;
	for (const runtime::Ptr<QuestNpc>& npc : questNpcs) {
		if (allSpawns.contains(npc->getNpcId()))
			continue;
		int32_t npcId = npc->getNpcId();
		std::unordered_set<int32_t> questIds = npc->findAllRegisteredQuestIds([&](int32_t id) {
			return (!ignoreEventQuests || id < 80000) && !isUnobtainable(id, unobtainableQuests) &&
				!existsSpawnDataForAnyAlternativeNpc(id, npcId, allSpawns);
		});
		if (questIds.empty())
			continue;
		missingSpawnsByQuests[std::set<int32_t>(questIds.begin(), questIds.end())].push_back(npcId);
	}
	timeMillis = commons::utils::currentTimeMillis() - timeMillis;
	if (missingSpawnsByQuests.empty()) {
		log.info("Quest handler analysis finished in " + std::to_string(timeMillis) + " ms without errors");
	} else {
		std::vector<std::string> lines;
		for (const auto& [questIdSet, npcIds] : missingSpawnsByQuests)
			lines.push_back("\n\tNpc " + joinSorted(npcIds, "/") + " (quests: " + joinSorted(questIdSet, ", ") + ")");
		std::ranges::sort(lines); // Java: Stream.sorted() of the strings (all ASCII: UTF-16 order is byte order)
		std::string missingSpawns;
		for (const std::string& line : lines)
			missingSpawns += line;
		log.warn("Quest handler analysis finished in " + std::to_string(timeMillis) + " ms. Found " + std::to_string(missingSpawnsByQuests.size()) +
			" missing quest npc spawns:" + missingSpawns);
	}
}

bool QuestSpawnAnalyzer::isUnobtainable(int32_t questId, const std::unordered_set<int32_t>& unobtainableQuests) {
	if (unobtainableQuests.contains(questId))
		return true;
	const QuestTemplate& qt = questTemplateOf(questId);
	for (const XMLStartCondition& startCondition : qt.getXMLStartConditions()) {
		// Java: getFinishedPreconditions() == null without <finished>; C++: an empty list (JAXB never binds an empty one)
		if (startCondition.getFinishedPreconditions().empty())
			continue;
		// java-bug kept: the recursion has no cycle guard. A quest that reached itself through its <finished> preconditions would end Java in a
		// StackOverflowError (logged by the pool's wrapper) and C++ in a stack overflow of the process. On the shipped data it never happens: the
		// only such quests are 80313 and 80314, each naming itself (quest_data.xml), and no quest registered at an npc reaches them - neither has
		// an XML quest script or a Java handler, and no other quest names them under <finished>. QuestEngine::init's ignoreEventQuests = true
		// also drops every id from 80000 on before this call (the filter comes first, QuestSpawnAnalyzer.java:56).
		if (std::ranges::all_of(startCondition.getFinishedPreconditions(),
				[&unobtainableQuests](const FinishedQuestCond& fpc) { return isUnobtainable(fpc.getQuestId(), unobtainableQuests); }))
			return true;
	}
	return false;
}

bool QuestSpawnAnalyzer::existsSpawnDataForAnyAlternativeNpc(int32_t questId, int32_t npcId, const std::unordered_set<int32_t>& allSpawns) {
	const handlers::models::XMLQuest* quest = dataholders::DataManager::XML_QUESTS->getQuest(questId);
	if (quest == nullptr)
		return true; // no way to get alternative npcs from non-xml based handlers, so assume the quest spawns work (lol)
	std::optional<std::unordered_set<int32_t>> alternativeNpcs = quest->getAlternativeNpcs(npcId);
	if (!alternativeNpcs)
		return false;
	return std::ranges::any_of(*alternativeNpcs, [&allSpawns](int32_t id) { return allSpawns.contains(id); });
}

std::unordered_set<int32_t> QuestSpawnAnalyzer::loadNpcIdsSpawnedByHandlers() {
	std::unordered_set<int32_t> npcIds;
	// Java: pattern \bsp(?:awn)?\([^,\d]*(\d{6})(?: : (\d{6}))? and parseSpawnNpcIds over InstanceConfig.HANDLER_DIRECTORY,
	// GSConfig.QUEST_HANDLER_DIRECTORY and AIConfig.HANDLER_DIRECTORY; C++: the table aion_gs_regscan built with that pattern from the
	// compiled-in handler sources of the same three directories (the class comment)
	std::span<const int32_t> spawnedByHandlers = gameserver::handlers::npcIdsSpawnedByHandlers();
	npcIds.insert(spawnedByHandlers.begin(), spawnedByHandlers.end());
	return npcIds;
}

} // namespace aion::gameserver::questEngine
