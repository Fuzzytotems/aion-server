#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

/**
 * The AI of Calindi's Surkanas ("calindi_surkana"): from 2.5 s every 5 s it gives the map's Calindi its reflect buff (20590 or
 * 20591) while both live; no decay, respawn or rewards.
 * <p>
 * Java: data/handlers/ai/instance/dragonLordsRefuge/CalindiSurkanaAI.java, @AIName("calindi_surkana") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class CalindiSurkanaAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> reflectTask{};
	runtime::Field<int32_t> calindiId{};

public:
	explicit CalindiSurkanaAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void startReflectBuffTask(int32_t buffId);

protected:
	void handleDied() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
