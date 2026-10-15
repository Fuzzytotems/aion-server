#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/SummonerAI.h"

namespace aion::gameserver::handlers::ai::instance::theHexway {

/**
 * The AI of Captain Jarka in the Hexway ("captain_jarka"): his death leaves a smoke and takes every npc within 15 m.
 * <p>
 * Java: data/handlers/ai/instance/theHexway/CaptainJarkaAI.java, @AIName("captain_jarka").
 *
 * @author Sykra
 */
class CaptainJarkaAI : public SummonerAI {
public:
	explicit CaptainJarkaAI(Npc& owner) : SummonerAI(owner) {}

protected:
	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::theHexway
