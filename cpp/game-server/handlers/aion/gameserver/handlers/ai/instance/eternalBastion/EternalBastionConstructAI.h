#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

/**
 * The AI of Eternal Bastion's constructs ("eternal_bastion_construct"): it never thinks; the castle wall and the inner water gate warn at most
 * every 30 s when attacked; immune to abnormal states, no loot or AP.
 * <p>
 * Java: data/handlers/ai/instance/eternalBastion/EternalBastionConstructAI.java, @AIName("eternal_bastion_construct") (the marker is in the .cpp).
 *
 * @author Tibald, Estrayl
 */
class EternalBastionConstructAI : public NpcAI {
private:
	runtime::Field<int64_t> lastMsgTime{};

public:
	explicit EternalBastionConstructAI(Npc& owner) : NpcAI(owner) {}

	bool canThink() override;

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void broadcastMsg(SM_SYSTEM_MESSAGE msg);

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
