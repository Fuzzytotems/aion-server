#include "aion/gameserver/handlers/admincommands/Quest.h"

#include <vector>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFaction.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_COMPLETED_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/EnumValueOf.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Quest);

// Java Quest.java:35-45
Quest::Quest()
	: AdminCommand("quest", "Handles quest states of your target.",
		  "[player] <quest> <reset|start|delete> - Resets/starts/deletes the specified quest.\n"
		  "[player] <quest> status - Shows the quest status of the specified quest.\n"
		  "[player] <quest> set <status> <var> [varNum] - Sets the specified quest state (default: apply var to all varNums, optional: set var to "
		  "varNum [0-5]).\n"
		  "[player] <quest> setflags <flags> - Sets the specified quest flags.\n"
		  "[player] <quest> dialog <page ID> - Sends the dialog page with the given page ID.\n"
		  "Note: If no player parameter is given, your current target will be taken (defaults to your character, if no player is targeted).\n") {
}

// Java Quest.java:46-112
void Quest::execute(Player& admin, std::span<const std::string> params) {
	using commons::utils::StringUtils::equalsIgnoreCase;
	if (params.size() == 0) { // parity= if (params.length == 0) {
		sendInfo(admin);
		return;
	}

	int8_t index = 0;
	runtime::Ptr<Player> target;
	int32_t questId = ChatUtil::getQuestId(params[index]);
	if (questId == 0) {
		std::string playerName = Util::convertName(params[index]);
		target = World::getInstance().getPlayer(playerName);
		if (target == nullptr) {
			PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER(playerName));
			return;
		}
		if (++index >= static_cast<int32_t>(params.size())) { // parity= if (++index >= params.length) {
			sendInfo(admin);
			return;
		}
		questId = ChatUtil::getQuestId(params[index]);
	} else {
		runtime::Ptr<Player> p = runtime::as<Player>(admin.getTarget()); // parity= target = admin.getTarget() instanceof Player p ? p : admin;
		target = p != nullptr ? p : runtime::Ptr<Player>(admin); // parity: (the same)
	}

	if (questId == 0 || DataManager::QUEST_DATA->getQuestById(questId) == nullptr) {
		sendInfo(admin, "Invalid quest.");
		return;
	}

	if (++index >= static_cast<int32_t>(params.size())) { // parity= if (++index >= params.length) {
		sendInfo(admin);
		return;
	}

	// quest and target are both valid at this point
	if (equalsIgnoreCase(params[index], "reset")) { // parity= if (params[index].equalsIgnoreCase("reset")) {
		resetQuest(admin, *target, questId);
	} else if (equalsIgnoreCase(params[index], "start")) { // parity= } else if (params[index].equalsIgnoreCase("start")) {
		startQuest(admin, *target, questId);
	} else if (equalsIgnoreCase(params[index], "delete")) { // parity= } else if (params[index].equalsIgnoreCase("delete")) {
		deleteQuest(admin, *target, questId);
	} else if (equalsIgnoreCase(params[index], "status")) { // parity= } else if (params[index].equalsIgnoreCase("status")) {
		showQuestStatus(admin, *target, questId);
	} else if (equalsIgnoreCase(params[index], "set") && static_cast<int32_t>(params.size()) > index + 2) { // parity= } else if (params[index].equalsIgnoreCase("set") && params.length > index + 2) {
		QuestStatus status = utils::enumValueOf<QuestStatus>(commons::utils::StringUtils::toUpperCase(params[++index])); // parity= QuestStatus status = QuestStatus.valueOf(params[++index].toUpperCase());
		int32_t var = commons::utils::parseInt(params[++index]); // parity= int var = Integer.parseInt(params[++index]);
		int32_t varNum = -1;
		if (++index < static_cast<int32_t>(params.size())) { // optional // parity= if (++index < params.length) {
			varNum = commons::utils::parseInt(params[index]); // parity= varNum = Integer.parseInt(params[index]);
			if (varNum < 0 || varNum > 5) {
				sendInfo(admin, "[varNum] must be between 0 and 5.");
				return;
			}
		}
		setQuestStatus(admin, *target, questId, status, var, varNum);
	} else if (equalsIgnoreCase(params[index], "setflags") && static_cast<int32_t>(params.size()) > index + 1) { // parity= } else if (params[index].equalsIgnoreCase("setflags") && params.length > index + 1) {
		int32_t flags = commons::utils::parseInt(params[++index]); // parity= int flags = Integer.parseInt(params[++index]);
		setQuestFlags(admin, *target, questId, flags);
	} else if (equalsIgnoreCase(params[index], "dialog") && static_cast<int32_t>(params.size()) > index + 1) { // parity= } else if (params[index].equalsIgnoreCase("dialog") && params.length > index + 1) {
		int32_t dialogPageId = commons::utils::parseInt(params[++index]); // parity= int dialogPageId = Integer.parseInt(params[++index]);
		sendQuestDialog(admin, questId, dialogPageId);
	} else {
		sendInfo(admin);
	}
}

// Java Quest.java:114-129
void Quest::resetQuest(Player& admin, Player& target, int32_t questId) {
	runtime::Ptr<QuestState> qs = target.getQuestStateList()->getQuestState(questId);
	if (qs == nullptr || qs->getStatus() != QuestStatus::START) {
		sendInfo(admin, "Only currently active quests can be reset.");
		return;
	}
	if (qs->getQuestVars()->getQuestVars() == 0 && !qs->getRewardGroup()) { // parity= if (qs.getQuestVars().getQuestVars() == 0 && qs.getRewardGroup() == null) {
		sendInfo(admin, name(target) + "'s quest is already at the beginning.");
		return;
	}
	qs->setStatus(QuestStatus::START);
	qs->setQuestVar(0);
	qs->setRewardGroup(std::nullopt); // parity= qs.setRewardGroup(null);
	PacketSendUtility::sendPacket(target, SM_QUEST_ACTION(ActionType::UPDATE, *qs));
	sendInfo(admin, "Reset " + ChatUtil::quest(questId) + " for " + name(target) + ".");
}

// Java Quest.java:131-164
void Quest::startQuest(Player& admin, Player& target, int32_t questId) {
	const QuestTemplate* template_ = DataManager::QUEST_DATA->getQuestById(questId); // parity= QuestTemplate template = DataManager.QUEST_DATA.getQuestById(questId);
	if (template_->getNpcFactionId() > 0) {
		startNpcFactionQuest(admin, target, questId, template_->getNpcFactionId());
		return;
	} else if (QuestService::startQuest(*QuestEnv::create(nullptr, target, questId))) { // parity= } else if (QuestService.startQuest(new QuestEnv(null, target, questId))) {
		sendInfo(admin, "Started " + ChatUtil::quest(questId) + " for " + name(target) + ".");
		return;
	}
	runtime::Ptr<QuestState> qs = target.getQuestStateList()->getQuestState(questId);
	if (qs != nullptr && (qs->getStatus() == QuestStatus::START || qs->getStatus() == QuestStatus::REWARD)) {
		sendInfo(admin, "Quest is already started.");
	} else if (qs != nullptr && qs->getStatus() == QuestStatus::COMPLETE && !qs->canRepeat()) {
		sendInfo(admin, "Quest is already completed.");
	} else {
		std::string sb; // parity= StringBuilder sb = new StringBuilder();
		const std::vector<XMLStartCondition>& preconditions = template_->getXMLStartConditions(); // parity= List<XMLStartCondition> preconditions = template.getXMLStartConditions();
		if (!preconditions.empty()) { // parity= if (preconditions != null) {
			for (const XMLStartCondition& condition : preconditions) { // parity= for (XMLStartCondition condition : preconditions) {
				const std::vector<FinishedQuestCond>& finisheds = condition.getFinishedPreconditions(); // parity= List<FinishedQuestCond> finisheds = condition.getFinishedPreconditions();
				if (!finisheds.empty()) { // parity= if (finisheds != null) {
					for (const FinishedQuestCond& fcondition : finisheds) { // parity= for (FinishedQuestCond fcondition : finisheds) {
						runtime::Ptr<QuestState> qs1 = target.getQuestStateList()->getQuestState(fcondition.getQuestId());
						if (qs1 == nullptr || qs1->getStatus() != QuestStatus::COMPLETE) {
							sb += "\n\t" + ChatUtil::quest(fcondition.getQuestId()); // parity= sb.append("\n\t" + ChatUtil.quest(fcondition.getQuestId()));
						}
					}
				}
			}
		}
		sendInfo(admin, // parity= sendInfo(admin, "Quest not started. " + (sb.length() > 0 ? "These quest(s) must be completed first:" + sb.toString() : "Some preconditions failed."));
			"Quest not started. " + (sb.size() > 0 ? "These quest(s) must be completed first:" + sb : std::string("Some preconditions failed."))); // parity: (the same)
	}
}

// Java Quest.java:166-190
void Quest::startNpcFactionQuest(Player& admin, Player& target, int32_t questId, int32_t factionId) {
	runtime::Ptr<NpcFaction> faction = target.getNpcFactions().getActiveNpcFaction(false);
	if (faction == nullptr || faction->getId() != factionId) {
		sendInfo(admin, name(target) + " is not registered to the organization for this quest.");
		return;
	}
	for (const QuestTemplate* template_ : DataManager::QUEST_DATA->getQuestsByNpcFaction(faction->getId(), target)) { // parity= for (QuestTemplate template : DataManager.QUEST_DATA.getQuestsByNpcFaction(faction.getId(), target)) {
		if (template_->getId() == questId) { // parity= if (template.getId() == questId) {
			// simulate daily reset
			faction->setActive(false);
			faction->setTime(-1);
			target.getNpcFactions().addNpcFaction(*faction);
			faction->setActive(true);
			// set daily quest Id and time to avoid random quest
			faction->setState(ENpcFactionQuestState::NOTING);
			faction->setQuestId(questId);
			faction->setTime(faction->getTime() + 100);
			// send the daily quest to player
			target.getNpcFactions().sendDailyQuest();
			sendInfo(admin, "Started NPC faction quest " + ChatUtil::quest(questId) + " for " + name(target) + ".");
			return;
		}
	}
	sendInfo(admin, "Quest not implemented or player level doesn't match.");
}

// Java Quest.java:192-209
void Quest::deleteQuest(Player& admin, Player& target, int32_t questId) {
	if (!admin.hasAccess(AdminConfig::CMD_QUEST_ADV_PARAMS.load())) { // parity= if (!admin.hasAccess(AdminConfig.CMD_QUEST_ADV_PARAMS)) {
		sendInfo(admin, "<You need access level " + std::to_string(AdminConfig::CMD_QUEST_ADV_PARAMS.load()) + " or higher to use this function>"); // parity= sendInfo(admin, "<You need access level " + AdminConfig.CMD_QUEST_ADV_PARAMS + " or higher to use this function>");
		return;
	}
	runtime::Ptr<QuestState> qs = target.getQuestStateList()->deleteQuest(questId);
	if (qs == nullptr) {
		sendInfo(admin, name(target) + " does not have that quest.");
		return;
	}
	if (qs->getStatus() == QuestStatus::COMPLETE)
		QuestEngine::getInstance().sendCompletedQuests(target); // rewrite completed quest list
	else
		PacketSendUtility::sendPacket(target, SM_QUEST_ACTION(ActionType::ABANDON, *qs));
	target.getController().updateNearbyQuests();
	if (!admin.equals(target))
		sendInfo(admin, "Deleted " + ChatUtil::quest(questId) + " for " + name(target) + ".");
}

// Java Quest.java:211-230
void Quest::showQuestStatus(Player& admin, Player& target, int32_t questId) {
	if (!admin.hasAccess(AdminConfig::CMD_QUEST_ADV_PARAMS.load())) { // parity= if (!admin.hasAccess(AdminConfig.CMD_QUEST_ADV_PARAMS)) {
		sendInfo(admin, "<You need access level " + std::to_string(AdminConfig::CMD_QUEST_ADV_PARAMS.load()) + " or higher to use this function>"); // parity= sendInfo(admin, "<You need access level " + AdminConfig.CMD_QUEST_ADV_PARAMS + " or higher to use this function>");
		return;
	}
	runtime::Ptr<QuestState> qs = target.getQuestStateList()->getQuestState(questId);
	std::string sb = "Player: " + name(target) + ", quest: " + ChatUtil::quest(questId) + "\n\tQuest status: "; // parity= StringBuilder sb = new StringBuilder("Player: " + name(target) + ", quest: " + ChatUtil.quest(questId) + "\n\tQuest status: ");
	if (qs == nullptr) {
		sb += "NULL"; // parity= sb.append("NULL");
	} else {
		sb += std::string(xml::enumName(qs->getStatus())); // parity= sb.append(qs.getStatus().toString());
		sb += "\n\tQuest vars:"; // parity= sb.append("\n\tQuest vars:");
		for (int32_t i = 0; i <= 5; i++)
			sb += " " + std::to_string(qs->getQuestVarById(i)); // parity= sb.append(" " + qs.getQuestVarById(i));
		sb += ", encoded [" + std::to_string(qs->getQuestVars()->getQuestVars()) + "]"; // parity= sb.append(", encoded [" + qs.getQuestVars().getQuestVars() + "]");
		sb += "\n\tQuest flags: " + std::to_string(qs->getFlags() & 0x3F) + " " + std::to_string(qs->getStepGroup()); // needs rework when flags are implemented like vars // parity= sb.append("\n\tQuest flags: " + (qs.getFlags() & 0x3F) + " " + qs.getStepGroup());
		sb += ", encoded [" + std::to_string(qs->getFlags()) + "]"; // parity= sb.append(", encoded [" + qs.getFlags() + "]");
	}
	sendInfo(admin, sb); // parity= sendInfo(admin, sb.toString());
}

// Java Quest.java:232-264
void Quest::setQuestStatus(Player& admin, Player& target, int32_t questId, QuestStatus status, int32_t var, int32_t varNum) {
	if (!admin.hasAccess(AdminConfig::CMD_QUEST_ADV_PARAMS.load())) { // parity= if (!admin.hasAccess(AdminConfig.CMD_QUEST_ADV_PARAMS)) {
		sendInfo(admin, "<You need access level " + std::to_string(AdminConfig::CMD_QUEST_ADV_PARAMS.load()) + " or higher to use this function>"); // parity= sendInfo(admin, "<You need access level " + AdminConfig.CMD_QUEST_ADV_PARAMS + " or higher to use this function>");
		return;
	}
	runtime::Ptr<QuestState> qs = target.getQuestStateList()->getQuestState(questId);
	ActionType actionType;
	if (qs == nullptr) { // player doesn't have that quest
		actionType = ActionType::ADD;
		runtime::Ref<QuestState> created = QuestState::create(questId, status); // parity= qs = new QuestState(questId, status);
		qs = created; // parity: (the same)
		target.getQuestStateList()->addQuest(questId, *qs);
	} else {
		actionType = qs->getStatus() == QuestStatus::COMPLETE ? ActionType::ADD : ActionType::UPDATE;
		qs->setStatus(status);
	}
	if (status == QuestStatus::COMPLETE) {
		qs->setQuestVar(0); // completed quests vars are always 0
		if (!DataManager::QUEST_DATA->getQuestById(qs->getQuestId())->getRewards().empty()) // parity= if (!DataManager.QUEST_DATA.getQuestById(qs.getQuestId()).getRewards().isEmpty())
			qs->setRewardGroup(0); // follow quests could require reward group > 0 to be unlocked (see quest_data.xml)
		QuestEngine::getInstance().onQuestCompleted(target, questId);
	} else {
		if (varNum == -1)
			qs->setQuestVar(var);
		else
			qs->setQuestVarById(varNum, var);
	}
	if (actionType == ActionType::ADD && status == QuestStatus::COMPLETE)
		PacketSendUtility::sendPacket(target, SM_QUEST_COMPLETED_LIST(1, {qs})); // parity= PacketSendUtility.sendPacket(target, new SM_QUEST_COMPLETED_LIST(1, Arrays.asList(qs)));
	else
		PacketSendUtility::sendPacket(target, SM_QUEST_ACTION(actionType, *qs));
	target.getController().updateNearbyQuests();
	sendInfo(admin, "Set quest status of " + ChatUtil::quest(questId) + " for " + name(target) + ".");
}

// Java Quest.java:266-279
void Quest::setQuestFlags(Player& admin, Player& target, int32_t questId, int32_t flags) { // needs rework when flags are implemented like vars
	if (!admin.hasAccess(AdminConfig::CMD_QUEST_ADV_PARAMS.load())) { // parity= if (!admin.hasAccess(AdminConfig.CMD_QUEST_ADV_PARAMS)) {
		sendInfo(admin, "<You need access level " + std::to_string(AdminConfig::CMD_QUEST_ADV_PARAMS.load()) + " or higher to use this function>"); // parity= sendInfo(admin, "<You need access level " + AdminConfig.CMD_QUEST_ADV_PARAMS + " or higher to use this function>");
		return;
	}
	runtime::Ptr<QuestState> qs = target.getQuestStateList()->getQuestState(questId);
	if (qs == nullptr || qs->getStatus() != QuestStatus::START) {
		sendInfo(admin, "Flags can only be set for active quests.");
		return;
	}
	qs->setFlags(flags);
	PacketSendUtility::sendPacket(target, SM_QUEST_ACTION(ActionType::UPDATE, *qs));
	sendInfo(admin, "Set " + name(target) + "'s quest flags to " + std::to_string(flags) + ".");
}

// Java Quest.java:281-288
void Quest::sendQuestDialog(Player& admin, int32_t questId, int32_t dialogPageId) {
	if (!admin.hasAccess(AdminConfig::CMD_QUEST_ADV_PARAMS.load())) { // parity= if (!admin.hasAccess(AdminConfig.CMD_QUEST_ADV_PARAMS)) {
		sendInfo(admin, "<You need access level " + std::to_string(AdminConfig::CMD_QUEST_ADV_PARAMS.load()) + " or higher to use this function>"); // parity= sendInfo(admin, "<You need access level " + AdminConfig.CMD_QUEST_ADV_PARAMS + " or higher to use this function>");
		return;
	}
	PacketSendUtility::sendPacket(admin, SM_DIALOG_WINDOW(0, dialogPageId, questId));
	sendInfo(admin, "Sent dialog page " + std::to_string(dialogPageId) + " of Q" + std::to_string(questId) + ".");
}

} // namespace aion::gameserver::handlers::admincommands
