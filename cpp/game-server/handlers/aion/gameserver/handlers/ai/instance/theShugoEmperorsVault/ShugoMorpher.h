#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault {

struct ShugoMorpher_ItemUseObserver;

/**
 * The AI of the morph Shugos of The Shugo Emperor's Vault ("emperorsVaultMorphNPC"): ignores attacks and takes no damage; a click (one at a
 * time) starts a 1 s use bar, aborted by any disturbing action; at its end the Shugo casts its morph on the player (21829-21831 for Elyos,
 * 21832-21834 for Asmodians, by the Shugo) and is deleted, or becomes usable again. No loot or rewards.
 * <p>
 * Java: data/handlers/ai/instance/theShugoEmperorsVault/ShugoMorpher.java, @AIName("emperorsVaultMorphNPC") (the marker is in the .cpp).
 * <p>
 * C++: Java's anonymous ItemUseObserver is the callback struct ShugoMorpher_ItemUseObserver of the .cpp, as ActionItemNpcAI's. Java's private
 * handleUseItemFinish is morph() here: a C++ method of that name would not be private to this class.
 *
 * @author Yeats
 */
class ShugoMorpher : public GeneralNpcAI {
	friend struct ShugoMorpher_ItemUseObserver;

private:
	runtime::AtomicBoolean started{AION_LOCK_CLASS(ShugoMorpher::started), false}; // Java: = new AtomicBoolean(false)

public:
	explicit ShugoMorpher(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

	void handleDialogStart(Player& player) override;

private:
	void morph(Player& player);

public:
	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault
