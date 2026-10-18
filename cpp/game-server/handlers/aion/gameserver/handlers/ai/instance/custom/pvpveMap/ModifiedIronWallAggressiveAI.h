#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai::instance::custom::pvpveMap {

/**
 * The AI of the PvPvE map's Iron Wall npcs ("modified_iron_wall_aggressive"): their damage and HP are scaled per npc (the map's random boss
 * hits for at most about 7,700-8,300 and has 882,321 HP); 231304 casts 21165 at 98, 77, 56, 35 and 10 % HP, jumps to a random target within
 * 26 m after it and casts 21171, and away from home it teleports back to its spawn.
 * <p>
 * Java: data/handlers/ai/instance/custom/pvpveMap/ModifiedIronWallAggressiveAI.java, @AIName("modified_iron_wall_aggressive") (the marker is in
 * the .cpp).
 *
 * @author Yeats
 */
class ModifiedIronWallAggressiveAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	/** Java's null for every npc but 231304 */
	const runtime::Ptr<gameserver::ai::HpPhases> hpPhases;

public:
	explicit ModifiedIronWallAggressiveAI(Npc& owner);

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;

	void modifyOwnerStat(Stat2& stat) override;

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

protected:
	void handleBackHome() override;

	void handleNotAtHome() override;

private:
	float getRandomDmg(float damage, float minMultiplier, float maxMultiplier);
};

} // namespace aion::gameserver::handlers::ai::instance::custom::pvpveMap
