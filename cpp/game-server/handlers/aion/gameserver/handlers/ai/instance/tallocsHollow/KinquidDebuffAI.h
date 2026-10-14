#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::tallocsHollow {

/**
 * The AI of Kinquid's destroyers ("kinquid_debuff"): Kinquid moving within 2 m of one takes its debuff (the armor or the accessory one).
 * <p>
 * Java: data/handlers/ai/instance/tallocsHollow/KinquidDebuffAI.java, @AIName("kinquid_debuff") (the marker is in the .cpp).
 *
 * @author xTz
 */
class KinquidDebuffAI : public AggressiveNpcAI {
public:
	explicit KinquidDebuffAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleCreatureMoved(Creature& creature) override;
};

} // namespace aion::gameserver::handlers::ai::instance::tallocsHollow
