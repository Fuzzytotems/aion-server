#pragma once

namespace aion::gameserver::model {

/**
 * An engine that GameServer initializes at startup (QuestEngine, AIEngine, InstanceEngine, ChatProcessor, ZoneService, GeoService).
 * <p>
 * Written in spine step S0b as the implemented interface of the hub QuestEngine (docs/design/hub-headers.md §3.1, §9.2).
 *
 * @author ATracer
 */
class GameEngine {
public:
	virtual void init() = 0;

	virtual ~GameEngine() = default;

protected:
	GameEngine() = default;
	GameEngine(const GameEngine&) = default;
	GameEngine& operator=(const GameEngine&) = default;
};

} // namespace aion::gameserver::model
