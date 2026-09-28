#include "aion/gameserver/network/aion/clientpackets/CM_TELEPORT_ANIMATION_DONE.h"

#include <exception>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_INFO.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

namespace {

const commons::logging::Logger& log() {
	static const auto logger =
		commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_TELEPORT_ANIMATION_DONE");
	return logger;
}

/** Java `log.error("", e.getCause())`: the stored exception of the failed task, rethrown to reach it as a std::exception */
void logCause(const runtime::ExecutionException& e) {
	if (!e.cause()) {
		log().error("", e);
		return;
	}
	try {
		std::rethrow_exception(e.cause());
	} catch (const std::exception& cause) {
		log().error("", cause);
	} catch (...) {
		log().error("", e);
	}
}

} // namespace

CM_TELEPORT_ANIMATION_DONE::CM_TELEPORT_ANIMATION_DONE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_TELEPORT_ANIMATION_DONE.java:29-31
void CM_TELEPORT_ANIMATION_DONE::readImpl() {
}

// Java CM_TELEPORT_ANIMATION_DONE.java:33-49
void CM_TELEPORT_ANIMATION_DONE::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	runtime::Ptr<runtime::Future> task = player->getController().getAndRemoveTask(model::TaskId::TELEPORT);
	// Java `task instanceof RunnableFuture && !task.isDone()`: every C++ Future can be run on the calling thread (Future::run)
	if (task && !task->isDone()) {
		try {
			task->run(); // run now since it's not started yet
			task->get(); // get to throw exception, if any
		} catch (const runtime::ExecutionException& e) { // Java: InterruptedException | ExecutionException (no C++ interrupts)
			logCause(e);
			if (!player->isSpawned()) {
				utils::PacketSendUtility::sendPacket(*player, serverpackets::SM_PLAYER_INFO(*player));
				world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*player));
			}
		}
	}
}

AION_CLIENT_PACKET(CM_TELEPORT_ANIMATION_DONE);

} // namespace aion::gameserver::network::aion::clientpackets
