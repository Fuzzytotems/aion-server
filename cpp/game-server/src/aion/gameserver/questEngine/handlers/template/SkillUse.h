#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/questEngine/handlers/models/QuestSkillData.h"
#include "aion/gameserver/questEngine/handlers/template/AbstractTemplateQuestHandler.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/HashSet.h"

namespace aion::gameserver::questEngine::handlers::template_ {

/**
 * Java com.aionemu.gameserver.questEngine.handlers.template.SkillUse: the `skill_use` XML quests - casts of the listed skills counted in the
 * 6-bit quest vars.
 * <p>
 * C++ differences:
 * - A Java `List<Integer>` parameter that may be null is `std::optional<std::vector<int32_t>>` (nullopt for null).
 * - `qsd` points to the <skill> elements of the SkillUseData that built the handler (Java: the same objects; static data, alive for the
 *   process). An absent list is the bound empty vector, which Java's `Collections.emptyList()` also gives.
 * - The var arithmetic wraps like Java int (shift distances masked by 31).
 *
 * @author vlog, Bobobear, Pad
 */
class SkillUse : public AbstractTemplateQuestHandler {
private:
	runtime::HashSet<int32_t> startNpcIds{AION_LOCK_CLASS(SkillUse::startNpcIds)};
	runtime::HashSet<int32_t> endNpcIds{AION_LOCK_CLASS(SkillUse::endNpcIds)};
	runtime::ArrayList<const models::QuestSkillData*> qsd{AION_LOCK_CLASS(SkillUse::qsd)};

public:
	SkillUse(int32_t questId, const std::optional<std::vector<int32_t>>& startNpcIds, const std::optional<std::vector<int32_t>>& endNpcIds,
		const std::vector<models::QuestSkillData>& qsd);

	void register_() override;

	bool onDialogEvent(model::QuestEnv& env) override;

	bool onUseSkillEvent(model::QuestEnv& env, int32_t skillId) override;
};

} // namespace aion::gameserver::questEngine::handlers::template_
