#include "aion/gameserver/model/gameobjects/player/LogoutBreakers.h"

#include <exception>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Kisk.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/IdianStone.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/spawnengine/WalkerGroup.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::model::gameobjects::player {

namespace {

/** C++-only class: no Java logger; the name follows the Java package of the breakers */
const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(
		commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.gameobjects.player.LogoutBreakers"));
	return *logger;
}

/** Runs one breaker step: an exception is logged with the step id and the object, and the next step still runs (noexcept contract) */
template <class F>
void runStep(std::string_view stepId, VisibleObject& object, F&& step) noexcept {
	try {
		step();
	} catch (const runtime::UnportedException&) {
		// the body of a C++-only breaker helper of another chunk is not ported yet: AION_UNPORTED logged its first hit already
	} catch (...) {
		try {
			log().errorCurrentException("Breaker step " + std::string(stepId) + " failed for object " + std::to_string(object.getObjectId()));
		} catch (...) {
			// logging must not throw out of a noexcept breaker
		}
	}
}

/** L3: resets the storage actors; a player without an account warehouse (never entered the world) has no third actor */
void resetStorageActors(Player& player) {
	player.getInventory().setOwner(nullptr);
	player.getWarehouse().setOwner(nullptr);
	try {
		player.getAccount()->getAccountWarehouse().setOwner(nullptr);
	} catch (const runtime::NullPointerException&) {
		// Java null accountWarehouse: nothing to reset
	}
}

} // namespace

void LogoutBreakers::run(Player& player) noexcept {
	// L1 target (breakTarget: no onTargetChanged)
	runStep("L1", player, [&player] { player.breakTarget(); });
	// L2 kisk
	runStep("L2", player, [&player] { player.setKisk(nullptr); });
	// L3 storage actors of the inventory, the regular warehouse and the account warehouse (Java resets them at the end of leaveWorld)
	runStep("L3", player, [&player] { resetStorageActors(player); });
	// L4 stance observer
	runStep("L4", player, [&player] { player.getController().breakStanceObserver(); });
	// L5 ride observers (they are also registered in the ObserveController, which L7 empties)
	runStep("L5", player, [&player] {
		if (runtime::Ptr<runtime::RcArrayList<runtime::Ref<controllers::observer::ActionObserver>>> rideObservers = player.getRideObservers())
			rideObservers->clear();
	});
	// L6 IdianStone action listeners of the equipped items (Java never unequips on logout)
	runStep("L6", player, [&player] {
		for (const runtime::Ptr<Item>& item : player.getEquipment().getEquippedItems()) {
			if (runtime::Ptr<items::IdianStone> idianStone = item->getIdianStone())
				idianStone->breakActionListener();
		}
	});
	// L7 observers and attack-calc observers, last
	runStep("L7", player, [&player] { player.getObserveController()->clearWithoutNotify(); });
}

void LogoutBreakers::onDelete(VisibleObject& object) noexcept {
	runStep("D1", object, [&object] { object.breakTarget(); });
	runtime::Ptr<Creature> creature;
	runStep("D2", object, [&object, &creature] {
		creature = runtime::as<Creature>(runtime::Ptr<VisibleObject>(object));
		if (creature)
			creature->getObserveController()->clearWithoutNotify();
	});
	if (creature) {
		runStep("D3", object, [&creature] {
			if (runtime::Ptr<controllers::effect::EffectController> effectController = creature->getEffectController())
				effectController->clearEffectMapsWithoutNotify();
		});
		runStep("D4", object, [&creature] {
			if (runtime::Ptr<stats::container::CreatureGameStats> gameStats = creature->getGameStats())
				gameStats->clearEffectFunctionsWithoutNotify();
		});
	}
	runStep("D5", object, [&object] {
		if (runtime::Ptr<Npc> npc = runtime::as<Npc>(runtime::Ptr<VisibleObject>(object)))
			npc->setWalkerGroup(nullptr);
	});
	runtime::Ptr<Player> player;
	runStep("D6", object, [&object, &player] { player = runtime::as<Player>(runtime::Ptr<VisibleObject>(object)); });
	if (player)
		run(*player);
}

std::vector<const char*> LogoutBreakers::breakZombieEdges(VisibleObject& object) {
	std::vector<const char*> cut;
	// Helpers of other chunks that are still AION_UNPORTED (KnownList::clearWithoutNotify, ObserveController::hasObservers/clearWithoutNotify,
	// PlayerController::breakStanceObserver) must not lose the edges cut so far: their UnportedException is swallowed here, every other exception
	// propagates to LeakCensus.
	auto unportedSafe = [](auto&& step) {
		try {
			step();
		} catch (const runtime::UnportedException&) {
			// AION_UNPORTED logged its first hit already
		}
	};
	if (object.getTarget()) {
		object.breakTarget();
		cut.push_back("target");
	}
	unportedSafe([&object, &cut] {
		world::knownlist::KnownList* knownList = nullptr;
		try {
			knownList = &object.getKnownList();
		} catch (const runtime::NullPointerException&) {
			return; // no known list: no knownObjects edge
		}
		if (knownList->clearWithoutNotify())
			cut.push_back("knownObjects");
	});
	if (runtime::Ptr<Npc> npc = runtime::as<Npc>(runtime::Ptr<VisibleObject>(object))) {
		if (npc->getWalkerGroup()) {
			npc->setWalkerGroup(nullptr);
			cut.push_back("walkerGroup");
		}
	}
	if (runtime::Ptr<Player> player = runtime::as<Player>(runtime::Ptr<VisibleObject>(object))) {
		if (player->getKisk()) {
			player->setKisk(nullptr);
			cut.push_back("kisk");
		}
		// PlayerStorage.actor: the inventory and the warehouse are parts of the player and act for it (their actor retains nothing), so they are
		// reset without a report; the account warehouse is shared by the account's players, so it is reset and reported only while this player
		// is its actor
		player->getInventory().setOwner(nullptr);
		player->getWarehouse().setOwner(nullptr);
		if (runtime::Ptr<account::Account> account = player->getAccount()) {
			items::storage::Storage* accountWarehouse = nullptr;
			try {
				accountWarehouse = &account->getAccountWarehouse();
			} catch (const runtime::NullPointerException&) {
				// Java null accountWarehouse: no actor edge
			}
			auto* playerStorage = dynamic_cast<items::storage::PlayerStorage*>(accountWarehouse);
			if (playerStorage != nullptr && playerStorage->getActor() == player) {
				playerStorage->setOwner(nullptr);
				cut.push_back("storageActor");
			}
		}
		if (runtime::Ptr<runtime::RcArrayList<runtime::Ref<controllers::observer::ActionObserver>>> rideObservers = player->getRideObservers()) {
			if (!rideObservers->isEmpty()) {
				rideObservers->clear();
				cut.push_back("rideObservers");
			}
		}
		for (const runtime::Ptr<Item>& item : player->getEquipment().getEquippedItems()) {
			if (runtime::Ptr<items::IdianStone> idianStone = item->getIdianStone())
				idianStone->breakActionListener();
		}
		if (player->getSummon()) {
			player->setSummon(nullptr);
			cut.push_back("summon");
		}
		if (player->getPet()) {
			player->setPet(nullptr);
			cut.push_back("pet");
		}
		unportedSafe([&player] { player->getController().breakStanceObserver(); });
	}
	if (runtime::Ptr<Creature> creature = runtime::as<Creature>(runtime::Ptr<VisibleObject>(object))) {
		unportedSafe([&creature, &cut] {
			runtime::Ptr<controllers::ObserveController> observeController = creature->getObserveController();
			if (observeController && observeController->hasObservers()) {
				observeController->clearWithoutNotify();
				cut.push_back("observers"); // one query for both lists: observers and attack-calc observers are reported as "observers"
			}
		});
	}
	return cut;
}

} // namespace aion::gameserver::model::gameobjects::player
