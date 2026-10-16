#pragma once

#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/handlers/ai/AiPrelude.h"

#include <cstdint>
#include <map>
#include <optional>

#include "aion/commons/logging/Logger.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraFaction.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

/**
 * Ahserion ("ahserion"): max hp and Artillery Blast's damage scale with the team size; his skills speak, call Ereshkigal's voice, his
 * support and the teams' base assault, and Ide Destruction hits the living players within 45 m of the centre; a random target gets
 * 100000 hate after his level-57 and level-26 skills. Killed during the raid, the team of the most damage wins; the fight is logged.
 * <p>
 * Java: data/handlers/ai/worlds/panesterra/ahserionsflight/AhserionAI.java, @AIName("ahserion") (the marker is in the .cpp).
 *
 * @author Yeats, Estrayl
 */
class AhserionAI : public AggressiveNpcAI {
private:
	/** Java: LoggerFactory.getLogger("SIEGE_LOG") */
	static const commons::logging::Logger log;

public:
	explicit AhserionAI(Npc& owner) : AggressiveNpcAI(owner) {}

	void modifyOwnerStat(Stat2& stat) override;

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

	void onStartUseSkill(const skillengine::model::SkillTemplate* st, int32_t lv) override;

	void onEndUseSkill(const skillengine::model::SkillTemplate* st, int32_t lv) override;

	void onEffectApplied(Effect& effect) override;

	void onEffectEnd(runtime::Ptr<Effect> effect) override;

private:
	void handleSupportSpawns();

	void handleBaseAssault();

	/** Retail: activate_skillarea 21575 */
	void handleIdeDestruction();

protected:
	void handleDied() override;

public:
	/** Java: private; C++ public for the tests (a function of the damage map and the service's teams) */
	static std::optional<services::panesterra::ahserion::PanesterraFaction> findWinnerTeam(
		const std::map<services::panesterra::ahserion::PanesterraFaction, int32_t>& panesterraDamage);

private:
	void logMetrics();
};

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
