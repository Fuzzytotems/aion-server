#include "aion/gameserver/handlers/ai/worlds/pernon/GaleCycloneAI.h"

#include <vector>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::worlds::pernon {

AION_AI(GaleCycloneAI, "gale_cyclone");

/**
 * Java: GaleCycloneAI.GaleCycloneObserver (GaleCycloneAI.java:55-89), a MOVE observer in the player's ObserveController and in the AI's map
 * until remove() or the player's logout (ObserveController::clearWithoutNotify) takes it out of the first; onRemoved takes it out of the second.
 * It holds the AI (Java's enclosing instance), the player and the cyclone.
 */
struct GaleCycloneAI_GaleCycloneObserver final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<GaleCycloneAI> galeCycloneAI; // the enclosing instance
	const runtime::Ref<Player> player;
	const runtime::Ref<Creature> creature;
	runtime::Field<double> oldRange;

	static runtime::Ref<GaleCycloneAI_GaleCycloneObserver> create(GaleCycloneAI& galeCycloneAI, Player& player, Creature& creature) {
		return runtime::makeRef<GaleCycloneAI_GaleCycloneObserver>(galeCycloneAI, player, creature);
	}

	// Java GaleCycloneAI.java:68-79
	void moved() override {
		double newRange = PositionUtil::getDistance(*player, *creature);
		if (creature->isDead() || creature->getLifeStats()->isAboutToDie() || !creature->getKnownList().sees(*player)) {
			remove();
			return;
		}
		if (oldRange.get() > 12 && newRange <= 12) {
			SkillEngine::getInstance().getSkill(*creature, 20528, 50, runtime::Ptr<VisibleObject>(*player))->useNoAnimationSkill();
		}
		oldRange = newRange;
	}

	// Java GaleCycloneAI.java:81-83
	void remove() { player->getObserveController()->removeObserver(*this); }

	// Java GaleCycloneAI.java:85-88
	void onRemoved() override { galeCycloneAI->observed.remove(player->getObjectId()); }

protected:
	// Java GaleCycloneAI.java:61-66
	GaleCycloneAI_GaleCycloneObserver(GaleCycloneAI& galeCycloneAIValue, Player& playerValue, Creature& creatureValue)
		: ActionObserver(ObserverType::MOVE), galeCycloneAI(runtime::Ref<GaleCycloneAI>(galeCycloneAIValue)), player(runtime::Ref<Player>(playerValue)),
		  creature(runtime::Ref<Creature>(creatureValue)), oldRange(PositionUtil::getDistance(playerValue, creatureValue)) {}
	~GaleCycloneAI_GaleCycloneObserver() override = default;
};

GaleCycloneAI::GaleCycloneAI(Npc& owner) : NpcAI(owner) {}

GaleCycloneAI::~GaleCycloneAI() = default;

// Java GaleCycloneAI.java:28-37
void GaleCycloneAI::handleCreatureSee(Creature& creature) {
	if (runtime::Ptr<Player> player = runtime::as<Player>(creature)) { // Java: instanceof Player player
		observed.computeIfAbsent(player->getObjectId(), [this, &player] {
			runtime::Ref<GaleCycloneAI_GaleCycloneObserver> galeCycloneObserver = GaleCycloneAI_GaleCycloneObserver::create(*this, *player, getOwner());
			player->getObserveController()->addObserver(*galeCycloneObserver);
			return galeCycloneObserver;
		});
	}
}

// Java GaleCycloneAI.java:39-43
void GaleCycloneAI::handleDied() {
	clear();
	NpcAI::handleDied();
}

// Java GaleCycloneAI.java:45-49
void GaleCycloneAI::handleDespawned() {
	clear();
	NpcAI::handleDespawned();
}

// Java GaleCycloneAI.java:51-53 (values() is weakly consistent: the observers' onRemoved take themselves out while it runs)
void GaleCycloneAI::clear() {
	for (const auto& observer : observed.values())
		observer->remove();
}

} // namespace aion::gameserver::handlers::ai::worlds::pernon
