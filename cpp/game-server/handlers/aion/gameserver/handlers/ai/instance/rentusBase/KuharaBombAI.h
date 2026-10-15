#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

/**
 * The AI of Kuhara's bombs in Rentus Base ("kuhara_bomb"): spawned it follows; arrived the first time it hits the living Kuhara with
 * 19659.
 * <p>
 * Java: data/handlers/ai/instance/rentusBase/KuharaBombAI.java, @AIName("kuhara_bomb") (the marker is in the .cpp).
 *
 * @author xTz
 */
class KuharaBombAI : public GeneralNpcAI {
private:
	runtime::AtomicBoolean isDestroyed{AION_LOCK_CLASS(KuharaBombAI::isDestroyed), false}; // Java: = new AtomicBoolean(false)
	runtime::Field<runtime::Ref<Npc>> boss{};

public:
	explicit KuharaBombAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleMoveArrived() override;
};

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
