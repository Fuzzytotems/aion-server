#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

/**
 * The AI of Calindi Flamelord ("IDTiamat_2_calindi_flamelord"): at 75, 50 and 25 % hp her hallucinatory victory (20911: the two
 * Surkanas and ten flames) while no Surkana is out, at 12 % 20942; each hit may engrave a blaze (2 %, 20913: a blaze on a random target).
 * <p>
 * Java: data/handlers/ai/instance/dragonLordsRefuge/CalindiFlamelordAI.java, @AIName("IDTiamat_2_calindi_flamelord") (the marker is in the .cpp).
 *
 * @author Cheatkiller, Estrayl
 */
class CalindiFlamelordAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(75, {50, 25, 12}); // Java: = new HpPhases(75, 50, 25, 12)

public:
	explicit CalindiFlamelordAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleBackHome() override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

protected:
	virtual void startHallucinatoryVictoryEvent();

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

protected:
	virtual void blazeEngraving();

	void rndSpawn(int32_t npcId);

	runtime::Ptr<Creature> getRandomTarget();
};

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
