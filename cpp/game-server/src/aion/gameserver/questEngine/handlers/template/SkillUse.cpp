#include "aion/gameserver/questEngine/handlers/template/SkillUse.h"

#include <algorithm>

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::questEngine::handlers::template_ {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::player::Player;
using model::QuestState;
using model::QuestStatus;
using models::QuestSkillData;

namespace {

/** Java int arithmetic (two's complement wrap-around; a shift distance is masked by 31) */
constexpr int32_t javaAdd(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

constexpr int32_t javaSub(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

constexpr int32_t javaMul(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

constexpr int32_t javaShl(int32_t a, int32_t distance) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) << (distance & 31));
}

/** Java `qd.getSkillIds()` dereferenced (the ids attribute is required; NullPointerException without it) */
const std::vector<int32_t>& skillIdsOf(const QuestSkillData& qd) {
	if (!qd.getSkillIds())
		throw runtime::NullPointerException("QuestSkillData.skillIds");
	return *qd.getSkillIds();
}

} // namespace

SkillUse::SkillUse(int32_t questIdValue, const std::optional<std::vector<int32_t>>& startNpcIdsValue,
	const std::optional<std::vector<int32_t>>& endNpcIdsValue, const std::vector<QuestSkillData>& qsdValue)
	: AbstractTemplateQuestHandler(questIdValue) {
	if (startNpcIdsValue)
		startNpcIds.addAll(*startNpcIdsValue);
	if (endNpcIdsValue)
		endNpcIds.addAll(*endNpcIdsValue);
	else
		endNpcIds.addAll(startNpcIds.snapshot());
	for (const QuestSkillData& qd : qsdValue)
		qsd.add(&qd);
}

void SkillUse::register_() {
	for (int32_t startNpcId : startNpcIds) {
		qe.registerQuestNpc(startNpcId)->addOnQuestStart(questId);
		qe.registerQuestNpc(startNpcId)->addOnTalkEvent(questId);
	}
	if (!equalSets(endNpcIds, startNpcIds)) {
		for (int32_t endNpcId : endNpcIds)
			qe.registerQuestNpc(endNpcId)->addOnTalkEvent(questId);
	}
	for (const QuestSkillData* questSkillData : qsd.snapshot()) {
		for (int32_t skillId : skillIdsOf(*questSkillData))
			qe.registerQuestSkill(skillId, questId);
	}
}

bool SkillUse::onDialogEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	int32_t dialogActionId = env.getDialogActionId();
	int32_t targetId = env.getTargetId();

	if (!qs || qs->isStartable()) {
		if (startNpcIds.isEmpty() || startNpcIds.contains(targetId)) {
			if (dialogActionId == DialogAction::QUEST_SELECT)
				return sendQuestDialog(env, 4762);
			else
				return sendQuestStartDialog(env);
		}
	} else if (qs->getStatus() == QuestStatus::START) {
		// TODO: check skill use count, see MonsterHunt.java how to get total count
		int32_t var = qs->getQuestVarById(0);
		if (endNpcIds.contains(targetId)) {
			if (dialogActionId == DialogAction::QUEST_SELECT) {
				return sendQuestDialog(env, 10002);
			} else if (dialogActionId == DialogAction::SELECT_QUEST_REWARD) {
				changeQuestStep(env, var, var, true); // reward
				return sendQuestDialog(env, 5);
			}
		}
	} else if (qs->getStatus() == QuestStatus::REWARD) {
		if (endNpcIds.contains(targetId)) {
			return sendQuestEndDialog(env);
		}
	}
	return false;
}

bool SkillUse::onUseSkillEvent(model::QuestEnv& env, int32_t skillId) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (qs && qs->getStatus() == QuestStatus::START) {
		int8_t rewardCount = 0; // Java: byte
		bool success = false;
		for (const QuestSkillData* qd : qsd.snapshot()) {
			const std::vector<int32_t>& skillIds = skillIdsOf(*qd);
			if (std::ranges::find(skillIds, skillId) != skillIds.end()) {
				int32_t endVar = qd->getEndVar();
				int32_t varId = qd->getVarNum();
				int32_t total = 0;
				do {
					int32_t currentVar = qs->getQuestVarById(varId);
					total = javaAdd(total, javaShl(currentVar, javaMul(javaSub(varId, qd->getVarNum()), 6)));
					endVar >>= 6;
					varId = javaAdd(varId, 1);
				} while (endVar > 0);
				total = javaAdd(total, 1);
				if (total <= qd->getEndVar()) {
					for (int32_t varsUsed = qd->getVarNum(); varsUsed < varId; varsUsed++) {
						int32_t value = total & 0x3F;
						total >>= 6;
						qs->setQuestVarById(varsUsed, value);
					}
					if (qs->getQuestVarById(qd->getVarNum()) == qd->getEndVar())
						rewardCount = static_cast<int8_t>(rewardCount + 1);
					updateQuestStatus(env);
					success = true;
				}
			}
		}
		if (rewardCount == qsd.size()) {
			if (qs->getQuestVarById(0) == 0)
				qs->setQuestVarById(0, 1);
			qs->setStatus(QuestStatus::REWARD);
			updateQuestStatus(env);
		}
		return success;
	}
	return false;
}

} // namespace aion::gameserver::questEngine::handlers::template_
