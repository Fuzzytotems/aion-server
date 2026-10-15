#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai::instance::esoterrace {

/**
 * The AI of the Kexkra prototype in Esoterrace ("kexkraprototype"): at 75 % HP the living players it knows see movie 472, Kexkra himself
 * takes its place and the prototype goes.
 * <p>
 * Java: data/handlers/ai/instance/esoterrace/KexkraPrototypeAI.java, @AIName("kexkraprototype").
 *
 * @author xTz
 */
class KexkraPrototypeAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(75); // Java: = new HpPhases(75)

public:
	explicit KexkraPrototypeAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;
};

} // namespace aion::gameserver::handlers::ai::instance::esoterrace
