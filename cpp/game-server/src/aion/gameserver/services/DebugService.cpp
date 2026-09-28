#include "aion/gameserver/services/DebugService.h"

#include <memory>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/clientpackets/CM_PING.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.DebugService");

// callback at DebugService.java:26 (fieldmap key DebugService@L26:55): scheduleAtFixedRate(this::analyzeWorldPlayers), pin {this} (Immortal)
DebugService::DebugService() {
	utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { analyzeWorldPlayers(); }, ANALYZE_PLAYERS_INTERVAL,
		ANALYZE_PLAYERS_INTERVAL);
	log.info("DebugService started. Analyze interval: " + std::to_string(ANALYZE_PLAYERS_INTERVAL));
}

DebugService::~DebugService() = default;

DebugService& DebugService::getInstance() {
	static DebugService instance; // Java SingletonHolder
	return instance;
}

void DebugService::analyzeWorldPlayers() {
	log.info("Starting analysis of world players");

	for (const runtime::Ptr<model::gameobjects::player::Player>& player : world::World::getInstance().getAllPlayers()) {
		// Check connection
		std::shared_ptr<network::aion::AionConnection> connection = player->getClientConnection();
		if (!connection) {
			log.warn("[DEBUG SERVICE] Found {} without connection: Spawned {}", player->toString(), player->isSpawned());
			continue;
		}

		int64_t lastPing = connection->getLastPingTime();
		if (lastPing > 0) {
			int64_t pingInterval = commons::utils::currentTimeMillis() - lastPing;
			if (pingInterval - 5000 > network::aion::clientpackets::CM_PING::CLIENT_PING_INTERVAL)
				log.warn("[DEBUG SERVICE] Found {} with large ping interval: Spawned {}, PingMS {}", player->toString(), player->isSpawned(), pingInterval);
		}
	}

	log.info("Analysis of world players finished");
}

} // namespace aion::gameserver::services
