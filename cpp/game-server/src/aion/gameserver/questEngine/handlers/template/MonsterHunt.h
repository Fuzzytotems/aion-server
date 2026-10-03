#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/model/templates/quest/fwd.h"
#include "aion/gameserver/questEngine/handlers/models/Monster.h"
#include "aion/gameserver/questEngine/handlers/template/AbstractTemplateQuestHandler.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::questEngine::handlers::template_ {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.template.MonsterHunt: the `monster_hunt` XML quests - kill counts packed into the 6-bit
 * quest vars (the <quest_kill> rows of quest_data.xml, which MonsterHuntData::register_ turns into Monster objects).
 * <p>
 * C++ differences:
 * - A Java `List<Integer>` parameter that may be null is `std::optional<std::vector<int32_t>>` (nullopt for null). `startZone` is the bound
 *   string, "" for Java null (xmlgen binds an absent attribute as ""; no quest in the data has `start_zone=""`, docs/deviations/P5-06c.md).
 * - `monsters` owns the Monster objects the data class builds for the handler (Java: `new Monster()` per kill row, kept only by this list).
 * - The var arithmetic wraps like Java int (shift distances masked by 31).
 *
 * @author MrPoke, vlog, Bobobear, Pad, Majka
 */
class MonsterHunt : public AbstractTemplateQuestHandler {
private:
	runtime::HashSet<int32_t> startNpcIds{AION_LOCK_CLASS(MonsterHunt::startNpcIds)};
	runtime::HashSet<int32_t> endNpcIds{AION_LOCK_CLASS(MonsterHunt::endNpcIds)};
	// Java: a new list of `new Monster()` objects that only this handler keeps (MonsterHuntData.register), or Collections.emptyList().
	// fieldmap: the handler owns the Monster values register_ builds and never changes them after the constructor (a const vector)
	const std::vector<models::Monster> monsters;
	const int32_t startDialogId;
	const int32_t endDialogId;
	runtime::HashSet<int32_t> aggroNpcIds{AION_LOCK_CLASS(MonsterHunt::aggroNpcIds)};
	const int32_t invasionWorldId;
	const gameserver::model::templates::quest::QuestItems* workItem = nullptr;
	const std::string startZone;
	const int32_t startDistanceNpcId;
	const bool reward;
	const bool rewardNextStep;
	const bool isDataDriven;

public:
	MonsterHunt(int32_t questId, const std::optional<std::vector<int32_t>>& startNpcIds, const std::optional<std::vector<int32_t>>& endNpcIds,
		std::vector<models::Monster> monsters, int32_t startDialogId, int32_t endDialogId, const std::optional<std::vector<int32_t>>& aggroNpcIds,
		int32_t invasionWorld, std::string startZone, int32_t startDistanceNpcId, bool reward, bool rewardNextStep);

	void register_() override;

	bool onDialogEvent(model::QuestEnv& env) override;

	bool onKillEvent(model::QuestEnv& env) override;

	bool onAddAggroListEvent(model::QuestEnv& env) override;

	bool onEnterWorldEvent(model::QuestEnv& env) override;

private:
	bool searchOpenRift();

public:
	bool onEnterZoneEvent(model::QuestEnv& env, const world::zone::ZoneName* zoneName) override;

	bool onAtDistanceEvent(model::QuestEnv& env) override;

	bool startQuest(model::QuestEnv& env);
};

} // namespace aion::gameserver::questEngine::handlers::template_
