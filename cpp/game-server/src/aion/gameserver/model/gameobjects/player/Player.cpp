#include "aion/gameserver/model/gameobjects/player/Player.h"

#include "aion/gameserver/runtime/base/Unported.h"

// S0b transition (docs/design/hub-headers.md §3.3): the narrowing accessor of the controller needs PlayerController.h (controllers group).
// Remove the guard once it exists (spine freeze).
#if __has_include("aion/gameserver/controllers/PlayerController.h")
#define AION_S0B_PLAYER_CONTROLLER 1
#include "aion/gameserver/controllers/PlayerController.h"
#else
#define AION_S0B_PLAYER_CONTROLLER 0
#endif

// Member and part types (docs/design/hub-headers.md §3.3): constructor, destructor, postConstruct, part accessors, Field<Ref> setters and the
// other narrowing accessors need complete types whose headers are mostly not S0b hubs (the player model of P4-12, storages of P4-13, team and
// legion classes of P5-10/P5-11, controllers and stats of P4-11b/P5-01). Not an S0b transition guard: the chunk that adds the last of them
// removes it.
#if AION_S0B_PLAYER_CONTROLLER && __has_include("aion/gameserver/model/account/Account.h") && \
	__has_include("aion/gameserver/model/account/PlayerAccountData.h") && __has_include("aion/gameserver/model/team/legion/LegionMember.h") && \
	__has_include("aion/gameserver/model/gameobjects/player/Macros.h") && __has_include("aion/gameserver/model/skill/PlayerSkillList.h") && \
	__has_include("aion/gameserver/model/gameobjects/player/FriendList.h") && \
	__has_include("aion/gameserver/model/gameobjects/player/BlockList.h") && \
	__has_include("aion/gameserver/model/gameobjects/player/PetList.h") && __has_include("aion/gameserver/model/gameobjects/player/Mailbox.h") && \
	__has_include("aion/gameserver/model/gameobjects/player/PrivateStore.h") && \
	__has_include("aion/gameserver/model/gameobjects/player/title/TitleList.h") && \
	__has_include("aion/gameserver/model/gameobjects/player/QuestStateList.h") && \
	__has_include("aion/gameserver/model/gameobjects/player/RecipeList.h") && \
	__has_include("aion/gameserver/model/gameobjects/player/ResponseRequester.h") && \
	__has_include("aion/gameserver/model/gameobjects/player/Equipment.h") && \
	__has_include("aion/gameserver/model/items/storage/PlayerStorage.h") && \
	__has_include("aion/gameserver/model/items/storage/LegionStorageProxy.h") && \
	__has_include("aion/gameserver/model/gameobjects/player/PlayerSettings.h") && \
	__has_include("aion/gameserver/model/team/group/PlayerGroup.h") && __has_include("aion/gameserver/model/team/alliance/PlayerAllianceGroup.h") && \
	__has_include("aion/gameserver/model/gameobjects/player/AbyssRank.h") && \
	__has_include("aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h") && \
	__has_include("aion/gameserver/controllers/FlyController.h") && \
	__has_include("aion/gameserver/skillengine/task/AbstractInteractionTask.h") && \
	__has_include("aion/gameserver/model/templates/flypath/FlightPath.h") && \
	__has_include("aion/gameserver/model/gameobjects/Pet.h") && __has_include("aion/gameserver/model/gameobjects/Kisk.h") && \
	__has_include("aion/gameserver/model/gameobjects/player/BindPointPosition.h") && __has_include("aion/gameserver/model/items/ItemCooldown.h") && \
	__has_include("aion/gameserver/model/gameobjects/player/PortalCooldownList.h") && \
	__has_include("aion/gameserver/model/gameobjects/player/Cooldowns.h") && \
	__has_include("aion/gameserver/skillengine/model/ChainSkills.h") && \
	__has_include("aion/gameserver/model/gameobjects/player/emotion/EmotionList.h") && \
	__has_include("aion/gameserver/model/gameobjects/player/motion/MotionList.h") && \
	__has_include("aion/gameserver/controllers/observer/ActionObserver.h") && \
	__has_include("aion/gameserver/model/gameobjects/player/InRoll.h") && \
	__has_include("aion/gameserver/controllers/movement/PlayerMoveController.h") && \
	__has_include("aion/gameserver/model/stats/container/PlayerGameStats.h") && \
	__has_include("aion/gameserver/model/stats/container/PlayerLifeStats.h") && \
	__has_include("aion/gameserver/controllers/effect/PlayerEffectController.h") && \
	__has_include("aion/gameserver/controllers/attack/PlayerAggroList.h") && \
	__has_include("aion/gameserver/model/gameobjects/TransformModel.h") && __has_include("aion/gameserver/world/WorldPosition.h")
#define AION_PLAYER_MEMBER_TYPES 1
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/attack/PlayerAggroList.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/Kisk.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/Cooldowns.h"
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
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/items/ItemCooldown.h"
#include "aion/gameserver/model/items/storage/LegionStorageProxy.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceGroup.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/templates/flypath/FlightPath.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/ChainSkills.h"
#include "aion/gameserver/skillengine/task/AbstractInteractionTask.h"
#else
#define AION_PLAYER_MEMBER_TYPES 0
#endif

namespace aion::gameserver::model::gameobjects::player {

#if AION_PLAYER_MEMBER_TYPES
Player::Player(CreateKey key, account::PlayerAccountData& playerAccountDataValue, account::Account& account)
	: Creature(key, playerAccountDataValue.getPlayerCommonData()->getPlayerObjId(), std::make_unique<controllers::PlayerController>(), nullptr,
		  playerAccountDataValue.getPlayerCommonData().get(), nullptr, false),
	  playerAccountData(playerAccountDataValue), playerAccount(account), toyPetList(std::make_unique<PetList>(*this)),
	  requester(std::make_unique<ResponseRequester>(*this)), equipment(std::make_unique<Equipment>(*this)),
	  inventory(std::make_unique<items::storage::PlayerStorage>(*this, items::storage::StorageType::CUBE)),
	  regularWarehouse(std::make_unique<items::storage::PlayerStorage>(*this, items::storage::StorageType::REGULAR_WAREHOUSE)),
	  // Java: petBags[i] = new PlayerStorage(this, StorageType.getStorageTypeById(StorageType.PET_BAG_MIN + i)), PET_BAG_6 .. CASH_PET_BAG_34
	  petBags([this] {
		  std::array<std::unique_ptr<items::storage::Storage>, PET_BAG_COUNT> bags;
		  for (int32_t i = 0; i < PET_BAG_COUNT; i++)
			  bags[static_cast<size_t>(i)] = std::make_unique<items::storage::PlayerStorage>(*this,
				  static_cast<items::storage::StorageType>(static_cast<int32_t>(items::storage::StorageType::PET_BAG_6) + i));
		  return bags;
	  }()),
	  // Java: cabinets[i] = new PlayerStorage(this, StorageType.getStorageTypeById(StorageType.HOUSE_WH_MIN + i)), HOUSE_STORAGE_01 .. 20
	  cabinets([this] {
		  std::array<std::unique_ptr<items::storage::Storage>, HOUSE_WH_COUNT> storages;
		  for (int32_t i = 0; i < HOUSE_WH_COUNT; i++)
			  storages[static_cast<size_t>(i)] = std::make_unique<items::storage::PlayerStorage>(*this,
				  static_cast<items::storage::StorageType>(static_cast<int32_t>(items::storage::StorageType::HOUSE_STORAGE_01) + i));
		  return storages;
	  }()),
	  portalCooldownList(std::make_unique<PortalCooldownList>(*this)), craftCooldowns(Cooldowns::create()),
	  houseObjectCooldowns(Cooldowns::create()) {
	// Java order: requester, questStateList, titleList, equipment, storages, portal cooldowns, cooldowns, toyPetList (the const parts above)
	questStateList.set(QuestStateList::create());
	// Java: new TitleList() without an owner; C++: the part is bound to this player (TitleList(Player&) binds only the part owner, the Java
	// owner field stays null until setTitleList)
	titleList.set(std::make_unique<title::TitleList>(*this));
}

Player::~Player() = default;

void Player::postConstruct() {
	Creature::postConstruct();
	getController().setOwner(*this);
	moveController.set(std::make_unique<controllers::movement::PlayerMoveController>(*this));
	setGameStats(std::make_unique<stats::container::PlayerGameStats>(*this));
	setLifeStats(std::make_unique<stats::container::PlayerLifeStats>(*this));
}

runtime::Ptr<controllers::movement::PlayerMoveController> Player::getMoveController() const {
	return runtime::cast<controllers::movement::PlayerMoveController>(Creature::getMoveController());
}

std::unique_ptr<controllers::attack::AggroList> Player::createAggroList() {
	return std::make_unique<controllers::attack::PlayerAggroList>(*this);
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

void Player::setCaptchaImage(runtime::Ptr<runtime::Array<int8_t>> value) {
	captchaImage.set(value);
}

FriendList& Player::getFriendList() const {
	return *friendList;
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

items::storage::Storage& Player::getInventory() const {
	return *inventory;
}

void Player::setPlayerSettings(runtime::Ptr<PlayerSettings> value) {
	playerSettings.set(value);
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

void Player::setLegionMember(runtime::Ptr<team::legion::LegionMember> value) {
	legionMember.set(value);
}

items::storage::Storage& Player::getWarehouse() const {
	return *regularWarehouse;
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

void Player::setFlightPath(runtime::Ptr<templates::flypath::FlightPath> value) {
	flightPath.set(value);
}

void Player::setSummon(runtime::Ptr<Summon> value) {
	summon.set(value);
}

void Player::setKisk(runtime::Ptr<Kisk> newKisk) {
	kisk.set(newKisk);
}

void Player::setPlayerAllianceGroup(runtime::Ptr<team::alliance::PlayerAllianceGroup> value) {
	playerAllianceGroup.set(value);
}

PortalCooldownList& Player::getPortalCooldownList() const {
	return *portalCooldownList;
}

void Player::setPostman(runtime::Ptr<Npc> value) {
	postman.set(value);
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
#endif

#if AION_S0B_PLAYER_CONTROLLER
controllers::PlayerController& Player::getController() const {
	return static_cast<controllers::PlayerController&>(Creature::getController());
}
#endif

std::optional<std::string> Player::getCaptchaWord() {
	const std::string& word = captchaWord.get();
	if (word.empty())
		return std::nullopt;
	return word;
}

void Player::setCaptchaWord(std::optional<std::string_view> value) {
	captchaWord.set(value ? std::string(*value) : std::string());
}

bool Player::isInPlayerMode(actions::PlayerMode mode) {
	AION_UNPORTED();
}

void Player::setPlayerMode(actions::PlayerMode mode, const std::any& obj) {
	AION_UNPORTED();
}

void Player::unsetPlayerMode(actions::PlayerMode mode) {
	AION_UNPORTED();
}

runtime::Ptr<PlayerCommonData> Player::getCommonData() {
	AION_UNPORTED();
}

std::string Player::getName() {
	AION_UNPORTED();
}

std::string Player::getName(bool displayCustomTag) {
	AION_UNPORTED();
}

runtime::Ptr<PlayerAppearance> Player::getPlayerAppearance() {
	AION_UNPORTED();
}

void Player::setPlayerAppearance(PlayerAppearance& playerAppearance) {
	AION_UNPORTED();
}

bool Player::isInAttackMode() {
	AION_UNPORTED();
}

bool Player::isGatherRestricted() {
	AION_UNPORTED();
}

int32_t Player::getGatherRestrictionDurationSeconds() {
	AION_UNPORTED();
}

bool Player::isOnline() {
	AION_UNPORTED();
}

int32_t Player::getQuestExpands() {
	AION_UNPORTED();
}

int32_t Player::getNpcExpands() {
	AION_UNPORTED();
}

int32_t Player::getItemExpands() {
	AION_UNPORTED();
}

void Player::setCubeLimit() {
	AION_UNPORTED();
}

PlayerClass Player::getPlayerClass() {
	AION_UNPORTED();
}

Gender Player::getGender() {
	AION_UNPORTED();
}

int8_t Player::getLevel() {
	AION_UNPORTED();
}

runtime::Ptr<items::storage::Storage> Player::getStorage(int32_t storageType) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<Item>> Player::getDirtyItemsToUpdate() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<Item>> Player::getAllItems() {
	AION_UNPORTED();
}

bool Player::isLegionMember() {
	AION_UNPORTED();
}

runtime::Ptr<team::legion::Legion> Player::getLegion() {
	AION_UNPORTED();
}

bool Player::hasStore() {
	AION_UNPORTED();
}

void Player::resetLegionMember() {
	AION_UNPORTED();
}

bool Player::isInGroup() {
	AION_UNPORTED();
}

std::string Player::getAccountName() {
	AION_UNPORTED();
}

int32_t Player::getWarehouseExpansions() {
	AION_UNPORTED();
}

int32_t Player::getWhNpcExpands() {
	AION_UNPORTED();
}

int32_t Player::getWhBonusExpands() {
	AION_UNPORTED();
}

void Player::setWarehouseLimit() {
	AION_UNPORTED();
}

void Player::setFlyState(gameobjects::state::FlyState flyStateValue) {
	AION_UNPORTED();
}

void Player::unsetFlyState(gameobjects::state::FlyState flyStateValue) {
	AION_UNPORTED();
}

bool Player::isInFlyState(gameobjects::state::FlyState flyStateValue) {
	AION_UNPORTED();
}

bool Player::isFlying() {
	AION_UNPORTED();
}

bool Player::isInFlyingState() {
	AION_UNPORTED();
}

bool Player::isInGlidingState() {
	AION_UNPORTED();
}

bool Player::isTrading() {
	AION_UNPORTED();
}

bool Player::isInPrison() {
	AION_UNPORTED();
}

int32_t Player::getPrisonDurationSeconds() {
	AION_UNPORTED();
}

bool Player::isProtectionActive() {
	AION_UNPORTED();
}

bool Player::isInvulnerable() {
	AION_UNPORTED();
}

void Player::setFlightTeleportId(int32_t flightTeleportId) {
	AION_UNPORTED();
}

bool Player::isUsingFlightPath(templates::flypath::FlightPath_Type type) {
	AION_UNPORTED();
}

bool Player::isUsingFlightTransporterOrWindstream() {
	AION_UNPORTED();
}

void Player::setCurrentFlypath(const templates::flypath::FlyPathEntry* path) {
	AION_UNPORTED();
}

bool Player::hasAccess(int8_t accessLevel) {
	AION_UNPORTED();
}

bool Player::isStaff() {
	AION_UNPORTED();
}

bool Player::isEnemy(Creature& creature) {
	AION_UNPORTED();
}

bool Player::isEnemyFrom(Npc& enemy) {
	AION_UNPORTED();
}

bool Player::isEnemyFrom(Player& enemy) {
	AION_UNPORTED();
}

bool Player::isAggroIconTo(Player& enemy) {
	AION_UNPORTED();
}

bool Player::isHostileInPanesterra(Player& enemy) {
	AION_UNPORTED();
}

bool Player::canPvP(Player& enemy) {
	AION_UNPORTED();
}

bool Player::isDueling(Creature& creature) {
	AION_UNPORTED();
}

bool Player::isInSameTeam(Player& player) {
	AION_UNPORTED();
}

bool Player::canSee(runtime::Ptr<VisibleObject> object) {
	AION_UNPORTED();
}

std::optional<TribeClass> Player::getTribe() {
	AION_UNPORTED();
}

TribeClass Player::getBaseTribe() {
	AION_UNPORTED();
}

runtime::Ptr<Creature> Player::getSummonOrMercenary(int32_t objectIdValue) {
	AION_UNPORTED();
}

bool Player::hasCooldown(Item& item) {
	AION_UNPORTED();
}

void Player::startCooldown(Item& item) {
	AION_UNPORTED();
}

int64_t Player::getItemReuseTime(int32_t delayId) {
	AION_UNPORTED();
}

void Player::addItemCoolDown(int32_t delayId, int64_t time, int32_t useDelay) {
	AION_UNPORTED();
}

void Player::removeItemCoolDown(int32_t delayId) {
	AION_UNPORTED();
}

runtime::Ptr<team::alliance::PlayerAlliance> Player::getPlayerAlliance() {
	AION_UNPORTED();
}

bool Player::isInAlliance() {
	AION_UNPORTED();
}

bool Player::isInLeague() {
	AION_UNPORTED();
}

bool Player::isInTeam() {
	AION_UNPORTED();
}

runtime::Ptr<team::TemporaryPlayerTeam> Player::getCurrentTeam() {
	AION_UNPORTED();
}

runtime::Ptr<team::TemporaryPlayerTeam> Player::getCurrentGroup() {
	AION_UNPORTED();
}

int32_t Player::getCurrentTeamId() {
	AION_UNPORTED();
}

std::optional<commons::database::Timestamp> Player::getCreationDate() {
	AION_UNPORTED();
}

bool Player::isCompleteQuest(int32_t questId) {
	AION_UNPORTED();
}

void Player::setCasting(runtime::Ptr<skillengine::model::Skill> castingSkillValue) {
	AION_UNPORTED();
}

bool Player::isHitTimeBoosted() {
	AION_UNPORTED();
}

bool Player::isHitTimeBoosted(int64_t timeMillis) {
	AION_UNPORTED();
}

void Player::setHitTimeBoost(int64_t expireTimeMillis, float castSpeed) {
	AION_UNPORTED();
}

runtime::Ptr<skillengine::model::ChainSkills> Player::getChainSkills() {
	AION_UNPORTED();
}

void Player::setLastCounterSkill(controllers::attack::AttackStatus status) {
	AION_UNPORTED();
}

int64_t Player::getLastCounterSkill(controllers::attack::AttackStatus status) {
	AION_UNPORTED();
}

bool Player::isInSiegeWorld() {
	AION_UNPORTED();
}

bool Player::hasPermission(int8_t perm) {
	AION_UNPORTED();
}

templates::item::ItemAttackType Player::getAttackType() {
	AION_UNPORTED();
}

void Player::resetAbyssRankListUpdated() {
	AION_UNPORTED();
}

void Player::setAbyssRankListUpdated(AbyssRank_AbyssRankUpdateType type) {
	AION_UNPORTED();
}

bool Player::isAbyssRankListUpdated(AbyssRank_AbyssRankUpdateType type) {
	AION_UNPORTED();
}

void Player::addSalvationPoints(int64_t points) {
	AION_UNPORTED();
}

bool Player::isPvpTarget(Creature& creature) {
	AION_UNPORTED();
}

bool Player::isTargetingNpcWithFunction(int32_t objectIdValue, int32_t dialogActionId) {
	AION_UNPORTED();
}

runtime::Ptr<Item> Player::getSelfRezStone() {
	AION_UNPORTED();
}

runtime::Ptr<Item> Player::getReviveStone(int32_t stoneId) {
	AION_UNPORTED();
}

bool Player::haveSelfRezItem() {
	AION_UNPORTED();
}

void Player::unsetResPosState() {
	AION_UNPORTED();
}

bool Player::isLooting() {
	AION_UNPORTED();
}

Race Player::getRace() {
	AION_UNPORTED();
}

Race Player::getOppositeRace() {
	AION_UNPORTED();
}

int32_t Player::getSkillCooldown(const skillengine::model::SkillTemplate* template_) {
	AION_UNPORTED();
}

void Player::setLastMessageTime() {
	AION_UNPORTED();
}

bool Player::canUseRebirthRevive() {
	AION_UNPORTED();
}

void Player::subtractSupplements(int32_t countValue, int32_t supplementId) {
	AION_UNPORTED();
}

void Player::updateSupplements() {
	AION_UNPORTED();
}

void Player::setPortAnimation(animations::ArrivalAnimation portAnimationValue) {
	AION_UNPORTED();
}

bool Player::isSkillDisabled(const skillengine::model::SkillTemplate* template_) {
	AION_UNPORTED();
}

runtime::Ptr<runtime::RcArrayList<runtime::Ref<house::House>>> Player::getHouses() {
	AION_UNPORTED();
}

void Player::resetHouses() {
	AION_UNPORTED();
}

runtime::Ptr<house::House> Player::getActiveHouse() {
	AION_UNPORTED();
}

void Player::setBattleReturnCoords(int32_t mapId, runtime::Ptr<runtime::Array<float>> coords) {
	AION_UNPORTED();
}

// lint: L7 unported stub; Java synchronizes it, the port adds SYNCHRONIZED
void Player::addRideObserver(controllers::observer::ActionObserver& observer) {
	AION_UNPORTED();
}

void Player::setPosition(runtime::Ptr<world::WorldPosition> positionValue) {
	AION_UNPORTED();
}

bool Player::isInRobotMode() {
	AION_UNPORTED();
}

bool Player::canPerformMove() {
	AION_UNPORTED();
}

std::string Player::toString() {
	AION_UNPORTED();
}

void Player::setCustomState(CustomPlayerState stateValue) {
	AION_UNPORTED();
}

void Player::unsetCustomState(CustomPlayerState stateValue) {
	AION_UNPORTED();
}

bool Player::isInCustomState(CustomPlayerState stateValue) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player
