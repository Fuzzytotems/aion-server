#include "aion/gameserver/model/gameobjects/player/Player.h"

#include <memory>
#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/attack/PlayerAggroList.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/model/CreatureType.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/actions/PlayerActions.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/animations/ArrivalAnimationInfo.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Kisk.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank_AbyssRankUpdateTypeInfo.h"
#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/Cooldowns.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerStateInfo.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/InRoll.h"
#include "aion/gameserver/model/gameobjects/player/Macros.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldownList.h"
#include "aion/gameserver/model/gameobjects/player/PrivateStore.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/gameobjects/player/emotion/EmotionList.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/gameobjects/state/FlyStateInfo.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/items/ItemCooldown.h"
#include "aion/gameserver/model/items/storage/LegionStorageProxy.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceGroup.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/templates/flypath/FlightPath.h"
#include "aion/gameserver/model/templates/item/ItemAttackType.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/ItemUseLimits.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplateType.h"
#include "aion/gameserver/model/templates/zone/ZoneType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATS_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/DuelService.h"
#include "aion/gameserver/services/ExchangeService.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/skillengine/condition/ChainCondition.h"
#include "aion/gameserver/skillengine/model/ChainSkills.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/task/AbstractInteractionTask.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::model::gameobjects::player {

namespace {

using items::storage::StorageType;
using state::CreatureState;

/** Java WorldMapType.isPanesterraMap(id): BELUS, TRANSIDIUM_ANNEX, ASPIDA, ATANATOS, DISILLON (the P4-10 companion does not exist yet) */
bool isPanesterraMap(int32_t id) {
	switch (id) {
		case 400020000: // BELUS
		case 400030000: // TRANSIDIUM_ANNEX
		case 400040000: // ASPIDA
		case 400050000: // ATANATOS
		case 400060000: // DISILLON
			return true;
		default:
			return false;
	}
}

/** Java AttackStatus.getBaseStatus(status) (the P5-01 companion does not exist yet) */
controllers::attack::AttackStatus getBaseStatus(controllers::attack::AttackStatus status) {
	using controllers::attack::AttackStatus;
	switch (status) {
		case AttackStatus::DODGE:
		case AttackStatus::CRITICAL_DODGE:
		case AttackStatus::OFFHAND_DODGE:
		case AttackStatus::OFFHAND_CRITICAL_DODGE:
			return AttackStatus::DODGE;
		case AttackStatus::RESIST:
		case AttackStatus::CRITICAL_RESIST:
		case AttackStatus::OFFHAND_RESIST:
		case AttackStatus::OFFHAND_CRITICAL_RESIST:
			return AttackStatus::RESIST;
		case AttackStatus::PARRY:
		case AttackStatus::CRITICAL_PARRY:
		case AttackStatus::OFFHAND_PARRY:
		case AttackStatus::OFFHAND_CRITICAL_PARRY:
			return AttackStatus::PARRY;
		case AttackStatus::BLOCK:
		case AttackStatus::CRITICAL_BLOCK:
		case AttackStatus::OFFHAND_BLOCK:
		case AttackStatus::OFFHAND_CRITICAL_BLOCK:
			return AttackStatus::BLOCK;
		default:
			return status;
	}
}

/**
 * Java String.format(format, name) for the custom name tags of AdminConfig.NAME_TAGS: `%s` takes the name, `%%` and `%n` are literal. Any
 * other conversion, and a second `%s`, throw like Java's IllegalFormatException / MissingFormatArgumentException.
 */
std::string formatNameTag(std::string_view format, std::string_view name) {
	std::string result;
	bool argumentUsed = false;
	for (size_t i = 0; i < format.size(); ++i) {
		if (format[i] != '%') {
			result += format[i];
			continue;
		}
		if (i + 1 >= format.size())
			throw runtime::IllegalArgumentException("UnknownFormatConversionException: Conversion = '%'");
		char conversion = format[++i];
		if (conversion == '%') {
			result += '%';
		} else if (conversion == 'n') {
			result += '\n';
		} else if (conversion == 's' && !argumentUsed) {
			result += name;
			argumentUsed = true;
		} else if (conversion == 's') {
			throw runtime::IllegalArgumentException("MissingFormatArgumentException: Format specifier '%s'");
		} else {
			throw runtime::IllegalArgumentException(std::string("UnknownFormatConversionException: Conversion = '") + conversion + "'");
		}
	}
	return result;
}

} // namespace

Player::Player(CreateKey key, account::PlayerAccountData& playerAccountDataValue, account::Account& account)
	: Creature(key, playerAccountDataValue.getPlayerCommonData()->getPlayerObjId(), std::make_unique<controllers::PlayerController>(), nullptr,
		  playerAccountDataValue.getPlayerCommonData().get(), nullptr, false),
	  playerAccountData(playerAccountDataValue), playerAccount(account),
	  // the pets are loaded in postConstruct (Java order: after the Creature constructor's AI), see PetList::DeferredLoad
	  toyPetList(std::make_unique<PetList>(*this, PetList::DeferredLoad{})), requester(std::make_unique<ResponseRequester>(*this)),
	  equipment(std::make_unique<Equipment>(*this)), inventory(std::make_unique<items::storage::PlayerStorage>(*this, StorageType::CUBE)),
	  regularWarehouse(std::make_unique<items::storage::PlayerStorage>(*this, StorageType::REGULAR_WAREHOUSE)),
	  // Java: petBags[i] = new PlayerStorage(this, StorageType.getStorageTypeById(StorageType.PET_BAG_MIN + i)), PET_BAG_6 .. CASH_PET_BAG_34
	  petBags([this] {
		  std::array<std::unique_ptr<items::storage::Storage>, PET_BAG_COUNT> bags;
		  for (int32_t i = 0; i < PET_BAG_COUNT; i++)
			  bags[static_cast<size_t>(i)] = std::make_unique<items::storage::PlayerStorage>(*this,
				  static_cast<StorageType>(static_cast<int32_t>(StorageType::PET_BAG_6) + i));
		  return bags;
	  }()),
	  // Java: cabinets[i] = new PlayerStorage(this, StorageType.getStorageTypeById(StorageType.HOUSE_WH_MIN + i)), HOUSE_STORAGE_01 .. 20
	  cabinets([this] {
		  std::array<std::unique_ptr<items::storage::Storage>, HOUSE_WH_COUNT> storages;
		  for (int32_t i = 0; i < HOUSE_WH_COUNT; i++)
			  storages[static_cast<size_t>(i)] = std::make_unique<items::storage::PlayerStorage>(*this,
				  static_cast<StorageType>(static_cast<int32_t>(StorageType::HOUSE_STORAGE_01) + i));
		  return storages;
	  }()),
	  portalCooldownList(std::make_unique<PortalCooldownList>(*this)), craftCooldowns(Cooldowns::create()),
	  houseObjectCooldowns(Cooldowns::create()) {
	// Java order: requester, questStateList, titleList, equipment, storages, portal cooldowns, cooldowns (the const parts above), toyPetList
	questStateList.set(QuestStateList::create());
	// Java: new TitleList() without an owner; C++: the part is bound to this player (TitleList(Player&) binds only the part owner, the Java
	// owner field stays null until setTitleList)
	titleList.set(std::make_unique<title::TitleList>(*this));
	// the pets are loaded in postConstruct: Java's PetList constructor runs after the Creature constructor created the AI (hub-headers.md
	// §10.1: loadPets reads the DAO and registers the pets with ExpireTimerTask, which publishes this player)
}

Player::~Player() = default;

void Player::postConstruct() {
	Creature::postConstruct();
	// Java: this.toyPetList = new PetList(this), whose constructor loads the pets, right before getController().setOwner(this)
	toyPetList->loadPets(*this);
	getController().setOwner(*this);
	moveController.set(std::make_unique<controllers::movement::PlayerMoveController>(*this));
	setGameStats(std::make_unique<stats::container::PlayerGameStats>(*this));
	setLifeStats(std::make_unique<stats::container::PlayerLifeStats>(*this));
}

bool Player::isInPlayerMode(actions::PlayerMode mode) {
	return actions::PlayerActions::isInPlayerMode(*this, mode);
}

void Player::setPlayerMode(actions::PlayerMode mode, const std::any& obj) {
	actions::PlayerActions::setPlayerMode(*this, mode, obj);
}

void Player::unsetPlayerMode(actions::PlayerMode mode) {
	actions::PlayerActions::unsetPlayerMode(*this, mode);
}

runtime::Ptr<controllers::movement::PlayerMoveController> Player::getMoveController() const {
	return runtime::cast<controllers::movement::PlayerMoveController>(Creature::getMoveController());
}

std::unique_ptr<controllers::attack::AggroList> Player::createAggroList() {
	return std::make_unique<controllers::attack::PlayerAggroList>(*this);
}

runtime::Ptr<PlayerCommonData> Player::getCommonData() {
	return playerAccountData->getPlayerCommonData();
}

std::string Player::getName() {
	return getName(false);
}

std::string Player::getName(bool displayCustomTag) {
	if (displayCustomTag) {
		std::shared_ptr<const std::vector<std::string>> nameTags = configs::administration::AdminConfig::NAME_TAGS.get();
		if (nameTags && !nameTags->empty()) {
			int32_t index = playerAccount->getAccessLevel() - 1;
			if (index >= 0 && index < static_cast<int32_t>(nameTags->size()))
				return formatNameTag((*nameTags)[static_cast<size_t>(index)], getCommonData()->getName());
		}
	}
	return getCommonData()->getName();
}

runtime::Ptr<PlayerAppearance> Player::getPlayerAppearance() {
	return playerAccountData->getAppearance();
}

void Player::setPlayerAppearance(PlayerAppearance& playerAppearance) {
	playerAccountData->setAppearance(playerAppearance);
}

void Player::setMacros(runtime::Ptr<Macros> value) {
	macros.set(value);
}

void Player::setSkillList(runtime::Ptr<skill::PlayerSkillList> value) {
	skillList.set(value);
}

void Player::setPet(runtime::Ptr<Pet> value) {
	pet.set(value);
}

FriendList& Player::getFriendList() const {
	return *friendList;
}

bool Player::isInAttackMode() {
	return isInState(CreatureState::WEAPON_EQUIPPED);
}

bool Player::isGatherRestricted() {
	return getGatherRestrictionDurationSeconds() > 0;
}

int32_t Player::getGatherRestrictionDurationSeconds() {
	if (gatherRestrictionMillis.get() == 0)
		return 0;
	int32_t durationSeconds = static_cast<int32_t>((gatherRestrictionMillis.get() - commons::utils::currentTimeMillis()) / 1000);
	if (durationSeconds < 0) {
		durationSeconds = 0;
		gatherRestrictionMillis.set(0);
	}
	return durationSeconds;
}

std::optional<std::string> Player::getCaptchaWord() {
	const std::string& word = captchaWord.get();
	if (word.empty())
		return std::nullopt;
	return word;
}

void Player::setCaptchaWord(std::optional<std::string_view> value) {
	captchaWord.set(value ? std::string(*value) : std::string());
}

void Player::setCaptchaImage(runtime::Ptr<runtime::Array<int8_t>> value) {
	captchaImage.set(value);
}

void Player::setFriendList(std::unique_ptr<FriendList> list) {
	friendList.set(std::move(list));
}

void Player::setBlockList(runtime::Ptr<BlockList> list) {
	blockList.set(list);
}

PetList& Player::getPetList() const {
	return *toyPetList;
}

runtime::Ptr<stats::container::PlayerLifeStats> Player::getLifeStats() const {
	return runtime::cast<stats::container::PlayerLifeStats>(Creature::getLifeStats());
}

runtime::Ptr<stats::container::PlayerGameStats> Player::getGameStats() const {
	return runtime::cast<stats::container::PlayerGameStats>(Creature::getGameStats());
}

ResponseRequester& Player::getResponseRequester() const {
	return *requester;
}

bool Player::isOnline() {
	return getClientConnection() != nullptr;
}

int32_t Player::getQuestExpands() {
	return getCommonData()->getQuestExpands();
}

int32_t Player::getNpcExpands() {
	return getCommonData()->getNpcExpands();
}

int32_t Player::getItemExpands() {
	return getCommonData()->getItemExpands();
}

void Player::setCubeLimit() {
	getInventory().setLimit(getLimit(StorageType::CUBE) + (getNpcExpands() + getQuestExpands() + getItemExpands()) * getInventory().getRowLength());
}

PlayerClass Player::getPlayerClass() {
	return getCommonData()->getPlayerClass();
}

Gender Player::getGender() {
	return getCommonData()->getGender();
}

controllers::PlayerController& Player::getController() const {
	return static_cast<controllers::PlayerController&>(Creature::getController());
}

int8_t Player::getLevel() {
	return static_cast<int8_t>(getCommonData()->getLevel());
}

Equipment& Player::getEquipment() const {
	return *equipment;
}

runtime::Ptr<PrivateStore> Player::getStore() const {
	return runtime::Ptr<PrivateStore>(store.get());
}

void Player::setStore(std::unique_ptr<PrivateStore> value) {
	store.set(std::move(value));
}

void Player::setQuestStateList(runtime::Ptr<QuestStateList> value) {
	questStateList.set(value);
}

void Player::setRecipeList(runtime::Ptr<RecipeList> value) {
	recipeList.set(value);
}

runtime::Ptr<items::storage::Storage> Player::getStorage(int32_t storageType) {
	if (storageType == getId(StorageType::CUBE))
		return runtime::Ptr<items::storage::Storage>(*inventory);

	if (storageType == getId(StorageType::REGULAR_WAREHOUSE))
		return runtime::Ptr<items::storage::Storage>(*regularWarehouse);

	if (storageType == getId(StorageType::ACCOUNT_WAREHOUSE))
		return runtime::Ptr<items::storage::Storage>(playerAccount->getAccountWarehouse());

	if (storageType == getId(StorageType::LEGION_WAREHOUSE) && getLegion()) {
		// Java: new LegionStorageProxy(getLegion().getLegionWarehouse(), this) per call; C++ keeps the last proxy in the RECLAIMER slot
		legionStorageProxy.set(std::make_unique<items::storage::LegionStorageProxy>(getLegion()->getLegionWarehouse(), *this));
		return runtime::Ptr<items::storage::Storage>(legionStorageProxy.get());
	}

	if (storageType >= items::storage::PET_BAG_MIN && storageType <= items::storage::PET_BAG_MAX)
		return runtime::Ptr<items::storage::Storage>(*petBags[static_cast<size_t>(storageType - items::storage::PET_BAG_MIN)]);

	if (storageType >= items::storage::HOUSE_WH_MIN && storageType <= items::storage::HOUSE_WH_MAX)
		return runtime::Ptr<items::storage::Storage>(*cabinets[static_cast<size_t>(storageType - items::storage::HOUSE_WH_MIN)]);

	return nullptr;
}

std::vector<runtime::Ptr<items::storage::Storage>> Player::getPetBags() const {
	std::vector<runtime::Ptr<items::storage::Storage>> bags;
	bags.reserve(petBags.size());
	for (const std::unique_ptr<items::storage::Storage>& bag : petBags)
		bags.emplace_back(bag.get());
	return bags;
}

std::vector<runtime::Ptr<items::storage::Storage>> Player::getCabinets() const {
	std::vector<runtime::Ptr<items::storage::Storage>> storages;
	storages.reserve(cabinets.size());
	for (const std::unique_ptr<items::storage::Storage>& cabinet : cabinets)
		storages.emplace_back(cabinet.get());
	return storages;
}

std::vector<runtime::Ptr<Item>> Player::getDirtyItemsToUpdate() {
	std::vector<runtime::Ptr<Item>> dirtyItems;

	for (size_t i = 0; i < xml::EnumTraits<StorageType>::names.size(); ++i) {
		runtime::Ptr<items::storage::Storage> storage = getStorage(getId(static_cast<StorageType>(i)));
		if (storage && storage->getPersistentState() == Persistable::PersistentState::UPDATE_REQUIRED) {
			for (const runtime::Ptr<Item>& item : storage->getItemsWithKinah())
				dirtyItems.push_back(item);
			for (const runtime::Ptr<Item>& item : storage->getDeletedItems().snapshot())
				dirtyItems.push_back(item);
			storage->setPersistentState(Persistable::PersistentState::UPDATED);
		}
	}

	if (equipment->getPersistentState() == Persistable::PersistentState::UPDATE_REQUIRED) {
		for (const runtime::Ptr<Item>& item : equipment->getEquippedItems())
			dirtyItems.push_back(item);
		equipment->setPersistentState(Persistable::PersistentState::UPDATED);
	}

	return dirtyItems;
}

std::vector<runtime::Ptr<Item>> Player::getAllItems() {
	std::vector<runtime::Ptr<Item>> items;
	auto addAll = [&items](const std::vector<runtime::Ptr<Item>>& more) { items.insert(items.end(), more.begin(), more.end()); };
	addAll(inventory->getItemsWithKinah());
	addAll(regularWarehouse->getItemsWithKinah());
	addAll(playerAccount->getAccountWarehouse().getItemsWithKinah());
	for (const std::unique_ptr<items::storage::Storage>& petBag : petBags)
		addAll(petBag->getItemsWithKinah());
	for (const std::unique_ptr<items::storage::Storage>& cabinet : cabinets)
		addAll(cabinet->getItemsWithKinah());
	addAll(getEquipment().getEquippedItems());
	return items;
}

items::storage::Storage& Player::getInventory() const {
	return *inventory;
}

void Player::setPlayerSettings(runtime::Ptr<PlayerSettings> value) {
	playerSettings.set(value);
}

title::TitleList& Player::getTitleList() const {
	return *titleList;
}

void Player::setTitleList(std::unique_ptr<title::TitleList> value) {
	// Java: this.titleList = titleList; titleList.setOwner(this). C++ binds the owner first (a part is bound before it is published).
	if (value == nullptr)
		throw runtime::NullPointerException("titleList");
	value->setOwner(*this);
	titleList.set(std::move(value));
}

void Player::setPlayerGroup(runtime::Ptr<team::group::PlayerGroup> value) {
	playerGroup.set(value);
}

void Player::setAbyssRank(runtime::Ptr<AbyssRank> value) {
	abyssRank.set(value);
}

runtime::Ptr<controllers::effect::PlayerEffectController> Player::getEffectController() const {
	return runtime::cast<controllers::effect::PlayerEffectController>(Creature::getEffectController());
}

bool Player::isLegionMember() {
	return static_cast<bool>(legionMember.get());
}

void Player::setLegionMember(runtime::Ptr<team::legion::LegionMember> value) {
	legionMember.set(value);
}

runtime::Ptr<team::legion::Legion> Player::getLegion() {
	runtime::Ptr<team::legion::LegionMember> member = legionMember.get();
	return member ? member->getLegion() : nullptr;
}

bool Player::hasStore() {
	return static_cast<bool>(getStore());
}

void Player::resetLegionMember() {
	setLegionMember(nullptr);
}

bool Player::isInGroup() {
	return static_cast<bool>(playerGroup.get());
}

std::string Player::getAccountName() {
	return playerAccount->getName();
}

int32_t Player::getWarehouseExpansions() {
	return getCommonData()->getWhNpcExpands() + getCommonData()->getWhBonusExpands();
}

int32_t Player::getWhNpcExpands() {
	return getCommonData()->getWhNpcExpands();
}

int32_t Player::getWhBonusExpands() {
	return getCommonData()->getWhBonusExpands();
}

void Player::setWarehouseLimit() {
	getWarehouse().setLimit(getLimit(StorageType::REGULAR_WAREHOUSE) + (getWarehouseExpansions() * getWarehouse().getRowLength()));
}

items::storage::Storage& Player::getWarehouse() const {
	return *regularWarehouse;
}

void Player::setFlyState(gameobjects::state::FlyState flyStateValue) {
	flyState |= getId(flyStateValue);
}

void Player::unsetFlyState(gameobjects::state::FlyState flyStateValue) {
	flyState &= ~getId(flyStateValue);
}

bool Player::isInFlyState(gameobjects::state::FlyState flyStateValue) {
	return (flyState.get() & getId(flyStateValue)) == getId(flyStateValue);
}

bool Player::isFlying() {
	return flyState.get() != 0;
}

bool Player::isInFlyingState() {
	return isInFlyState(gameobjects::state::FlyState::FLYING);
}

bool Player::isInGlidingState() {
	return isInFlyState(gameobjects::state::FlyState::GLIDING);
}

bool Player::isTrading() {
	return services::ExchangeService::getInstance().isPlayerInExchange(*this);
}

bool Player::isInPrison() {
	return getPrisonDurationSeconds() > 0;
}

int32_t Player::getPrisonDurationSeconds() {
	if (prisonEndTimeMillis.get() == 0)
		return 0;
	int32_t durationSeconds = static_cast<int32_t>((prisonEndTimeMillis.get() - commons::utils::currentTimeMillis()) / 1000);
	if (durationSeconds < 0) {
		durationSeconds = 0;
		prisonEndTimeMillis.set(0);
	}
	return durationSeconds;
}

bool Player::isProtectionActive() {
	return isInVisualState(state::CreatureVisualState::BLINKING);
}

bool Player::isInvulnerable() {
	return isInCustomState(CustomPlayerState::INVULNERABLE);
}

void Player::setMailbox(std::unique_ptr<Mailbox> value) {
	mailbox.set(std::move(value));
}

runtime::Ptr<Mailbox> Player::getMailbox() const {
	return runtime::Ptr<Mailbox>(mailbox.get());
}

controllers::FlyController& Player::getFlyController() const {
	return *flyController;
}

void Player::setFlyController(std::unique_ptr<controllers::FlyController> value) {
	flyController.set(std::move(value));
}

void Player::setInteractionTask(runtime::Ptr<skillengine::task::AbstractInteractionTask> value) {
	interactionTask.set(value);
}

void Player::setFlightTeleportId(int32_t flightTeleportId) {
	setFlightPath(templates::flypath::FlightPath::create(templates::flypath::FlightPath_Type::FLIGHT_TRANSPORTER, flightTeleportId, 0));
}

void Player::setFlightPath(runtime::Ptr<templates::flypath::FlightPath> value) {
	flightPath.set(value);
}

bool Player::isUsingFlightPath(templates::flypath::FlightPath_Type type) {
	runtime::Ptr<templates::flypath::FlightPath> path = flightPath.get();
	return path && path->getType() == type && isInState(CreatureState::FLYING);
}

bool Player::isUsingFlightTransporterOrWindstream() {
	return static_cast<bool>(flightPath.get()) && isInState(CreatureState::FLYING);
}

void Player::setCurrentFlypath(const templates::flypath::FlyPathEntry* path) {
	flyLocationId.set(path);
	if (path != nullptr)
		flyStartTime.set(commons::utils::currentTimeMillis());
	else
		flyStartTime.set(0);
}

bool Player::hasAccess(int8_t accessLevel) {
	return playerAccount->getAccessLevel() >= accessLevel;
}

bool Player::isStaff() {
	return playerAccount->getAccessLevel() > 0;
}

bool Player::isEnemy(Creature& creature) {
	return creature.isEnemyFrom(*this) || isEnemyFrom(creature);
}

bool Player::isEnemyFrom(Npc& enemy) {
	switch (enemy.getType(*this)) {
		case CreatureType::AGGRESSIVE:
		case CreatureType::ATTACKABLE:
			return true;
		default:
			return false;
	}
}

bool Player::isEnemyFrom(Player& enemy) {
	if (equals(enemy))
		return false;
	if (isInCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS) || enemy.isInCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS))
		return !isInFfaTeamMode_.get() || !enemy.isInFfaTeamMode() || !isInSameTeam(enemy);
	return canPvP(enemy) || isDueling(enemy);
}

bool Player::isAggroIconTo(Player& enemy) {
	if (isInCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS) || enemy.isInCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS))
		return !isInFfaTeamMode_.get() || !enemy.isInFfaTeamMode() || !isInSameTeam(enemy);
	return isHostileInPanesterra(enemy) || enemy.getRace() != getRace();
}

bool Player::isHostileInPanesterra(Player& enemy) {
	std::optional<services::panesterra::ahserion::PanesterraFaction> faction = panesterraFaction.get();
	if (faction && isPanesterraMap(getWorldId()))
		return faction != enemy.getPanesterraFaction();
	return false;
}

bool Player::canPvP(Player& enemy) {
	int32_t worldId = enemy.getWorldId();
	if (enemy.getRace() != getRace() || isHostileInPanesterra(enemy)) {
		return isInsidePvPZone() && enemy.isInsidePvPZone();
	} else if (worldId == 110010000 || worldId == 120010000 || isInInstance()) {
		return isInsideZoneType(templates::zone::ZoneType::PVP) && enemy.isInsideZoneType(templates::zone::ZoneType::PVP) && !isInSameTeam(enemy);
	}
	return false;
}

bool Player::isDueling(Creature& creature) {
	runtime::Ptr<Player> master = runtime::as<Player>(creature.getMaster());
	return master && services::DuelService::getInstance().isDueling(*master, *this);
}

bool Player::isInSameTeam(Player& player) {
	int32_t teamId = getCurrentTeamId();
	return teamId != 0 && teamId == player.getCurrentTeamId();
}

bool Player::canSee(runtime::Ptr<VisibleObject> object) {
	if (Creature::canSee(object))
		return true;

	if (runtime::Ptr<Creature> creature = runtime::as<Creature>(object)) {
		if (runtime::Ptr<Player> player = runtime::as<Player>(creature->getMaster())) { // player or a summon's master
			if (isInSameTeam(*player) && !isDueling(*player))
				return true;
		}
		// invisible kisks can be seen from players of the same race
		runtime::Ptr<Kisk> kiskObject = runtime::as<Kisk>(object);
		return kiskObject && kiskObject->getOwnerRace() == getRace();
	}

	return false;
}

std::optional<TribeClass> Player::getTribe() {
	std::optional<TribeClass> transformTribe = getTransformModel().getTribe();
	if (transformTribe)
		return transformTribe;
	return getRace() == Race::ELYOS ? TribeClass::PC : TribeClass::PC_DARK;
}

TribeClass Player::getBaseTribe() {
	std::optional<TribeClass> transformTribe = getTransformModel().getTribe();
	if (transformTribe)
		return dataholders::DataManager::TRIBE_RELATIONS_DATA->getBaseTribe(*transformTribe);
	return getTribe().value(); // Java: getTribe() of a player is never null without a transform tribe
}

void Player::setSummon(runtime::Ptr<Summon> value) {
	summon.set(value);
}

runtime::Ptr<Creature> Player::getSummonOrMercenary(int32_t objectIdValue) {
	runtime::Ptr<Summon> currentSummon = summon.get();
	if (currentSummon && currentSummon->getObjectId() == objectIdValue)
		return currentSummon;
	runtime::Ptr<Npc> npc = runtime::as<Npc>(getKnownList().getObject(objectIdValue));
	if (npc && npc->getCreatorId() == getObjectId() && npc->getNpcTemplateType() == templates::npc::NpcTemplateType::MERCENARY)
		return npc;
	return nullptr;
}

void Player::setKisk(runtime::Ptr<Kisk> newKisk) {
	kisk.set(newKisk);
}

bool Player::hasCooldown(Item& item) {
	const templates::item::ItemUseLimits* limits = item.getItemTemplate()->getUseLimits();
	if (limits == nullptr)
		return false;

	int64_t reuseTime = getItemReuseTime(limits->getDelayId());
	if (reuseTime == 0)
		return false;

	if (reuseTime <= commons::utils::currentTimeMillis()) {
		itemCoolDowns.remove(limits->getDelayId());
		return false;
	}
	return true;
}

void Player::startCooldown(Item& item) {
	const templates::item::ItemUseLimits* limits = item.getItemTemplate()->getUseLimits();
	if (limits == nullptr || limits->getDelayTime() <= 0)
		return;

	addItemCoolDown(limits->getDelayId(), commons::utils::currentTimeMillis() + limits->getDelayTime(), limits->getDelayTime() / 1000);
}

int64_t Player::getItemReuseTime(int32_t delayId) {
	runtime::Ptr<items::ItemCooldown> cd = itemCoolDowns.get(delayId);
	return !cd ? 0 : cd->getReuseTime();
}

void Player::addItemCoolDown(int32_t delayId, int64_t time, int32_t useDelay) {
	itemCoolDowns.put(delayId, items::ItemCooldown::create(time, useDelay));
}

void Player::removeItemCoolDown(int32_t delayId) {
	itemCoolDowns.remove(delayId);
}

runtime::Ptr<team::alliance::PlayerAlliance> Player::getPlayerAlliance() {
	runtime::Ptr<team::alliance::PlayerAllianceGroup> group = playerAllianceGroup.get();
	return group ? group->getAlliance() : nullptr;
}

bool Player::isInAlliance() {
	return static_cast<bool>(playerAllianceGroup.get());
}

void Player::setPlayerAllianceGroup(runtime::Ptr<team::alliance::PlayerAllianceGroup> value) {
	playerAllianceGroup.set(value);
}

bool Player::isInLeague() {
	return isInAlliance() && getPlayerAlliance()->isInLeague();
}

bool Player::isInTeam() {
	return isInGroup() || isInAlliance();
}

runtime::Ptr<team::TemporaryPlayerTeam> Player::getCurrentTeam() {
	return isInGroup() ? runtime::Ptr<team::TemporaryPlayerTeam>(getPlayerGroup()) : runtime::Ptr<team::TemporaryPlayerTeam>(getPlayerAlliance());
}

runtime::Ptr<team::TemporaryPlayerTeam> Player::getCurrentGroup() {
	return isInGroup() ? runtime::Ptr<team::TemporaryPlayerTeam>(getPlayerGroup())
					   : runtime::Ptr<team::TemporaryPlayerTeam>(getPlayerAllianceGroup());
}

int32_t Player::getCurrentTeamId() {
	runtime::Ptr<team::TemporaryPlayerTeam> team = getCurrentTeam();
	return !team ? 0 : team->getTeamId();
}

PortalCooldownList& Player::getPortalCooldownList() const {
	return *portalCooldownList;
}

void Player::setPostman(runtime::Ptr<Npc> value) {
	postman.set(value);
}

std::optional<commons::database::Timestamp> Player::getCreationDate() {
	return playerAccountData->getCreationDate();
}

bool Player::isCompleteQuest(int32_t questId) {
	runtime::Ptr<questEngine::model::QuestState> qs = getQuestStateList()->getQuestState(questId);
	return qs && qs->getStatus() == questEngine::model::QuestStatus::COMPLETE;
}

void Player::setCasting(runtime::Ptr<skillengine::model::Skill> castingSkillValue) {
	runtime::Ptr<skillengine::model::Skill> previousSkill = getCastingSkill();
	Creature::setCasting(castingSkillValue);
	if (previousSkill)
		lastSkill.set(previousSkill->getSkillTemplate());
}

bool Player::isHitTimeBoosted() {
	return isHitTimeBoosted(commons::utils::currentTimeMillis());
}

bool Player::isHitTimeBoosted(int64_t timeMillis) {
	return timeMillis <= hitTimeBoostExpireTimeMillis.get();
}

void Player::setHitTimeBoost(int64_t expireTimeMillis, float castSpeed) {
	hitTimeBoostExpireTimeMillis.set(expireTimeMillis);
	hitTimeBoostCastSpeed.set(castSpeed);
}

runtime::Ptr<skillengine::model::ChainSkills> Player::getChainSkills() {
	// java-race: unsynchronized lazy creation, two threads may create (and one may use) different chain skill holders
	if (!chainSkills)
		chainSkills.set(skillengine::model::ChainSkills::create());
	return chainSkills.get();
}

void Player::setLastCounterSkill(controllers::attack::AttackStatus status) {
	controllers::attack::AttackStatus result = getBaseStatus(status);

	switch (result) {
		case controllers::attack::AttackStatus::DODGE:
		case controllers::attack::AttackStatus::PARRY:
		case controllers::attack::AttackStatus::BLOCK:
		case controllers::attack::AttackStatus::RESIST:
			lastCounterSkill.put(result, commons::utils::currentTimeMillis());
			break;
		default:
			break;
	}
}

int64_t Player::getLastCounterSkill(controllers::attack::AttackStatus status) {
	std::optional<int64_t> time = lastCounterSkill.get(status);
	if (!time)
		return 0;

	return *time;
}

bool Player::isInSiegeWorld() {
	switch (getWorldId()) {
		case 210050000:
		case 220070000:
		case 400010000:
			return true;
		default:
			return false;
	}
}

bool Player::hasPermission(int8_t perm) {
	return playerAccount->getMembership() >= perm;
}

runtime::Ptr<emotion::EmotionList> Player::getEmotions() const {
	return runtime::Ptr<emotion::EmotionList>(emotions.get());
}

void Player::setEmotions(std::unique_ptr<emotion::EmotionList> value) {
	emotions.set(std::move(value));
}

void Player::setBindPoint(runtime::Ptr<BindPointPosition> value) {
	bindPoint.set(value);
}

templates::item::ItemAttackType Player::getAttackType() {
	runtime::Ptr<Item> weapon = getEquipment().getMainHandWeapon();
	if (weapon) // Java returns the template's attack type, which may be null (bad_optional_access here)
		return weapon->getItemTemplate()->getAttackType().value();
	return templates::item::ItemAttackType::PHYSICAL;
}

void Player::resetAbyssRankListUpdated() {
	abyssRankListUpdateMask.set(0);
}

void Player::setAbyssRankListUpdated(AbyssRank_AbyssRankUpdateType type) {
	abyssRankListUpdateMask |= value(type);
}

bool Player::isAbyssRankListUpdated(AbyssRank_AbyssRankUpdateType type) {
	return (abyssRankListUpdateMask.get() & value(type)) == value(type);
}

void Player::addSalvationPoints(int64_t points) {
	getCommonData()->addSalvationPoints(points);
	utils::PacketSendUtility::sendPacket(*this, network::aion::serverpackets::SM_STATS_INFO(*this));
}

bool Player::isPvpTarget(Creature& creature) {
	return static_cast<bool>(runtime::as<Player>(creature.getActingCreature()));
}

bool Player::isTargetingNpcWithFunction(int32_t objectIdValue, int32_t dialogActionId) {
	runtime::Ptr<Npc> npc = runtime::as<Npc>(getTarget());
	return npc && npc->getObjectId() == objectIdValue && npc->getObjectTemplate()->supportsAction(dialogActionId);
}

motion::MotionList& Player::getMotions() const {
	return *motions;
}

void Player::setMotions(std::unique_ptr<motion::MotionList> value) {
	motions.set(std::move(value));
}

npcFaction::NpcFactions& Player::getNpcFactions() const {
	return *npcFactions;
}

void Player::setNpcFactions(std::unique_ptr<npcFaction::NpcFactions> value) {
	npcFactions.set(std::move(value));
}

runtime::Ptr<Item> Player::getSelfRezStone() {
	runtime::Ptr<Item> item = getReviveStone(161001001);
	if (!item)
		item = getReviveStone(161000003);
	if (!item)
		item = getReviveStone(161000004);
	if (!item)
		item = getReviveStone(161000001);
	return item;
}

runtime::Ptr<Item> Player::getReviveStone(int32_t stoneId) {
	runtime::Ptr<Item> item = getInventory().getFirstItemByItemId(stoneId);
	if (item && hasCooldown(*item))
		item = nullptr;
	return item;
}

bool Player::haveSelfRezItem() {
	return static_cast<bool>(getSelfRezStone());
}

void Player::unsetResPosState() {
	if (isInResPostState()) {
		setResPosState(false);
		setResPosX(0);
		setResPosY(0);
		setResPosZ(0);
	}
}

bool Player::isLooting() {
	return lootingNpcOid.get() != 0;
}

Race Player::getRace() {
	return getCommonData()->getRace();
}

Race Player::getOppositeRace() {
	return getRace() == Race::ELYOS ? Race::ASMODIANS : Race::ELYOS;
}

int32_t Player::getSkillCooldown(const skillengine::model::SkillTemplate* template_) {
	return isInCustomState(CustomPlayerState::NO_SKILL_COOLDOWN_MODE) ? 0 : template_->getCooldown();
}

void Player::setLastMessageTime() {
	if ((commons::utils::currentTimeMillis() - lastMsgTime.get()) / 1000 < configs::main::SecurityConfig::FLOOD_DELAY.load())
		floodMsgCount_++;
	else
		floodMsgCount_.set(0);
	lastMsgTime.set(commons::utils::currentTimeMillis());
}

bool Player::canUseRebirthRevive() {
	return rebirthEffect.get() != nullptr || hasAccess(configs::administration::AdminConfig::AUTO_RES.load());
}

void Player::subtractSupplements(int32_t countValue, int32_t supplementId) {
	subtractedSupplementsCount.set(countValue);
	subtractedSupplementId.set(supplementId);
}

void Player::updateSupplements() {
	if (subtractedSupplementId.get() == 0 || subtractedSupplementsCount.get() == 0)
		return;
	getInventory().decreaseByItemId(subtractedSupplementId.get(), subtractedSupplementsCount.get());
	subtractedSupplementsCount.set(0);
	subtractedSupplementId.set(0);
}

void Player::setPortAnimation(animations::ArrivalAnimation portAnimationValue) {
	portAnimation.set(getId(portAnimationValue));
}

bool Player::isSkillDisabled(const skillengine::model::SkillTemplate* template_) {
	const skillengine::condition::ChainCondition* cond = template_->getChainCondition();
	if (cond != nullptr && cond->getAllowedActivations() > 1) { // exception for multicast
		int32_t chainCount = getChainSkills()->getCurrentChainCount(cond->getCategory());
		if (chainCount > 0 && chainCount < cond->getAllowedActivations() && !getChainSkills()->isChainExpired())
			return false;
	}
	if (Creature::isSkillDisabled(template_)) {
		utils::PacketSendUtility::sendPacket(*this, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_SKILL_NOT_READY());
		return true;
	}
	return false;
}

runtime::Ptr<runtime::RcArrayList<runtime::Ref<house::House>>> Player::getHouses() {
	// java-race: unsynchronized lazy load, concurrent callers may load the houses twice
	if (!houses)
		resetHouses();
	return houses.get();
}

void Player::resetHouses() {
	runtime::Ref<runtime::RcArrayList<runtime::Ref<house::House>>> list =
		runtime::RcArrayList<runtime::Ref<house::House>>::create(AION_LOCK_CLASS(Player::houses));
	for (const runtime::Ptr<house::House>& house : services::HousingService::getInstance().findPlayerHouses(getObjectId()))
		list->add(runtime::Ref<house::House>(house));
	houses.set(list);
}

runtime::Ptr<house::House> Player::getActiveHouse() {
	for (const runtime::Ptr<house::House>& house : *getHouses()) {
		if (!house->isInactive())
			return house;
	}
	return nullptr;
}

void Player::setBattleReturnCoords(int32_t mapId, runtime::Ptr<runtime::Array<float>> coords) {
	battleReturnMap.set(mapId);
	battleReturnCoords.set(coords);
}

void Player::addRideObserver(controllers::observer::ActionObserver& observer) {
	// java-race: unsynchronized lazy creation (Java synchronizes only the add)
	if (!rideObservers)
		rideObservers.set(runtime::RcArrayList<runtime::Ref<controllers::observer::ActionObserver>>::create(AION_LOCK_CLASS(Player::rideObservers)));

	runtime::Ptr<runtime::RcArrayList<runtime::Ref<controllers::observer::ActionObserver>>> list = rideObservers.get();
	SYNCHRONIZED(*list) {
		list->add(runtime::Ref<controllers::observer::ActionObserver>(observer));
	}
}

void Player::setPosition(runtime::Ptr<world::WorldPosition> positionValue) {
	Creature::setPosition(positionValue);
	getMoveController()->resetLastPositionFromClient(); // if we don't reset it, material collision handlers (such as shields) affect you on teleport
	getCommonData()->setMapId(positionValue->getMapId());
	getCommonData()->setX(positionValue->getX());
	getCommonData()->setY(positionValue->getY());
	getCommonData()->setZ(positionValue->getZ());
	getCommonData()->setHeading(positionValue->getHeading());
	getCommonData()->setWorldOwnerId(!positionValue->getMapRegion() ? 0 : positionValue->getWorldMapInstance()->getOwnerId());
}

bool Player::isInRobotMode() {
	return robotId.get() != 0;
}

bool Player::canPerformMove() {
	// player cannot move is transformed
	if (getTransformModel().cantMove())
		return false;

	return Creature::canPerformMove();
}

std::string Player::toString() {
	return "Player [id=" + std::to_string(getObjectId()) + ", name=" + getName() + "]";
}

void Player::setCustomState(CustomPlayerState stateValue) {
	customStates |= getMask(stateValue);
}

void Player::unsetCustomState(CustomPlayerState stateValue) {
	customStates &= ~getMask(stateValue);
}

bool Player::isInCustomState(CustomPlayerState stateValue) {
	return (customStates.get() & getMask(stateValue)) == getMask(stateValue);
}

} // namespace aion::gameserver::model::gameobjects::player
