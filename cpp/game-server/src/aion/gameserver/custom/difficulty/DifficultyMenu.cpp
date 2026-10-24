#include "aion/gameserver/custom/difficulty/DifficultyMenu.h"

#include <algorithm>
#include <format>

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/AIRequest.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/configs/main/RuntimeConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PortalLocData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/portal/PortalLoc.h"
#include "aion/gameserver/model/templates/portal/PortalPath.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/services/HTMLService.h"
#include "aion/gameserver/services/teleport/PortalService.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::custom::difficulty {

using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using model::templates::portal::PortalPath;
using network::aion::serverpackets::SM_QUESTION_WINDOW;

DifficultyMenuOffer::DifficultyMenuOffer(int32_t playerObjectIdValue, int32_t messageIdValue, Npc& portalNpc, const PortalPath* path)
	: playerObjectId(playerObjectIdValue), messageId(messageIdValue), portal(portalNpc), portalPath(path) {
}

DifficultyMenuOffer::~DifficultyMenuOffer() = default;

runtime::Ref<DifficultyMenuOffer> DifficultyMenuOffer::create(int32_t playerObjectId, int32_t messageId, Npc& portal, const PortalPath* portalPath) {
	return runtime::makeRef<DifficultyMenuOffer>(playerObjectId, messageId, portal, portalPath);
}

namespace {

/** the Yes/No of the chosen difficulty (the AIRequest of AIActions::addRequest) */
struct DifficultyMenu_AIRequest final : ai::AIRequest {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<DifficultyMenuOffer> offer;
	const int8_t difficulty;

	static runtime::Ref<DifficultyMenu_AIRequest> create(DifficultyMenuOffer& offerValue, int8_t difficultyValue) {
		return runtime::makeRef<DifficultyMenu_AIRequest>(offerValue, difficultyValue);
	}

	void acceptRequest(runtime::Ptr<model::gameobjects::Creature> requester, Player& responder, int32_t requestId) override {
		static_cast<void>(requester);
		static_cast<void>(requestId);
		DifficultyMenu::enter(responder, *offer, difficulty);
	}

protected:
	DifficultyMenu_AIRequest(DifficultyMenuOffer& offerValue, int8_t difficultyValue) : offer(offerValue), difficulty(difficultyValue) {}
	~DifficultyMenu_AIRequest() override = default;
};

/** the difficulty enter() sets for the instance its enter function may create on this thread (confined: one thread, one call) */
struct NewInstanceDifficulty {
	int32_t mapId = 0;
	int8_t difficulty = DifficultyMenu::NORMAL;
};

NewInstanceDifficulty& newInstanceDifficulty() {
	thread_local NewInstanceDifficulty current; // confined: the entering thread's own value
	return current;
}

/** the map the portal path leads to (PortalService::port's loc), 0 without a loc */
int32_t targetMapId(const PortalPath* portalPath) {
	if (portalPath == nullptr)
		return 0;
	const model::templates::portal::PortalLoc* loc = dataholders::DataManager::PORTAL_LOC_DATA->getPortalLoc(portalPath->getLocId());
	return loc == nullptr ? 0 : loc->getWorldId();
}

} // namespace

bool DifficultyMenu::offer(Player& player, ai::AbstractAI& portalAi, const PortalPath* portalPath) {
	if (!configs::main::RuntimeConfig::DIFFICULTY_MENU_ENABLE.load()) // Java's entry, before any lookup
		return false;
	runtime::Ptr<Npc> portal = runtime::as<Npc>(runtime::Ptr<model::gameobjects::Creature>(&portalAi.getOwner()));
	if (portal == nullptr)
		return false;
	return offerFor(player, *portal, portalPath, targetMapId(portalPath));
}

bool DifficultyMenu::offerFor(Player& player, Npc& portal, const PortalPath* portalPath, int32_t mapId) {
	if (!configs::main::RuntimeConfig::DIFFICULTY_MENU_ENABLE.load() || mapId != FIRE_TEMPLE_MAP_ID)
		return false;
	const int32_t messageId = utils::idfactory::IDFactory::getInstance().nextId(); // as HTMLService::showHTML
	offers.put(player.getObjectId(), DifficultyMenuOffer::create(player.getObjectId(), messageId, portal, portalPath));
	services::HTMLService::sendData(player, messageId, html());
	return true;
}

bool DifficultyMenu::onAnswer(Player& player, int32_t messageId, const std::vector<int32_t>& items) {
	runtime::Ptr<DifficultyMenuOffer> open = offers.get(player.getObjectId());
	if (open == nullptr || open->messageId != messageId)
		return false;
	runtime::Ref<DifficultyMenuOffer> answered(open);
	offers.remove(player.getObjectId());
	int8_t difficulty;
	if (std::ranges::find(items, HARD_CHOICE_ITEM_ID) != items.end())
		difficulty = HARD;
	else if (std::ranges::find(items, NORMAL_CHOICE_ITEM_ID) != items.end())
		difficulty = NORMAL;
	else
		return true; // no choice: the menu is closed
	runtime::Ref<DifficultyMenu_AIRequest> request = DifficultyMenu_AIRequest::create(*answered, difficulty);
	// 902050 "Enter %WORLDNAME0 (Difficulty: %1)?"; 902051 Normal, 902052 Difficult (as Beshmundir's Walk asks)
	ai::AIActions::addRequest(answered->portal->getAi(), player, SM_QUESTION_WINDOW::STR_INSTANCE_DUNGEON_WITH_DIFFICULTY_ENTER_CONFIRM, *request,
		{std::to_string(FIRE_TEMPLE_MAP_ID), utils::ChatUtil::l10n(difficulty == HARD ? 902052 : 902051)});
	return true;
}

void DifficultyMenu::enter(Player& player, DifficultyMenuOffer& offer, int8_t difficulty) {
	NewInstanceDifficulty& current = newInstanceDifficulty();
	const NewInstanceDifficulty previous = current;
	current = NewInstanceDifficulty{FIRE_TEMPLE_MAP_ID, difficulty};
	struct Restore {
		NewInstanceDifficulty& current;
		NewInstanceDifficulty previous;
		~Restore() { current = previous; }
	} restore{current, previous};
	enterFunction.get()(offer.portalPath, player, *offer.portal, difficulty);
}

int8_t DifficultyMenu::difficultyOfNewInstance(int32_t mapId) {
	const NewInstanceDifficulty& current = newInstanceDifficulty();
	return current.mapId == mapId ? current.difficulty : NORMAL;
}

std::string DifficultyMenu::html() {
	return std::format(
		"<poll><poll_introduction><![CDATA[<font color=\"4CB1E5\"> Fire Temple </font>]]></poll_introduction>"
		"<poll_title><![CDATA[<font color=\"000000\"> Choose the difficulty </font>]]></poll_title><main_class></main_class><world_id></world_id>"
		"<reward_info><![CDATA[]]></reward_info><reward_items multi_count=\"1\"><item_id count=\"1\">{}</item_id><item_id count=\"1\">{}</item_id>"
		"</reward_items><have_item></have_item><quest></quest><legion></legion><questions><question><title><![CDATA[<BR>"
		"Pick the first item for <font color=\"4CB1E5\">Normal</font>: Fire Temple as you know it.<br>"
		"Pick the second item for <font color=\"FF4040\">Hard</font>: enemies have doubled health and stronger attacks.<br>"
		"Nothing is given: the item only marks your choice.<br>]]></title><select><input type=\"radio\">Normal</input>"
		"<input type=\"radio\">Hard</input></select></question></questions><concluding_remarks><![CDATA[<br>]]></concluding_remarks></poll>",
		NORMAL_CHOICE_ITEM_ID, HARD_CHOICE_ITEM_ID);
}

runtime::Ptr<DifficultyMenuOffer> DifficultyMenu::getOffer(int32_t playerObjectId) {
	return offers.get(playerObjectId);
}

void DifficultyMenu::setEnterFunctionForTests(EnterFunction function) {
	enterFunction.set(function != nullptr ? function : &DifficultyMenu::portalServiceEnter);
}

void DifficultyMenu::portalServiceEnter(const PortalPath* portalPath, Player& player, Npc& npc, int8_t difficult) {
	services::teleport::PortalService::port(portalPath, player, npc, difficult);
}

} // namespace aion::gameserver::custom::difficulty
