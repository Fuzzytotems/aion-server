#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk {

/**
 * The base of the Illuminary Obelisk's four shield generators: a charge of 20 s per Idgel Dome Battery (164000289), three charges at most,
 * the vortex at the first charge, the attack shout at most every 10 s, the charges and the vortex gone with the generator.
 * <p>
 * Java: data/handlers/ai/instance/illuminaryObelisk/ShieldGeneratorAI.java, abstract (no @AIName).
 *
 * @author Estrayl
 */
class ShieldGeneratorAI : public GeneralNpcAI {
	friend struct ShieldGeneratorAI_ItemUseObserver;

private:
	runtime::AtomicBoolean isUnderCharge{AION_LOCK_CLASS(ShieldGeneratorAI::isUnderCharge)}; // Java: = new AtomicBoolean()
	runtime::AtomicBoolean isVortexSpawned{AION_LOCK_CLASS(ShieldGeneratorAI::isVortexSpawned)}; // Java: = new AtomicBoolean()

protected:
	runtime::ArrayList<runtime::Ref<Npc>> charges{AION_LOCK_CLASS(ShieldGeneratorAI::charges)}; // Java: = new ArrayList<>()
	runtime::Field<int32_t> chargeCount{0};

private:
	runtime::Field<int64_t> lastAttackedTime{};

protected:
	virtual network::aion::serverpackets::SM_SYSTEM_MESSAGE getAttackMsg() = 0;

	virtual network::aion::serverpackets::SM_SYSTEM_MESSAGE getChargeMsg() = 0;

	virtual network::aion::serverpackets::SM_SYSTEM_MESSAGE getGateMsg() = 0;

	virtual network::aion::serverpackets::SM_SYSTEM_MESSAGE getDestructionMsg() = 0;

	virtual void handleChargeComplete() = 0;

	virtual void handleVortexSpawn() = 0;

	virtual void handleVortexDespawn() = 0;

public:
	explicit ShieldGeneratorAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

	void shout(network::aion::serverpackets::SM_SYSTEM_MESSAGE msg);

	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	void handleCharging(Player& player);

	void deleteNpcs();

protected:
	void handleDespawned() override;

	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk
