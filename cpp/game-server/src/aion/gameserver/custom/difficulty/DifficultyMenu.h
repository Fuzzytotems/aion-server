#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/portal/fwd.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver::custom::difficulty {

/** One offered menu: who was offered it, its HTML message id, the portal npc and the path the choice enters through */
class DifficultyMenuOffer : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	const int32_t playerObjectId;
	const int32_t messageId;
	const runtime::Ref<model::gameobjects::Npc> portal;
	const model::templates::portal::PortalPath* const portalPath; // static data, immortal

	static runtime::Ref<DifficultyMenuOffer> create(int32_t playerObjectId, int32_t messageId, model::gameobjects::Npc& portal,
		const model::templates::portal::PortalPath* portalPath);

protected:
	DifficultyMenuOffer(int32_t playerObjectId, int32_t messageId, model::gameobjects::Npc& portal,
		const model::templates::portal::PortalPath* portalPath);
	~DifficultyMenuOffer() override;
};

/**
 * The example difficulty menu in front of Fire Temple's entrance (C++-dev only, not in Java; docs/deviations/C2.md, "C++-dev only: the
 * difficulty menu example"). With RuntimeConfig::DIFFICULTY_MENU_ENABLE (gameserver.custom.difficulty_menu.enable, default false):
 * <ol>
 * <li>offer: PortalDialogAI's entry dialog of a Fire Temple portal sends an HTML poll (SM_QUESTIONNAIRE, HTMLService::sendData) whose two
 * reward-item choices stand for Normal and Hard, instead of porting at once;</li>
 * <li>onAnswer: the poll's answer (CM_QUESTIONNAIRE -> HTMLService::getReward with the message id) asks "Enter Fire Temple (Difficulty:
 * Normal/Difficult)?" (SM_QUESTION_WINDOW 902050 through AIActions::addRequest, as Beshmundir's Walk asks);</li>
 * <li>enter: a Yes ports the player through Java's PortalService::port(path, player, npc, difficult) with difficult 0 (Normal) or 1 (Hard): its
 * group rules and InstanceService::getNextAvailableInstance(mapId, difficult, maxPlayers) are Java's.</li>
 * </ol>
 * A new instance created inside enter reads the difficulty with difficultyOfNewInstance (FireTempleInstance's Hard mode). Switch off: offer
 * answers false and Java's path runs unchanged.
 */
class DifficultyMenu {
public:
	static constexpr int32_t FIRE_TEMPLE_MAP_ID = 320100000;
	/** the reward items of the poll that stand for the choices (Fire Temple Tunic: Normal, Fire Temple Hauberk: Hard); nothing is given */
	static constexpr int32_t NORMAL_CHOICE_ITEM_ID = 110101217;
	static constexpr int32_t HARD_CHOICE_ITEM_ID = 110501122;
	static constexpr int8_t NORMAL = 0;
	static constexpr int8_t HARD = 1;

	/** the portal step tests replace: Java's PortalService::port(path, player, npc, difficult) */
	using EnterFunction = void (*)(const model::templates::portal::PortalPath* portalPath, model::gameobjects::player::Player& player,
		model::gameobjects::Npc& npc, int8_t difficult);

	/** @return true if the menu was offered in place of Java's entry (the switch is on and the path leads into Fire Temple) */
	static bool offer(model::gameobjects::player::Player& player, ai::AbstractAI& portalAi, const model::templates::portal::PortalPath* portalPath);

	/** offer once the path's target map is known (PortalService::port's loc): the switch, the map, then the menu */
	static bool offerFor(model::gameobjects::player::Player& player, model::gameobjects::Npc& portal,
		const model::templates::portal::PortalPath* portalPath, int32_t targetMapId);

	/** @return true if messageId is the player's open menu (the answer is handled here; HTMLService's reward path is skipped) */
	static bool onAnswer(model::gameobjects::player::Player& player, int32_t messageId, const std::vector<int32_t>& items);

	/** the Yes of the confirmation: enters through the enter function with the difficulty set for an instance created meanwhile */
	static void enter(model::gameobjects::player::Player& player, DifficultyMenuOffer& offer, int8_t difficulty);

	/** the difficulty of an instance of `mapId` created inside enter on this thread, else NORMAL (Java's entry) */
	static int8_t difficultyOfNewInstance(int32_t mapId);

	/** the poll's HTML */
	static std::string html();

	/** tests: the open offer of a player (null if none) */
	static runtime::Ptr<DifficultyMenuOffer> getOffer(int32_t playerObjectId);

	/** tests: replaces the enter function (nullptr restores PortalService::port) */
	static void setEnterFunctionForTests(EnterFunction function);

private:
	static void portalServiceEnter(const model::templates::portal::PortalPath* portalPath, model::gameobjects::player::Player& player,
		model::gameobjects::Npc& npc, int8_t difficult);

	static inline runtime::ConcurrentHashMap<int32_t, runtime::Ref<DifficultyMenuOffer>> offers{AION_LOCK_CLASS(DifficultyMenu::offers#stripe)};
	static inline runtime::Field<EnterFunction> enterFunction{&DifficultyMenu::portalServiceEnter};
};

} // namespace aion::gameserver::custom::difficulty
