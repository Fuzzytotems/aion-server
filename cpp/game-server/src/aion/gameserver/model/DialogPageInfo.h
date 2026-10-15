#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPage.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model {

/**
 * Companion of the generated enum DialogPage (docs/design/static-data.md §2.5): Java's constructor data and methods as free functions found by
 * ADL (`id(page)` for Java `page.id()`). The methods that read npcs, players and quests are defined in DialogPageInfo.cpp.
 */

namespace detail {
/** Java constructor arguments (dialogActionId, id) in ordinal order; the one-argument constructor passes dialogActionId 0 */
struct DialogPageData {
	int32_t dialogActionId;
	int32_t id;
};

inline constexpr std::array<DialogPageData, 43> DIALOG_PAGE_DATA{{
	{DialogAction::NULL_, 0},                          // NULL
	{DialogAction::OPEN_STIGMA_WINDOW, 1},             // STIGMA
	{DialogAction::CREATE_LEGION, 2},                  // CREATE_LEGION
	{0, 4},                                            // ASK_QUEST_ACCEPT_WINDOW
	{0, 5},                                            // SELECT_QUEST_REWARD_WINDOW1
	{0, 6},                                            // SELECT_QUEST_REWARD_WINDOW2
	{0, 7},                                            // SELECT_QUEST_REWARD_WINDOW3
	{0, 8},                                            // SELECT_QUEST_REWARD_WINDOW4
	{0, 45},                                           // SELECT_QUEST_REWARD_WINDOW5
	{0, 46},                                           // SELECT_QUEST_REWARD_WINDOW6
	{0, 47},                                           // SELECT_QUEST_REWARD_WINDOW7
	{0, 48},                                           // SELECT_QUEST_REWARD_WINDOW8
	{0, 49},                                           // SELECT_QUEST_REWARD_WINDOW9
	{0, 50},                                           // SELECT_QUEST_REWARD_WINDOW10
	{DialogAction::OPEN_VENDOR, 13},                   // VENDOR
	{DialogAction::RETRIEVE_CHAR_WAREHOUSE, 14},       // RETRIEVE_CHAR_WAREHOUSE
	{DialogAction::DEPOSIT_CHAR_WAREHOUSE, 26},        // DEPOSIT_CHAR_WAREHOUSE (open char warehouse)
	{DialogAction::RETRIEVE_ACCOUNT_WAREHOUSE, 16},    // RETRIEVE_ACCOUNT_WAREHOUSE
	{DialogAction::DEPOSIT_ACCOUNT_WAREHOUSE, 17},     // DEPOSIT_ACCOUNT_WAREHOUSE
	{DialogAction::OPEN_POSTBOX, 18},                  // MAIL
	{DialogAction::CHANGE_ITEM_SKIN, 19},              // CHANGE_ITEM_SKIN
	{DialogAction::REMOVE_ITEM_OPTION, 20},            // REMOVE_MANASTONE
	{DialogAction::GIVE_ITEM_PROC, 21},                // GIVE_ITEM_PROC
	{DialogAction::GATHER_SKILL_LEVELUP, 23},          // GATHER_SKILL_LEVELUP
	{0, 24},                                           // LOOT
	{DialogAction::OPEN_LEGION_WAREHOUSE, 25},         // LEGION_WAREHOUSE
	{0, 27},                                           // NO_RIGHT
	{DialogAction::COMBINE_TASK, 28},                  // COMBINETASK_WINDOW
	{DialogAction::COMPOUND_WEAPON, 29},               // COMPOUND_WEAPON
	{DialogAction::DECOMPOUND_WEAPON, 30},             // DECOMPOUND_WEAPON
	{DialogAction::HOUSING_BUILD, 32},                 // HOUSING_MARKER (housing build)
	{DialogAction::HOUSING_DESTRUCT, 33},              // HOUSING_LIFETIME (housing destruct)
	{DialogAction::CHARGE_ITEM_SINGLE, 35},            // CHARGE_ITEM (actually, two choices)
	{DialogAction::CHARGE_ITEM_SINGLE2, 42},           // CHARGE_ITEM2
	{DialogAction::HOUSING_BUDDY_LIST, 36},            // HOUSING_FRIENDLIST
	{0, 37},                                           // HOUSING_POST (unknown)
	{DialogAction::HOUSING_PERSONAL_AUCTION, 38},      // HOUSING_AUCTION
	{DialogAction::HOUSING_PAY_RENT, 39},              // HOUSING_PAY_RENT
	{DialogAction::HOUSING_KICK, 40},                  // HOUSING_KICK
	{DialogAction::HOUSING_CONFIG, 41},                // HOUSING_CONFIG
	{DialogAction::TOWN_CHALLENGE, 43},                // TOWN_CHALLENGE_TASK
	{DialogAction::ITEM_UPGRADE, 52},                  // ITEM_UPGRADE
	{DialogAction::OPEN_STIGMA_ENCHANT, 53},           // OPEN_STIGMA_ENCHANT
}};
static_assert(static_cast<size_t>(DialogPage::OPEN_STIGMA_ENCHANT) + 1 == DIALOG_PAGE_DATA.size(), "one entry per DialogPage constant");
} // namespace detail

/** Java: DialogPage.id() */
constexpr int32_t id(DialogPage page) noexcept {
	return detail::DIALOG_PAGE_DATA[static_cast<size_t>(page)].id;
}

/** Java: DialogPage.getByActionId(int) - the first page with the dialog action id, NULL if there is none (pages without an action have 0) */
constexpr DialogPage getByActionId(int32_t dialogActionId) noexcept {
	for (size_t i = 0; i < detail::DIALOG_PAGE_DATA.size(); ++i) {
		if (detail::DIALOG_PAGE_DATA[i].dialogActionId == dialogActionId)
			return static_cast<DialogPage>(i);
	}
	return DialogPage::NULL_;
}

/** Java: DialogPage.getRewardPageByIndex(Integer) - SELECT_QUEST_REWARD_WINDOW1..10 for the indexes 0..9, NULL otherwise and for null */
constexpr DialogPage getRewardPageByIndex(std::optional<int32_t> rewardIndex) noexcept {
	if (rewardIndex) {
		switch (*rewardIndex) {
			case 0:
				return DialogPage::SELECT_QUEST_REWARD_WINDOW1;
			case 1:
				return DialogPage::SELECT_QUEST_REWARD_WINDOW2;
			case 2:
				return DialogPage::SELECT_QUEST_REWARD_WINDOW3;
			case 3:
				return DialogPage::SELECT_QUEST_REWARD_WINDOW4;
			case 4:
				return DialogPage::SELECT_QUEST_REWARD_WINDOW5;
			case 5:
				return DialogPage::SELECT_QUEST_REWARD_WINDOW6;
			case 6:
				return DialogPage::SELECT_QUEST_REWARD_WINDOW7;
			case 7:
				return DialogPage::SELECT_QUEST_REWARD_WINDOW8;
			case 8:
				return DialogPage::SELECT_QUEST_REWARD_WINDOW9;
			case 9:
				return DialogPage::SELECT_QUEST_REWARD_WINDOW10;
		}
	}
	return DialogPage::NULL_;
}

/** Java: DialogPage.getStartPageId(Npc, Player) - the HTML page id the npc dialog starts with */
int32_t getStartPageId(gameobjects::Npc& npc, gameobjects::player::Player& player);

/** Java: private static DialogPage.hasAlternativeDialogAfterAscension(Npc) (the npc id list of Poeta and Ishalgen) */
bool hasAlternativeDialogAfterAscension(gameobjects::Npc& npc);

/** Java: private static DialogPage.hasQuestInteraction(Player, Npc) */
bool hasQuestInteraction(gameobjects::player::Player& player, gameobjects::Npc& npc);

} // namespace aion::gameserver::model
