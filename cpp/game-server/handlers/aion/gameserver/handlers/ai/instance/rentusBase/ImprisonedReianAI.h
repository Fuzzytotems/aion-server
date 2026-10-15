#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

#include <optional>
#include <string>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

/**
 * The AI of the imprisoned Reians in Rentus Base ("imprisoned_reian"): spawned it keeps its route aside; a player within 21 m may hear a
 * plea, within 6 m frees it - it walks its route (perhaps with thanks) and goes four steps before the end.
 * <p>
 * Java: data/handlers/ai/instance/rentusBase/ImprisonedReianAI.java, @AIName("imprisoned_reian") (the marker is in the .cpp).
 *
 * @author xTz
 */
class ImprisonedReianAI : public GeneralNpcAI {
private:
	runtime::AtomicBoolean isSaved{AION_LOCK_CLASS(ImprisonedReianAI::isSaved), false}; // Java: = new AtomicBoolean(false)
	runtime::AtomicBoolean isAsked{AION_LOCK_CLASS(ImprisonedReianAI::isAsked), false}; // Java: = new AtomicBoolean(false)
	runtime::Field<std::string> walkerId{}; // Java null: the empty string (a route id is never empty)
	runtime::Field<const model::templates::walker::WalkerTemplate*> template_{}; // Java: template (a C++ keyword)

public:
	explicit ImprisonedReianAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleMoveArrived() override;

	void handleCreatureMoved(Creature& creature) override;
};

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
