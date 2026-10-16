#pragma once

#include <any>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/controllers/effect/fwd.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/controllers/movement/fwd.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/model/account/fwd.h"
#include "aion/gameserver/model/actions/fwd.h"
#include "aion/gameserver/model/animations/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank_AbyssRankUpdateType.h"
#include "aion/gameserver/model/gameobjects/player/emotion/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/gameobjects/player/motion/fwd.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/fwd.h"
#include "aion/gameserver/model/gameobjects/player/title/fwd.h"
#include "aion/gameserver/model/gameobjects/state/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/model/items/fwd.h"
#include "aion/gameserver/model/items/storage/fwd.h"
#include "aion/gameserver/model/skill/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/fwd.h"
#include "aion/gameserver/model/team/group/fwd.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/model/templates/flypath/FlightPath_Type.h"
#include "aion/gameserver/model/templates/flypath/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/model/templates/ride/fwd.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/services/panesterra/ahserion/fwd.h"
#include "aion/gameserver/skillengine/effect/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/skillengine/task/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * This class is representing Player object, it contains all needed data.
 * <p>
 * Hub header (docs/design/hub-headers.md). Java `new Player(accountData, account)` is `VisibleObject::create<Player>(accountData, account)`.
 * The constructor stores the members and creates the parts that only need the owner (requester, equipment, the storages, pet bags and cabinets,
 * portal cooldowns, toy pet list, quest state and title lists, cooldowns); postConstruct() runs the rest of the Java constructor body in Java
 * order: `getController().setOwner(*this)`, the PlayerMoveController, game and life stats (§10.1).
 * Parts (parts.json): the storages, equipment, requester, toy pet list and portal cooldowns are `const std::unique_ptr` (references returned),
 * pet bags and cabinets `std::array`s of parts, the mailbox and the fly controller PartSlots. getMailbox() returns Ptr (Java checks it for null,
 * SystemMailService.java:121). getStorage(int) returns Ptr: null for unknown types, and for LEGION_WAREHOUSE the LegionStorageProxy that Java
 * creates per call, held by the C++-only legionStorageProxy slot (RetireTo::RECLAIMER).
 * Parts created outside the constructor (cycles review, runtime-architecture.md §2.3): the friend list, emotions, motions, NPC factions and title
 * list (loaded by DAOs or CMT_CHARACTER_INFORMATION) and the private store are PartSlots set with `std::unique_ptr`; their getters return
 * references (NullPointerException when empty) except getEmotions() and getStore(), which Java checks for null.
 * Java null for the captcha word and the Panesterra faction: `std::optional` (PunishmentService.java:138, PlayerController.java:392). Arrays
 * that Java stores and returns as they are (captchaImage, battleReturnCoords) are passed and returned as the runtime Array (null allowed).
 *
 * @author -Nemesiss-, SoulKeeper, alexa026, cura
 */
class Player : public Creature {
	AION_MAKE_REF_FRIEND
private:
	/** Java StorageType.PET_BAG_MAX - StorageType.PET_BAG_MIN + 1 (43 - 32 + 1; the StorageType companion is not written yet) */
	static constexpr int32_t PET_BAG_COUNT = 12;
	/** Java StorageType.HOUSE_WH_MAX - StorageType.HOUSE_WH_MIN + 1 (79 - 60 + 1) */
	static constexpr int32_t HOUSE_WH_COUNT = 20;

public:
	runtime::Field<const templates::ride::RideInfo*> ride{};
	runtime::Field<runtime::Ref<InRoll>> inRoll{};

private:
	const runtime::Ref<account::PlayerAccountData> playerAccountData;
	const runtime::Ref<account::Account> playerAccount;
	runtime::Field<runtime::Ref<team::legion::LegionMember>> legionMember{};
	runtime::Field<runtime::Ref<Macros>> macros{};
	runtime::Field<runtime::Ref<skill::PlayerSkillList>> skillList{};
	runtime::PartSlot<FriendList> friendList{*this};
	runtime::Field<runtime::Ref<BlockList>> blockList{};
	const std::unique_ptr<PetList> toyPetList;
	runtime::PartSlot<Mailbox> mailbox{*this};
	/** Replaced on every store opening and set to null on closing: retired to the Reclaimer, not kept with the player */
	runtime::PartSlot<PrivateStore, runtime::RetireTo::RECLAIMER> store{*this};
	runtime::PartSlot<title::TitleList> titleList{*this};
	runtime::Field<runtime::Ref<QuestStateList>> questStateList{};
	runtime::Field<runtime::Ref<RecipeList>> recipeList{};
	runtime::Field<runtime::Ref<runtime::RcArrayList<runtime::Ref<house::House>>>> houses{};
	const std::unique_ptr<ResponseRequester> requester;
	runtime::Field<bool> lookingForGroup{false};
	const std::unique_ptr<Equipment> equipment;
	const std::unique_ptr<items::storage::Storage> inventory;
	const std::unique_ptr<items::storage::Storage> regularWarehouse;
	const std::array<std::unique_ptr<items::storage::Storage>, PET_BAG_COUNT> petBags;
	const std::array<std::unique_ptr<items::storage::Storage>, HOUSE_WH_COUNT> cabinets;
	runtime::Field<runtime::Ref<PlayerSettings>> playerSettings{};
	runtime::Field<runtime::Ref<team::group::PlayerGroup>> playerGroup{};
	runtime::Field<runtime::Ref<team::alliance::PlayerAllianceGroup>> playerAllianceGroup{};
	runtime::Field<runtime::Ref<AbyssRank>> abyssRank{};
	runtime::PartSlot<npcFaction::NpcFactions> npcFactions{*this};
	runtime::Field<int32_t> flyState{0};
	runtime::PartSlot<controllers::FlyController> flyController{*this};
	runtime::Field<runtime::Ref<skillengine::task::AbstractInteractionTask>> interactionTask{};
	runtime::Field<runtime::Ref<templates::flypath::FlightPath>> flightPath{};
	runtime::Field<runtime::Ref<Summon>> summon{};
	runtime::Field<runtime::Ref<Pet>> pet{};
	runtime::Field<runtime::Ref<Kisk>> kisk{};
	runtime::Field<bool> isResByPlayer{false};
	runtime::Field<int32_t> resurrectionSkill{0};
	runtime::Field<bool> isFlyingBeforeDeath{false};
	runtime::Field<runtime::Ref<Npc>> postman{};
	runtime::Field<bool> isInResurrectPosState{false};
	runtime::Field<float> resPosX{0};
	runtime::Field<float> resPosY{0};
	runtime::Field<float> resPosZ{0};
	runtime::Field<int32_t> abyssRankListUpdateMask{0};
	runtime::Field<runtime::Ref<BindPointPosition>> bindPoint{};
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<items::ItemCooldown>> itemCoolDowns{AION_LOCK_CLASS(Player::itemCoolDowns#stripe)};
	const std::unique_ptr<PortalCooldownList> portalCooldownList;
	const runtime::Ref<Cooldowns> craftCooldowns;
	const runtime::Ref<Cooldowns> houseObjectCooldowns;
	runtime::Field<int64_t> nextSkillUse{};
	runtime::Field<const skillengine::model::SkillTemplate*> lastSkill{};
	runtime::Field<int64_t> hitTimeBoostExpireTimeMillis{};
	runtime::Field<float> hitTimeBoostCastSpeed{};
	runtime::Field<runtime::Ref<skillengine::model::ChainSkills>> chainSkills{};
	runtime::HashMap<controllers::attack::AttackStatus, int64_t> lastCounterSkill{AION_LOCK_CLASS(Player::lastCounterSkill)};
	runtime::Field<int64_t> prisonEndTimeMillis{0};
	runtime::Field<int64_t> gatherRestrictionMillis{};
	runtime::Field<std::string> captchaWord{};
	runtime::Field<runtime::Ref<runtime::Array<int8_t>>> captchaImage{};
	/** Connection of this Player. */
	runtime::Field<std::shared_ptr<network::aion::AionConnection>> clientConnection{};
	runtime::Field<const templates::flypath::FlyPathEntry*> flyLocationId{};
	runtime::Field<int64_t> flyStartTime{};
	runtime::PartSlot<emotion::EmotionList> emotions{*this};
	runtime::PartSlot<motion::MotionList> motions{*this};
	runtime::Field<int64_t> flyReuseTime{};
	runtime::Field<bool> isMentor_{};
	runtime::Field<int64_t> lastMsgTime{0};
	runtime::Field<int32_t> floodMsgCount_{0};
	runtime::Field<int32_t> lootingNpcOid{};
	runtime::Field<const skillengine::effect::RebirthEffect*> rebirthEffect{};
	// Needed to remove supplements queue
	runtime::Field<int32_t> subtractedSupplementsCount{};
	runtime::Field<int32_t> subtractedSupplementId{};
	runtime::Field<int8_t> portAnimation{};
	runtime::Field<bool> isInSprintMode_{};
	runtime::Field<runtime::Ref<runtime::RcArrayList<runtime::Ref<controllers::observer::ActionObserver>>>> rideObservers{};
	runtime::Field<int32_t> battleReturnMap{};
	runtime::Field<runtime::Ref<runtime::Array<float>>> battleReturnCoords{};
	runtime::Field<int32_t> robotId{};
	runtime::Field<bool> isInFfaTeamMode_{};
	runtime::Field<int32_t> customStates{};
	runtime::Field<std::optional<services::panesterra::ahserion::PanesterraFaction>> panesterraFaction{};
	/**
	 * C++ only (Java: getStorage(LEGION_WAREHOUSE) returns `new LegionStorageProxy(legionWarehouse, this)` kept alive by the GC): the proxy of the
	 * last getStorage(LEGION_WAREHOUSE) call. Each call stores a new proxy, the previous one is retired to the Reclaimer, so a proxy returned
	 * to a task stays valid until the task ends (and while a Ref holds it).
	 */
	runtime::PartSlot<items::storage::LegionStorageProxy, runtime::RetireTo::RECLAIMER> legionStorageProxy{*this};

public:
	runtime::Field<int32_t> speedHackCounter{};
	runtime::Field<int32_t> abnormalHackCounter{};

protected:
	Player(CreateKey key, account::PlayerAccountData& playerAccountData, account::Account& account);
	~Player() override;

	/** Java constructor body after the parts: getController().setOwner(this), moveController = new PlayerMoveController(this), game/life stats */
	void postConstruct() override;

public:
	bool isInPlayerMode(actions::PlayerMode mode);

	void setPlayerMode(actions::PlayerMode mode, const std::any& obj);

	void unsetPlayerMode(actions::PlayerMode mode);

	/** Narrows Creature::getMoveController (Java cast-only override) */
	runtime::Ptr<controllers::movement::PlayerMoveController> getMoveController() const;

protected:
	std::unique_ptr<controllers::attack::AggroList> createAggroList() override final;

public:
	runtime::Ptr<PlayerCommonData> getCommonData();

	std::string getName() override final;

	std::string getName(bool displayCustomTag);

	runtime::Ptr<PlayerAppearance> getPlayerAppearance();

	void setPlayerAppearance(PlayerAppearance& playerAppearance);

	/** Set connection of this player (null when the player leaves the world). */
	void setClientConnection(std::shared_ptr<network::aion::AionConnection> value) { clientConnection.set(std::move(value)); }

	/** Get connection of this player. @return AionConnection of this player. */
	std::shared_ptr<network::aion::AionConnection> getClientConnection() const { return clientConnection.get(); }

	runtime::Ptr<Macros> getMacros() const { return macros.get(); }

	void setMacros(runtime::Ptr<Macros> macros);

	runtime::Ptr<skill::PlayerSkillList> getSkillList() const { return skillList.get(); }

	void setSkillList(runtime::Ptr<skill::PlayerSkillList> skillList);

	runtime::Ptr<Pet> getPet() const { return pet.get(); }

	void setPet(runtime::Ptr<Pet> pet);

	/** Gets this players Friend List. @throws NullPointerException before setFriendList (Java: null) */
	FriendList& getFriendList() const;

	/** Is this player looking for a group */
	bool isLookingForGroup() const { return lookingForGroup.get(); }

	/** Sets whether this player is looking for a group */
	void setLookingForGroup(bool value) { lookingForGroup.set(value); }

	bool isInAttackMode();

	bool isGatherRestricted();

	void setGatherRestrictionExpirationTime(int64_t millis) { gatherRestrictionMillis.set(millis); }

	int32_t getGatherRestrictionDurationSeconds();

	std::optional<std::string> getCaptchaWord();

	void setCaptchaWord(std::optional<std::string_view> captchaWord);

	runtime::Ptr<runtime::Array<int8_t>> getCaptchaImage() const { return captchaImage.get(); }

	void setCaptchaImage(runtime::Ptr<runtime::Array<int8_t>> captchaImage);

	/**
	 * Sets this players friend list. <br />
	 * Remember to send the player the <tt>SM_FRIEND_LIST</tt> packet.
	 */
	void setFriendList(std::unique_ptr<FriendList> list);

	runtime::Ptr<BlockList> getBlockList() const { return blockList.get(); }

	void setBlockList(runtime::Ptr<BlockList> list);

	/** Java final */
	PetList& getPetList() const;

	/** Narrows Creature::getLifeStats (Java cast-only override) */
	runtime::Ptr<stats::container::PlayerLifeStats> getLifeStats() const;

	/** Narrows Creature::getGameStats (Java cast-only override) */
	runtime::Ptr<stats::container::PlayerGameStats> getGameStats() const;

	/** Gets the ResponseRequester for this player */
	ResponseRequester& getResponseRequester() const;

	bool isOnline();

	int32_t getQuestExpands();

	int32_t getNpcExpands();

	int32_t getItemExpands();

	void setCubeLimit();

	PlayerClass getPlayerClass();

	Gender getGender();

	/** Return PlayerController of this Player Object. Narrows Creature::getController (Java cast-only override) */
	controllers::PlayerController& getController() const;

	int8_t getLevel() override;

	Equipment& getEquipment() const;

	/** @return the player private store, null if none is open (Java checks it for null) */
	runtime::Ptr<PrivateStore> getStore() const;

	/** @param store the store that needs to be set (nullptr closes it) */
	void setStore(std::unique_ptr<PrivateStore> store);

	/** @return the questStatesList */
	runtime::Ptr<QuestStateList> getQuestStateList() const { return questStateList.get(); }

	/** @param questStateList the QuestStateList to set */
	void setQuestStateList(runtime::Ptr<QuestStateList> questStateList);

	runtime::Ptr<RecipeList> getRecipeList() const { return recipeList.get(); }

	void setRecipeList(runtime::Ptr<RecipeList> recipeList);

	/**
	 * @return the storage of the given StorageType id, null if there is none. LEGION_WAREHOUSE: a new LegionStorageProxy per call, stored into
	 *         the C++-only legionStorageProxy slot (RECLAIMER), so the returned Ptr stays valid until the task ends
	 */
	runtime::Ptr<items::storage::Storage> getStorage(int32_t storageType);

	std::vector<runtime::Ptr<items::storage::Storage>> getPetBags() const;

	std::vector<runtime::Ptr<items::storage::Storage>> getCabinets() const;

	/** Items from UPDATE_REQUIRED storages and equipment */
	std::vector<runtime::Ptr<Item>> getDirtyItemsToUpdate();

	std::vector<runtime::Ptr<Item>> getAllItems();

	items::storage::Storage& getInventory() const;

	/** @return the playerSettings */
	runtime::Ptr<PlayerSettings> getPlayerSettings() const { return playerSettings.get(); }

	/** @param playerSettings the playerSettings to set */
	void setPlayerSettings(runtime::Ptr<PlayerSettings> playerSettings);

	title::TitleList& getTitleList() const;

	/** Stores the list and binds its owner (Java: titleList.setOwner(this)) */
	void setTitleList(std::unique_ptr<title::TitleList> titleList);

	runtime::Ptr<team::group::PlayerGroup> getPlayerGroup() const { return playerGroup.get(); }

	void setPlayerGroup(runtime::Ptr<team::group::PlayerGroup> playerGroup);

	/** @return the abyssRank */
	runtime::Ptr<AbyssRank> getAbyssRank() const { return abyssRank.get(); }

	/** @param abyssRank the abyssRank to set */
	void setAbyssRank(runtime::Ptr<AbyssRank> abyssRank);

	/** Narrows Creature::getEffectController (Java cast-only override) */
	runtime::Ptr<controllers::effect::PlayerEffectController> getEffectController() const;

	/** Returns true if has valid LegionMember */
	bool isLegionMember();

	/** @param legionMember the legionMember to set */
	void setLegionMember(runtime::Ptr<team::legion::LegionMember> legionMember);

	/** @return the legionMember */
	runtime::Ptr<team::legion::LegionMember> getLegionMember() const { return legionMember.get(); }

	/** @return the legion */
	runtime::Ptr<team::legion::Legion> getLegion();

	/** @return true if a player has a store opened */
	bool hasStore();

	/** Removes legion from player */
	void resetLegionMember();

	bool isInGroup();

	/** @return The account name of this player. */
	std::string getAccountName();

	int32_t getWarehouseExpansions();

	int32_t getWhNpcExpands();

	int32_t getWhBonusExpands();

	void setWarehouseLimit();

	/** @return regularWarehouse */
	items::storage::Storage& getWarehouse() const;

	/** 0: regular, 1: fly, 2: glide its bitset */
	int32_t getFlyState() const { return flyState.get(); }

	void setFlyState(gameobjects::state::FlyState flyState);

	void unsetFlyState(gameobjects::state::FlyState flyState);

	bool isInFlyState(gameobjects::state::FlyState flyState);

	/** CreatureState is unreliable for players returns true if player is flying or gliding */
	bool isFlying() override;

	/** CreatureState is unreliable for players returns true if player is flying */
	bool isInFlyingState() override;

	bool isInGlidingState();

	bool isTrading();

	bool isInPrison();

	void setPrisonEndTimeMillis(int64_t value) { prisonEndTimeMillis.set(value); }

	int32_t getPrisonDurationSeconds();

	bool isProtectionActive();

	bool isInvulnerable() override;

	void setMailbox(std::unique_ptr<Mailbox> mailbox);

	/** @return the mailbox, null until it is loaded (Java checks it for null) */
	runtime::Ptr<Mailbox> getMailbox() const;

	/** @return the flyController */
	controllers::FlyController& getFlyController() const;

	/** @param flyController the flyController to set */
	void setFlyController(std::unique_ptr<controllers::FlyController> flyController);

	void setInteractionTask(runtime::Ptr<skillengine::task::AbstractInteractionTask> interactionTask);

	/** @return The gathering or crafting task the player is currently busy with, null if there is none. */
	runtime::Ptr<skillengine::task::AbstractInteractionTask> getInteractionTask() const { return interactionTask.get(); }

	void setFlightTeleportId(int32_t flightTeleportId);

	void setFlightPath(runtime::Ptr<templates::flypath::FlightPath> flightPath);

	bool isUsingFlightPath(templates::flypath::FlightPath_Type type);

	bool isUsingFlightTransporterOrWindstream();

	runtime::Ptr<templates::flypath::FlightPath> getFlightPath() const { return flightPath.get(); }

	void setCurrentFlypath(const templates::flypath::FlyPathEntry* path);

	/** @return True if the player has the specified access level or higher */
	bool hasAccess(int8_t accessLevel);

	/** @return True if the player is a member of the server staff */
	bool isStaff();

	bool isEnemy(Creature& creature) override;

	/** C++: keeps Creature::isEnemyFrom(Creature&) visible next to the overrides (no hiding in Java) */
	using Creature::isEnemyFrom;

	bool isEnemyFrom(Npc& enemy) override;

	/**
	 * Player enemies:<br>
	 * - different race<br>
	 * - duel partner<br>
	 * - in pvp zone
	 */
	bool isEnemyFrom(Player& enemy) override;

	bool isAggroIconTo(Player& enemy);

	void setInFfaTeamMode(bool value) { isInFfaTeamMode_.set(value); }

	bool isInFfaTeamMode() const { return isInFfaTeamMode_.get(); }

private:
	bool isHostileInPanesterra(Player& enemy);

	bool canPvP(Player& enemy);

public:
	bool isDueling(Creature& creature);

	bool isInSameTeam(Player& player);

	bool canSee(runtime::Ptr<VisibleObject> object) override;

	std::optional<TribeClass> getTribe() override;

	TribeClass getBaseTribe() override;

	runtime::Ptr<Summon> getSummon() const { return summon.get(); }

	void setSummon(runtime::Ptr<Summon> summon);

	runtime::Ptr<Creature> getSummonOrMercenary(int32_t objectId);

	/** @param newKisk kisk to bind to (null if unbinding) */
	void setKisk(runtime::Ptr<Kisk> newKisk);

	runtime::Ptr<Kisk> getKisk() const { return kisk.get(); }

	bool hasCooldown(Item& item);

	void startCooldown(Item& item);

	int64_t getItemReuseTime(int32_t delayId);

	runtime::ConcurrentHashMap<int32_t, runtime::Ref<items::ItemCooldown>>& getItemCoolDowns() { return itemCoolDowns; }

	void addItemCoolDown(int32_t delayId, int64_t time, int32_t useDelay);

	void removeItemCoolDown(int32_t delayId);

	void setPlayerResActivate(bool isActivated) { isResByPlayer.set(isActivated); }

	bool getResStatus() const { return isResByPlayer.get(); }

	int32_t getResurrectionSkill() const { return resurrectionSkill.get(); }

	void setResurrectionSkill(int32_t value) { resurrectionSkill.set(value); }

	void setIsFlyingBeforeDeath(bool isActivated) { isFlyingBeforeDeath.set(isActivated); }

	bool getIsFlyingBeforeDeath() const { return isFlyingBeforeDeath.get(); }

	runtime::Ptr<team::alliance::PlayerAlliance> getPlayerAlliance();

	runtime::Ptr<team::alliance::PlayerAllianceGroup> getPlayerAllianceGroup() const { return playerAllianceGroup.get(); }

	bool isInAlliance();

	void setPlayerAllianceGroup(runtime::Ptr<team::alliance::PlayerAllianceGroup> playerAllianceGroup);

	/** Java final */
	bool isInLeague();

	/** Java final */
	bool isInTeam();

	/** @return current {@link PlayerGroup}, {@link PlayerAlliance} or null (Java final) */
	runtime::Ptr<team::TemporaryPlayerTeam> getCurrentTeam();

	/** @return current {@link PlayerGroup}, {@link PlayerAllianceGroup} or null (Java final) */
	runtime::Ptr<team::TemporaryPlayerTeam> getCurrentGroup();

	/** @return current team id, 0 if not in a team (Java final) */
	int32_t getCurrentTeamId();

	PortalCooldownList& getPortalCooldownList() const;

	runtime::Ptr<Cooldowns> getCraftCooldowns() const { return craftCooldowns; }

	runtime::Ptr<Cooldowns> getHouseObjectCooldowns() const { return houseObjectCooldowns; }

	runtime::Ptr<Npc> getPostman() const { return postman.get(); }

	void setPostman(runtime::Ptr<Npc> postman);

	runtime::Ptr<account::PlayerAccountData> getAccountData() const { return playerAccountData; }

	runtime::Ptr<account::Account> getAccount() const { return playerAccount; }

	std::optional<commons::database::Timestamp> getCreationDate();

	/** Quest completion */
	bool isCompleteQuest(int32_t questId);

	int64_t getNextSkillUse() const { return nextSkillUse.get(); }

	void setNextSkillUse(int64_t value) { nextSkillUse.set(value); }

	void setCasting(runtime::Ptr<skillengine::model::Skill> castingSkill) override;

	const skillengine::model::SkillTemplate* getLastSkill() const { return lastSkill.get(); }

	bool isHitTimeBoosted();

	bool isHitTimeBoosted(int64_t timeMillis);

	float getHitTimeBoostCastSpeed() const { return hitTimeBoostCastSpeed.get(); }

	void setHitTimeBoost(int64_t expireTimeMillis, float castSpeed);

	/** chain skills (created lazily) */
	runtime::Ptr<skillengine::model::ChainSkills> getChainSkills();

	void setLastCounterSkill(controllers::attack::AttackStatus status);

	int64_t getLastCounterSkill(controllers::attack::AttackStatus status);

	/** @return the Resurrection Positional State */
	bool isInResPostState() const { return isInResurrectPosState.get(); }

	/** @param value Resurrection Positional State to set */
	void setResPosState(bool value) { isInResurrectPosState.set(value); }

	/** @param value Resurrection Positional X value to set */
	void setResPosX(float value) { resPosX.set(value); }

	/** @return the Resurrection Positional X value */
	float getResPosX() const { return resPosX.get(); }

	/** @param value Resurrection Positional Y value to set */
	void setResPosY(float value) { resPosY.set(value); }

	/** @return the Resurrection Positional Y value */
	float getResPosY() const { return resPosY.get(); }

	/** @param value Resurrection Positional Z value to set */
	void setResPosZ(float value) { resPosZ.set(value); }

	/** @return the Resurrection Positional Z value */
	float getResPosZ() const { return resPosZ.get(); }

	bool isInSiegeWorld();

	bool hasPermission(int8_t perm);

	/** @return Returns the emotions, null until they are loaded (Java checks it for null, EmotionLearnAction.java:44). */
	runtime::Ptr<emotion::EmotionList> getEmotions() const;

	/** @param emotions The emotions to set. */
	void setEmotions(std::unique_ptr<emotion::EmotionList> emotions);

	runtime::Ptr<BindPointPosition> getBindPoint() const { return bindPoint.get(); }

	void setBindPoint(runtime::Ptr<BindPointPosition> bindPoint);

	templates::item::ItemAttackType getAttackType() override;

	int64_t getFlyStartTime() const { return flyStartTime.get(); }

	const templates::flypath::FlyPathEntry* getCurrentFlyPath() const { return flyLocationId.get(); }

	void resetAbyssRankListUpdated();

	void setAbyssRankListUpdated(AbyssRank_AbyssRankUpdateType type);

	bool isAbyssRankListUpdated(AbyssRank_AbyssRankUpdateType type);

	void addSalvationPoints(int64_t points);

	bool isPvpTarget(Creature& creature) override;

	bool isTargetingNpcWithFunction(int32_t objectId, int32_t dialogActionId);

	/** @return the motions */
	motion::MotionList& getMotions() const;

	/** @param motions the motions to set */
	void setMotions(std::unique_ptr<motion::MotionList> motions);

	/** @return the npcFactions */
	npcFaction::NpcFactions& getNpcFactions() const;

	/** @param npcFactions the npcFactions to set */
	void setNpcFactions(std::unique_ptr<npcFaction::NpcFactions> npcFactions);

	/** @return the flyReuseTime */
	int64_t getFlyReuseTime() const { return flyReuseTime.get(); }

	/** @param flyReuseTime the flyReuseTime to set */
	void setFlyReuseTime(int64_t value) { flyReuseTime.set(value); }

	/** Stone Use Order determined by highest inventory slot. :( If player has two types, wrong one might be used. @return selfRezItem */
	runtime::Ptr<Item> getSelfRezStone();

private:
	/** @return stoneItem or null */
	runtime::Ptr<Item> getReviveStone(int32_t stoneId);

public:
	/** Need to find how an item is determined as able to self-rez. @return boolean can self rez with item */
	bool haveSelfRezItem();

	void unsetResPosState();

	bool isLooting();

	void setLootingNpcOid(int32_t value) { lootingNpcOid.set(value); }

	int32_t getLootingNpcOid() const { return lootingNpcOid.get(); }

	/** Java final */
	bool isMentor() const { return isMentor_.get(); }

	/** Java final */
	void setMentor(bool value) { isMentor_.set(value); }

	Race getRace() override;

	Race getOppositeRace();

	int32_t getSkillCooldown(const skillengine::model::SkillTemplate* template_) override;

	void setLastMessageTime();

	int32_t floodMsgCount() const { return floodMsgCount_.get(); }

	void setRebirthEffect(const skillengine::effect::RebirthEffect* value) { rebirthEffect.set(value); }

	const skillengine::effect::RebirthEffect* getRebirthEffect() const { return rebirthEffect.get(); }

	bool canUseRebirthRevive();

	/**
	 * Put up supplements to subtraction queue, so that when moving they would not decrease, need update as confirmation To update use
	 * updateSupplements()
	 */
	void subtractSupplements(int32_t count, int32_t supplementId);

	/** Update supplements in queue and clear the queue */
	void updateSupplements();

	int8_t getPortAnimationId() const { return portAnimation.get(); }

	void setPortAnimation(animations::ArrivalAnimation portAnimation);

	bool isSkillDisabled(const skillengine::model::SkillTemplate* template_) override;

	/** @return the live house list (loaded lazily through HousingService) */
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<house::House>>> getHouses();

	void resetHouses();

	runtime::Ptr<house::House> getActiveHouse();

	runtime::Ptr<runtime::Array<float>> getBattleReturnCoords() const { return battleReturnCoords.get(); }

	void setBattleReturnCoords(int32_t mapId, runtime::Ptr<runtime::Array<float>> coords);

	int32_t getBattleReturnMap() const { return battleReturnMap.get(); }

	bool isInSprintMode() const { return isInSprintMode_.get(); }

	void setSprintMode(bool value) { isInSprintMode_.set(value); }

	void addRideObserver(controllers::observer::ActionObserver& observer);

	/** @return the live ride observer list, null before the first addRideObserver */
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<controllers::observer::ActionObserver>>> getRideObservers() const { return rideObservers.get(); }

	void setPosition(runtime::Ptr<world::WorldPosition> position) override;

	int32_t getRobotId() const { return robotId.get(); }

	void setRobotId(int32_t value) { robotId.set(value); }

	bool isInRobotMode();

	bool canPerformMove() override;

	std::string toString() override;

	void setCustomState(CustomPlayerState state);

	void unsetCustomState(CustomPlayerState state);

	bool isInCustomState(CustomPlayerState state);

	std::optional<services::panesterra::ahserion::PanesterraFaction> getPanesterraFaction() const { return panesterraFaction.get(); }

	void setPanesterraFaction(std::optional<services::panesterra::ahserion::PanesterraFaction> value) { panesterraFaction.set(value); }
};

} // namespace aion::gameserver::model::gameobjects::player
