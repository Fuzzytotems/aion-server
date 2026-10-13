#include "aion/gameserver/handlers/consolecommands/Deletecquest.h"

#include "aion/gameserver/handlers/admincommands/Quest.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/chathandlers/ChatProcessor.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Deletecquest);

// Java Deletecquest.java:15-19
Deletecquest::Deletecquest() : ConsoleCommand("deletecquest", "Deletes a quest from a player's quest list.",
	"<quest link|ID> - Deletes the quest from your target's quest list (defaults to your character, if no player is targeted).\n") {
}

// Java Deletecquest.java:22-30
void Deletecquest::execute(Player& admin, std::span<const std::string> params) {
	if (params.empty()) {
		sendInfo(admin);
		return;
	}
	runtime::Ptr<Player> target = runtime::as<Player>(admin.getTarget()); // parity= Player player = admin.getTarget() instanceof Player target ? target : admin;
	Player& player = target != nullptr ? *target : admin; // parity: (the same)
	admincommands::Quest* questCommand = ChatProcessor::getInstance().getCommand<admincommands::Quest>(); // parity= Quest questCommand = ChatProcessor.getInstance().getCommand(Quest.class);
	if (questCommand == nullptr) // parity: Java's NullPointerException of the dereference below, explicit
		throw runtime::NullPointerException("ChatProcessor.getCommand(Quest.class)"); // parity: (the same)
	questCommand->deleteQuest(admin, player, ChatUtil::getQuestId(params[0])); // parity= questCommand.deleteQuest(admin, player, ChatUtil.getQuestId(params[0]));
}

} // namespace aion::gameserver::handlers::consolecommands
