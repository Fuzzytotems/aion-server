#pragma once

// Prelude of the player chat command handlers (data/handlers/playercommands; the three command packages form one library, aion_gs_handlers_commands).
// A prelude holds the core names of its category as using-declarations in the category namespace, e.g. `using gameserver::ai::NpcAI;`
// (handlers-and-porting-plan.md §1.2). It is identical for every file of the category, so unity batches may include it any number of times.
// Handler files include it first; it also provides the registration markers and AION_UNPORTED. The handler file rules apply
// (aion_gs_regscan): declarations only inside the namespace of this directory, no namespace-scope `static`, no other namespaces.
// Each command package has its own prelude: a using-declaration here and a command class of the same name in this package (Pet, Event,
// SysMail, WorldRaid) are a compile error in every build, instead of lookup that depends on the unity batch. Such a name is not re-exported;
// the commands spell the core type qualified (test_handler_preludes checks the names against the Java classes of the package).
// Using-declarations (S0b): every core type that the Java files of the category import, when a forward header declares it, its simple
// name comes from one package only and no Java type of the category has that name (nested types are spelled Outer::Inner or
// Outer_Inner). The prelude includes the full headers of the category's handler base classes, the generated headers of the
// re-exported enums, the full hub headers the category's handlers use most (Player, Npc, PacketSendUtility, ...; spine freeze) and the forward
// headers of the rest.

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/configs/main/fwd.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/custom/pvpmap/fwd.h"
#include "aion/gameserver/dataholders/fwd.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/gameobjects/state/fwd.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/items/fwd.h"
#include "aion/gameserver/model/items/storage/fwd.h"
#include "aion/gameserver/model/templates/fwd.h"
#include "aion/gameserver/model/templates/item/actions/fwd.h"
#include "aion/gameserver/model/templates/item/enums/EquipType.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/model/templates/item/enums/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/model/templates/itemset/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/network/loginserver/fwd.h"
#include "aion/gameserver/network/loginserver/serverpackets/fwd.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/restrictions/fwd.h"
#include "aion/gameserver/services/item/fwd.h"
#include "aion/gameserver/services/player/fwd.h"
#include "aion/gameserver/services/reward/fwd.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/fwd.h"
#include "aion/gameserver/utils/chathandlers/PlayerCommand.h"
#include "aion/gameserver/utils/chathandlers/fwd.h"
#include "aion/gameserver/utils/fwd.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::handlers::playercommands {

using ::aion::gameserver::configs::main::CustomConfig;
using ::aion::gameserver::configs::main::EventsConfig;
using ::aion::gameserver::controllers::observer::ItemUseObserver;
using ::aion::gameserver::custom::pvpmap::PvpMapService;
using ::aion::gameserver::dataholders::DataManager;
using ::aion::gameserver::model::ChatType;
using ::aion::gameserver::model::EmotionType;
using ::aion::gameserver::model::Race;
using ::aion::gameserver::model::TaskId;
using ::aion::gameserver::model::gameobjects::Creature;
using ::aion::gameserver::model::gameobjects::Gatherable;
using ::aion::gameserver::model::gameobjects::Item;
using ::aion::gameserver::model::gameobjects::Npc;
using ::aion::gameserver::model::gameobjects::VisibleObject;
using ::aion::gameserver::model::gameobjects::player::FriendList;
using ::aion::gameserver::model::gameobjects::player::Player;
using ::aion::gameserver::model::gameobjects::player::PlayerCommonData;
using ::aion::gameserver::model::gameobjects::player::RequestResponseHandler;
using ::aion::gameserver::model::gameobjects::state::CreatureState;
using ::aion::gameserver::model::items::ItemSlot;
using ::aion::gameserver::model::items::storage::Storage;
using ::aion::gameserver::model::templates::QuestTemplate;
using ::aion::gameserver::model::templates::item::ItemTemplate;
using ::aion::gameserver::model::templates::item::actions::AbstractItemAction;
using ::aion::gameserver::model::templates::item::actions::DecomposeAction;
using ::aion::gameserver::model::templates::item::actions::EmotionLearnAction;
using ::aion::gameserver::model::templates::item::actions::ItemActions;
using ::aion::gameserver::model::templates::item::enums::EquipType;
using ::aion::gameserver::model::templates::item::enums::ItemGroup;
using ::aion::gameserver::model::templates::itemset::ItemPart;
using ::aion::gameserver::model::templates::itemset::ItemSetTemplate;
using ::aion::gameserver::network::aion::serverpackets::SM_MESSAGE;
using ::aion::gameserver::network::aion::serverpackets::SM_QUESTION_WINDOW;
using ::aion::gameserver::network::aion::serverpackets::SM_QUEST_ACTION;
using ::aion::gameserver::network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using ::aion::gameserver::network::loginserver::LoginServer;
using ::aion::gameserver::network::loginserver::serverpackets::SM_CHANGE_ALLOWED_HDD_SERIAL;
using ::aion::gameserver::questEngine::model::QuestState;
using ::aion::gameserver::questEngine::model::QuestStatus;
using ::aion::gameserver::restrictions::PlayerRestrictions;
using ::aion::gameserver::services::item::ItemService;
using ::aion::gameserver::services::player::PlayerChatService;
using ::aion::gameserver::services::reward::AdventService;
using ::aion::gameserver::utils::ChatUtil;
using ::aion::gameserver::utils::PacketSendUtility;
using ::aion::gameserver::utils::ThreadPoolManager;
using ::aion::gameserver::utils::audit::GMService;
using ::aion::gameserver::utils::chathandlers::ChatCommand;
using ::aion::gameserver::utils::chathandlers::ChatProcessor;
using ::aion::gameserver::utils::chathandlers::ConsoleCommand;
using ::aion::gameserver::utils::chathandlers::PlayerCommand;
using ::aion::gameserver::world::World;

} // namespace aion::gameserver::handlers::playercommands
