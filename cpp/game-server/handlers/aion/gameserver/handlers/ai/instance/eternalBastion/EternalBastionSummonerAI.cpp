#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionSummonerAI.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

AION_AI(EternalBastionSummonerAI, "eternal_bastion_summoner");

// Java EternalBastionSummonerAI.java:19-25
bool EternalBastionSummonerAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return SummonerAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
