#include "aion/gameserver/handlers/consolecommands/Addquest.h"

#include <regex>
#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/services/QuestService.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Addquest);

Addquest::Addquest() : ConsoleCommand("addquest") {
}

// Java Addquest.java:28-74
void Addquest::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 1) {
		info(admin, std::nullopt); // parity= info(admin, null);
		return;
	}

	runtime::Ptr<Player> targetPlayer = runtime::as<Player>(admin.getTarget()); // parity= final Player player = admin.getTarget() instanceof Player target ? target : admin;
	Player& player = targetPlayer != nullptr ? *targetPlayer : admin; // parity: (the same)

	int32_t id;
	try {
		const std::string& quest = params[0]; // parity= String quest = params[0];
		static const auto* const questId = new std::regex("\\[quest:([^%]+)\\]"); // ECMAScript: the closing bracket escaped // parity= Pattern questId = Pattern.compile("\\[quest:([^%]+)]");
		std::smatch result; // parity= Matcher result = questId.matcher(quest);
		if (std::regex_search(quest, result, *questId)) // parity= if (result.find())
			id = commons::utils::parseInt(result[1].str()); // parity= id = Integer.parseInt(result.group(1));
		else
			id = commons::utils::parseInt(params[0]); // parity= id = Integer.parseInt(params[0]);
	} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException e) {
		info(admin, std::nullopt); // parity= info(admin, null);
		return;
	}

	runtime::Ref<QuestEnv> env = QuestEnv::create(nullptr, player, id); // parity= QuestEnv env = new QuestEnv(null, player, id);

	if (QuestService::startQuest(*env)) {
		PacketSendUtility::sendMessage(admin, "Quest started.");
	} else {
		const QuestTemplate* template_ = DataManager::QUEST_DATA->getQuestById(id); // parity= QuestTemplate template = DataManager.QUEST_DATA.getQuestById(id);
		if (template_ == nullptr) // parity: Java's NullPointerException of template.getXMLStartConditions(), explicit
			throw runtime::NullPointerException("QuestTemplate"); // parity: (the same)
		const std::vector<XMLStartCondition>& preconditions = template_->getXMLStartConditions(); // parity= List<XMLStartCondition> preconditions = template.getXMLStartConditions();
		if (preconditions.size() > 0) { // parity= if (preconditions != null && preconditions.size() > 0) {
			for (const XMLStartCondition& condition : preconditions) { // parity= for (XMLStartCondition condition : preconditions) {
				const std::vector<FinishedQuestCond>& finisheds = condition.getFinishedPreconditions(); // parity= List<FinishedQuestCond> finisheds = condition.getFinishedPreconditions();
				if (finisheds.size() > 0) { // parity= if (finisheds != null && finisheds.size() > 0) {
					for (const FinishedQuestCond& fcondition : finisheds) { // parity= for (FinishedQuestCond fcondition : finisheds) {
						runtime::Ptr<QuestState> qs1 = admin.getQuestStateList()->getQuestState(fcondition.getQuestId());
						if (qs1 == nullptr || qs1->getStatus() != QuestStatus::COMPLETE) {
							PacketSendUtility::sendMessage(admin, "You have to finish " + std::to_string(fcondition.getQuestId()) + " first!");
						}
					}
				}
			}
		}
		PacketSendUtility::sendMessage(admin, "Quest not started. Some preconditions failed");
	}
}

// Java Addquest.java:75-78
void Addquest::info(Player& player, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(player, "syntax ///addquest <id quest>");
}

} // namespace aion::gameserver::handlers::consolecommands
