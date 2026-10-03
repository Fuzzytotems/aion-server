#pragma once

#include <cstdint>

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai {

struct ActionItemNpcAI_ItemUseObserver;

/**
 * The AI of an npc that is used like an item ("useitem", 489 npc templates: siege weapons, ticket boxes, teleport devices, ...; and, through
 * QuestItemNpcAI, the 610 "quest_use_item" quest objects). A click that DialogService.isInteractionAllowed lets through starts a use bar of the
 * template's talk delay (seconds): the user gets SM_USE_OBJECT and broadcasts the quest-loot emotion, an ItemUseObserver aborts the bar on any
 * disturbing action of the user (a move, a skill, being attacked, ...), and when the delay has passed handleUseItemFinish runs. Without a talk
 * delay the click finishes at once. When the npc dies, every bar still running on it is aborted.
 * <p>
 * Java: data/handlers/ai/ActionItemNpcAI.java, @AIName("useitem") (the marker is in the .cpp).
 * <p>
 * C++: Java's anonymous ItemUseObserver is the callback struct ActionItemNpcAI_ItemUseObserver of the .cpp (fieldmap ai.ActionItemNpcAI$1),
 * which reads the private list and the cancel animation as a friend, as the anonymous class reads its enclosing instance. handleDespawned is a
 * C++-only lifetime breaker (see there).
 *
 * @author xTz, vlog
 */
class ActionItemNpcAI : public NpcAI {
	friend struct ActionItemNpcAI_ItemUseObserver;

public:
	explicit ActionItemNpcAI(Npc& owner) : NpcAI(owner) {}

protected:
	const int32_t startBarAnimation = 1;
	const int32_t cancelBarAnimation = 2;

	void handleDialogStart(Player& player) override;

	virtual void handleUseItemStart(Player& player);

	virtual void handleUseItemFinish(Player& player);

	virtual int32_t getTalkDelayInMs();

	void handleDied() override;

	/**
	 * C++ only (lifetime, not in Java): after NpcAI.handleDespawned, drops the list's references to the bars still running, without aborting
	 * them. An observer holds its AI (and so the npc) and the list holds the observer, so an npc despawned while a bar runs on it, whose user
	 * then logs out, would keep itself alive; in Java the garbage collector frees the pair. A bar still running ends as in Java, by its task or
	 * by its user's abort, and a despawned AI handles no DIED (AIState.DESPAWNED handles only BEFORE_SPAWNED and SPAWNED). The one difference:
	 * an npc brought back into the world as the same object (SpawnEngine.bringIntoWorld: ClusteredNpc, PvpMapHandler) that dies within the talk
	 * delay of a bar started before its despawn - Java's death aborts that bar, here it finishes (P5-05.md, "M5d stage 1").
	 */
	void handleDespawned() override;

private:
	/** Java: private final List<ItemUseObserver> observers = new ArrayList<>(), guarded by synchronized (observers) */
	runtime::ArrayList<runtime::Ref<ItemUseObserver>> observers{AION_LOCK_CLASS(ActionItemNpcAI::observers)};
};

} // namespace aion::gameserver::handlers::ai
