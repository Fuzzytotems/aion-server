#include "aion/gameserver/network/aion/clientpackets/CM_READ_EXPRESS_MAIL.h"

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/LetterType.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/spawnengine/VisibleObjectSpawner.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_READ_EXPRESS_MAIL::CM_READ_EXPRESS_MAIL(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_READ_EXPRESS_MAIL.java:31-34
void CM_READ_EXPRESS_MAIL::readImpl() {
	action = readC();
}

// Java CM_READ_EXPRESS_MAIL.java:36-69. The cooldown lambda (a task, pin {player}) is the player's EXPRESS_MAIL_USE controller task.
void CM_READ_EXPRESS_MAIL::runImpl() {
	using serverpackets::SM_SYSTEM_MESSAGE;
	static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_READ_EXPRESS_MAIL");
	runtime::Ptr<model::gameobjects::player::Player> playerPtr = getConnection()->getActivePlayer();
	model::gameobjects::player::Player& player = *playerPtr;

	switch (action) {
		case 0: // window is closed
			if (player.getPostman() != nullptr) {
				player.getPostman()->getController().delete_();
				player.setPostman(nullptr);
			}
			break;
		case 1: { // click on icon
			const bool haveUnreadExpress = player.getMailbox()->haveUnreadByType(model::gameobjects::LetterType::EXPRESS);
			const bool haveUnreadBlackcloud = player.getMailbox()->haveUnreadByType(model::gameobjects::LetterType::BLACKCLOUD);
			if (player.getPostman() != nullptr) {
				utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_POSTMAN_ALREADY_SUMMONED());
			} else if (player.isFlying()) {
				utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_POSTMAN_UNABLE_IN_FLIGHT());
			} else if (haveUnreadBlackcloud) {
				spawnengine::VisibleObjectSpawner::spawnPostman(player);
			} else if (haveUnreadExpress) {
				if (player.getController().hasTask(model::TaskId::EXPRESS_MAIL_USE)) {
					utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_POSTMAN_UNABLE_IN_COOLTIME());
					return;
				}
				spawnengine::VisibleObjectSpawner::spawnPostman(player);
				player.getController().addTask(model::TaskId::EXPRESS_MAIL_USE,
					utils::ThreadPoolManager::getInstance().schedule(
						{&player}, [&player] { player.getController().cancelTask(model::TaskId::EXPRESS_MAIL_USE); }, 600000)); // 10 min
			}
			break;
		}
		default:
			log.warn(player.toString() + " sent unknown read express mail action type: " + std::to_string(action));
	}
}

AION_CLIENT_PACKET(CM_READ_EXPRESS_MAIL);

} // namespace aion::gameserver::network::aion::clientpackets
