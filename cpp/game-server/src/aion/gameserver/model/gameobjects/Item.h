#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/Expirable.h"
#include "aion/gameserver/model/enchants/fwd.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/items/fwd.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/model/templates/L10n.h"
#include "aion/gameserver/model/templates/item/enums/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * Hub header (docs/design/hub-headers.md). RefCounted (through AionObject): `Item::create(...)` replaces the three Java constructors. Expirable
 * and StatOwner are held by Ref (ExpireTimerTask, stat function owners), so Item forwards their retain()/release() to RefCounted (§9.2).
 * The mana stone and fusion stone sets are TreeSets ordered by slot, as in Java (the anonymous Comparator Item$1, created by
 * itemStonesCollection); fieldmap still guesses RcHashSet until fieldmap.toml overrides it. The idian stone is a part of the item
 * (cycles review: IdianStone.item only holds its owner), replaced through setIdianStone(std::unique_ptr). The charge info part is replaced and set to
 * null by updateChargeInfo, so getConditioningInfo() returns Ptr.
 *
 * @author ATracer, Wakizashi, xTz
 */
class Item : public AionObject, public Expirable, public stats::calc::StatOwner, public Persistable, public templates::L10n {
	AION_MAKE_REF_FRIEND
public:
	static constexpr int32_t MAX_BASIC_STONES = 6;

private:
	// Java: private static final Logger log = LoggerFactory.getLogger(Item.class) - namespace-scope logger in Item.cpp
	runtime::Field<int64_t> itemCount{1};
	runtime::Field<std::optional<int32_t>> itemColor{};
	runtime::Field<int32_t> colorExpireTime{0};
	runtime::Field<std::string> itemCreator{};
	const templates::item::ItemTemplate* itemTemplate;
	runtime::Field<const templates::item::ItemTemplate*> itemSkinTemplate{};
	runtime::Field<const templates::item::ItemTemplate*> fusionedItemTemplate{};
	runtime::Field<bool> isEquipped_{false};
	runtime::Field<int64_t> equipmentSlot; // Java: = ItemStorage.FIRST_AVAILABLE_SLOT (constructors)
	runtime::Field<Persistable::PersistentState> persistentState{};
	// fieldmap: Java TreeSet ordered by slot (itemStonesCollection, comparator Item$1); fieldmap guessed HashSet (cycles change request)
	runtime::Field<runtime::Ref<runtime::RcTreeSet<runtime::Ref<items::ManaStone>>>> manaStones{};
	// fieldmap: Java TreeSet ordered by slot (itemStonesCollection, comparator Item$1); fieldmap guessed HashSet (cycles change request)
	runtime::Field<runtime::Ref<runtime::RcTreeSet<runtime::Ref<items::ManaStone>>>> fusionStones{};
	runtime::Field<int32_t> optionalSockets{};
	runtime::Field<int32_t> fusionedItemOptionalSockets{};
	runtime::Field<runtime::Ref<items::GodStone>> godStone{};
	/** Replaced by PolishAction and removed by the stone itself (IdianStone.java:95): retired to the Reclaimer */
	runtime::PartSlot<items::IdianStone, runtime::RetireTo::RECLAIMER> idianStone{*this}; // fieldmap: part (setII.toml; RECLAIMER: replaced repeatedly)
	runtime::Field<bool> isSoulBound_{false};
	runtime::Field<int32_t> itemLocation{};
	runtime::Field<int32_t> enchantLevel{};
	runtime::Field<int32_t> enchantBonus{};
	const int32_t expireTime; // Java: = 0, set by the constructors
	runtime::Field<int32_t> temporaryExchangeTime{0};
	runtime::Field<int64_t> repurchasePrice{};
	runtime::Field<int32_t> activationCount{0};
	runtime::PartSlot<items::ChargeInfo> conditioningInfo{*this};
	runtime::Field<runtime::Ref<runtime::RcArrayList<const stats::calc::functions::StatFunction*>>> currentModifiers{};
	runtime::Field<int32_t> tuneCount{0};
	runtime::Field<runtime::Ref<items::RandomBonusEffect>> bonusStatsEffect{};
	runtime::Field<runtime::Ref<items::RandomBonusEffect>> fusionedItemBonusStatsEffect{};
	runtime::Field<int32_t> packCount{};
	runtime::Field<int32_t> tempering{};
	runtime::Field<runtime::Ref<enchants::EnchantEffect>> enchantEffect{};
	runtime::Field<runtime::Ref<enchants::TemperingEffect>> temperingEffect{};
	runtime::Field<bool> isAmplified_{false};
	runtime::Field<int32_t> buffSkill{};
	runtime::Field<int32_t> rndPlumeBonusValue{};
	runtime::Field<runtime::Ref<items::PendingTuneResult>> pendingTuneResult{};

protected:
	/** Create simple item with minimum information */
	Item(int32_t objId, const templates::item::ItemTemplate* itemTemplate);

	/** This constructor should be called from ItemService for newly created items and loadedFromDb */
	Item(int32_t objId, const templates::item::ItemTemplate* itemTemplate, int64_t itemCount, bool isEquipped, int64_t equipmentSlot);

	/** This constructor should be called only from DAO while loading from DB */
	Item(int32_t objId, int32_t itemId, int64_t itemCount, std::optional<int32_t> itemColor, int32_t colorExpires, std::string_view itemCreator,
		int32_t expireTime, int32_t activationCount, bool isEquipped, bool isSoulBound, int64_t equipmentSlot, int32_t itemLocation, int32_t enchant,
		int32_t enchantBonus, int32_t itemSkin, int32_t fusionedItem, int32_t optionalSockets, int32_t fusionedItemOptionalSockets, int32_t charge,
		int32_t tuneCount, int32_t statBonusId, int32_t fusionedItemStatBonusId, int32_t tempering, int32_t packCount, bool isAmplified, int32_t buffSkill,
		int32_t rndPlumeBonusValue);

	~Item() override;

public:
	/** Java `new Item(objId, itemTemplate)` */
	static runtime::Ref<Item> create(int32_t objId, const templates::item::ItemTemplate* itemTemplate);

	/** Java `new Item(objId, itemTemplate, itemCount, isEquipped, equipmentSlot)` */
	static runtime::Ref<Item> create(int32_t objId, const templates::item::ItemTemplate* itemTemplate, int64_t itemCount, bool isEquipped,
		int64_t equipmentSlot);

	/** Java `new Item(...)` of the DAO */
	static runtime::Ref<Item> create(int32_t objId, int32_t itemId, int64_t itemCount, std::optional<int32_t> itemColor, int32_t colorExpires,
		std::string_view itemCreator, int32_t expireTime, int32_t activationCount, bool isEquipped, bool isSoulBound, int64_t equipmentSlot,
		int32_t itemLocation, int32_t enchant, int32_t enchantBonus, int32_t itemSkin, int32_t fusionedItem, int32_t optionalSockets,
		int32_t fusionedItemOptionalSockets, int32_t charge, int32_t tuneCount, int32_t statBonusId, int32_t fusionedItemStatBonusId, int32_t tempering,
		int32_t packCount, bool isAmplified, int32_t buffSkill, int32_t rndPlumeBonusValue);

	/** C++ only: Expirable and StatOwner are held by Ref (hub-headers.md §9.2); retains the item itself. */
	void retain() const noexcept override { runtime::RefCounted::retain(); }

	void release() const noexcept override { runtime::RefCounted::release(); }

	int32_t getTempering() const { return tempering.get(); }

	void setTempering(int32_t tempering);

private:
	void updateChargeInfo(int32_t charge);

public:
	std::string getName() override;

	/** @return itemCreator (Java: "" for null) */
	std::string getItemCreator();

	/** @param itemCreator the itemCreator to set */
	void setItemCreator(std::string_view value) { itemCreator.set(std::string(value)); }

	std::string getItemName();

	int32_t getOptionalSockets() const { return optionalSockets.get(); }

	void setOptionalSockets(int32_t value) { optionalSockets.set(value); }

	bool hasOptionalSocket();

	int32_t getFusionedItemOptionalSockets() const { return fusionedItemOptionalSockets.get(); }

	bool hasOptionalFusionSocket();

	void setFusionedItemOptionalSockets(int32_t value) { fusionedItemOptionalSockets.set(value); }

	int32_t getEnchantBonus() const { return enchantBonus.get(); }

	void setEnchantBonus(int32_t value) { enchantBonus.set(value); }

	bool isStigmaChargeable();

	/** @return the itemTemplate */
	const templates::item::ItemTemplate* getItemTemplate() const { return itemTemplate; }

	/** @return the itemAppearanceTemplate */
	const templates::item::ItemTemplate* getItemSkinTemplate();

	void setItemSkinTemplate(const templates::item::ItemTemplate* newTemplate);

	bool isSkinnedItem();

	/** @return RGB color value (no alpha channel info) or null if not dyed */
	std::optional<int32_t> getItemColor() const { return itemColor.get(); }

	/** @param color - the item color to set (RGB color value or null to remove dye) */
	void setItemColor(std::optional<int32_t> color);

	/** @return positive values if not expired, 0 for not expirable, negative for expired */
	int32_t getColorTimeLeft();

	int32_t getColorExpireTime() const { return colorExpireTime.get(); }

	void setColorExpireTime(int32_t dyeRemainsUntil);

	/** @return the itemCount Number of this item in stack. Should be not more than template maxstackcount ? */
	int64_t getItemCount() const { return itemCount.get(); }

	int64_t getFreeCount();

	/** @param itemCount the itemCount to set */
	void setItemCount(int64_t itemCount);

	/**
	 * This method should be called ONLY from Storage class In all other ways it is not guaranteed to be udpated in a regular update service It is
	 * allowed to use this method for newly created items which are not yet in any storage
	 */
	int64_t increaseItemCount(int64_t count);

	/**
	 * This method should be called ONLY from Storage class In all other ways it is not guaranteed to be udpated in a regular update service It is
	 * allowed to use this method for newly created items which are not yet in any storage
	 */
	int64_t decreaseItemCount(int64_t count);

	/** @return the isEquipped */
	bool isEquipped() const { return isEquipped_.get(); }

	/** @param isEquipped the isEquipped to set */
	void setEquipped(bool isEquipped);

	/**
	 * @return the equipmentSlot Equipment slot can be of 2 types - one is the ItemSlot enum type if equipped, second - is position in cube FIXME:
	 *         That's the biggest nonsense!!! Slot value is Q, while position in Cube is H [RR]
	 */
	int64_t getEquipmentSlot() const { return equipmentSlot.get(); }

	/** @param equipmentSlot the equipmentSlot to set */
	void setEquipmentSlot(int64_t equipmentSlot);

	/** This method should be used to lazy initialize empty manastone list. @return the live set of item stones */
	runtime::Ptr<runtime::RcTreeSet<runtime::Ref<items::ManaStone>>> getItemStones();

	/** This method should be used to lazy initialize empty manastone list. @return the live set of fusion stones */
	runtime::Ptr<runtime::RcTreeSet<runtime::Ref<items::ManaStone>>> getFusionStones();

	int32_t getFusionStonesSize();

	int32_t getItemStonesSize();

private:
	/** Java: new TreeSet<>(comparator by slot) */
	runtime::Ref<runtime::RcTreeSet<runtime::Ref<items::ManaStone>>> itemStonesCollection();

public:
	/** Check manastones without initialization */
	bool hasManaStones();

	/** Check fusionstones without initialization */
	bool hasFusionStones();

	bool hasIdianStone();

	bool hasGodStone();

	runtime::Ptr<items::GodStone> getGodStone() const { return godStone.get(); }

	int32_t getGodStoneId();

	void addGodStone(int32_t itemId);

	void addGodStone(int32_t itemId, int32_t activatedCount);

	void setGodStone(runtime::Ptr<items::GodStone> godStone);

	/** @return the echantLevel */
	int32_t getEnchantLevel() const { return enchantLevel.get(); }

	/** @param enchantLevel the echantLevel to set */
	void setEnchantLevel(int32_t enchantLevel);

	/** @return the persistentState */
	PersistentState getPersistentState() override { return persistentState.get(); }

	/**
	 * Possible changes: NEW -> UPDATED NEW -> UPDATE_REQURIED UPDATE_REQUIRED -> DELETED UPDATE_REQUIRED -> UPDATED UPDATED -> DELETED UPDATED ->
	 * UPDATE_REQUIRED
	 */
	void setPersistentState(PersistentState persistentState) override;

	void setItemLocation(int32_t storageType);

	int32_t getItemLocation() const { return itemLocation.get(); }

	int32_t getItemMask();

	bool isSoulBound() const { return isSoulBound_.get(); }

	void setSoulBound(bool isSoulBound);

	templates::item::enums::EquipType getEquipmentType();

	int32_t getItemId();

	int32_t getL10nId() const override;

	bool hasFusionedItem();

	const templates::item::ItemTemplate* getFusionedItemTemplate() const { return fusionedItemTemplate.get(); }

	int32_t getFusionedItemId();

	void setFusionedItem(runtime::Ptr<Item> fusionedItem);

	void setFusionedItem(const templates::item::ItemTemplate* template_, int32_t bonusStatsId, int32_t optionalSockets);

private:
	void removeAllFusionStones();

public:
	int32_t getSockets(bool isFusionItem);

	bool isStorableInWarehouse();

	bool isStorableInAccWarehouse();

	bool isStorableInLegWarehouse();

	bool isTradeable();

	bool isLegionTradeable();

	bool isRemodelable();

	bool isSellable();

	bool canApExtract();

	bool canSocketGodstone();

	/** @return Returns the expireTime. */
	int32_t getExpireTime() override { return expireTime; }

	/** @return Returns the temporaryExchangeTime. */
	int32_t getTemporaryExchangeTime() const { return temporaryExchangeTime.get(); }

	int32_t getTemporaryExchangeTimeRemaining();

	/** @param temporaryExchangeTime The temporaryExchangeTime to set. */
	void setTemporaryExchangeTime(int32_t value) { temporaryExchangeTime.set(value); }

	void onExpire(player::Player& player) override;

	void onBeforeExpire(player::Player& player, int32_t remainingMinutes) override;

	void setRepurchasePrice(int64_t price) { repurchasePrice.set(price); }

	int64_t getRepurchasePrice() const { return repurchasePrice.get(); }

	int32_t getActivationCount() const { return activationCount.get(); }

	void setActivationCount(int32_t value) { activationCount.set(value); }

	/** @return the charge info part, null for items without a charge level */
	runtime::Ptr<items::ChargeInfo> getConditioningInfo() const;

	int32_t getChargePoints();

	int32_t getChargeLevel();

	/** Calculate charge level based on main item and fusioned item */
	int32_t calculateMaxChargeLevel();

	/** Check for disabled charge levels due to recommend rank restriction */
	int32_t calculateAvailableChargeLevel(player::Player& player);

	const templates::item::Improvement* getImprovement();

	/** @return the live modifier list (created lazily) */
	runtime::Ptr<runtime::RcArrayList<const stats::calc::functions::StatFunction*>> getCurrentModifiers();

	void setCurrentModifiers(const std::vector<const stats::calc::functions::StatFunction*>& currentModifiers);

	/** @return the idian stone, null if there is none (Java checks it for null, PolishChargeCondition.java:24) */
	runtime::Ptr<items::IdianStone> getIdianStone() const;

	/** @param idianStone the new stone part (nullptr removes it) */
	void setIdianStone(std::unique_ptr<items::IdianStone> idianStone);

	int32_t getBonusStatsId();

	runtime::Ptr<items::RandomBonusEffect> getBonusStatsEffect() const { return bonusStatsEffect.get(); }

	/** Must only be called while the item is unequipped, otherwise the old stats will remain active. */
	void setBonusStats(int32_t statBonusId, bool validate);

	int32_t getTuneCount() const { return tuneCount.get(); }

	void setTuneCount(int32_t tuneCount);

	void removeRemainingTuningCountIfPossible();

	/** @return False if the item must be identified (tuned) before it can be equipped (identification can be made without a tuning scroll) */
	bool isIdentified();

	int32_t getFusionedItemBonusStatsId();

	runtime::Ptr<items::RandomBonusEffect> getFusionedItemBonusStatsEffect() const { return fusionedItemBonusStatsEffect.get(); }

	/** Must only be called while the item is unequipped, otherwise the old stats will remain active. */
	void setFusionedItemBonusStats(int32_t statBonusId, bool validate);

	void setTemperingEffect(runtime::Ptr<enchants::TemperingEffect> temperingEffect);

	runtime::Ptr<enchants::TemperingEffect> getTemperingEffect() const { return temperingEffect.get(); }

	void setEnchantEffect(runtime::Ptr<enchants::EnchantEffect> enchantEffect);

	runtime::Ptr<enchants::EnchantEffect> getEnchantEffect() const { return enchantEffect.get(); }

	int32_t getPackCount() const { return packCount.get(); }

	void setPackCount(int32_t value) { packCount.set(value); }

	int32_t getMaxEnchantLevel();

	int32_t getItemEnchantParam();

	bool isAmplified() const { return isAmplified_.get(); }

	void setAmplified(bool isAmplified);

	int32_t getBuffSkill() const { return buffSkill.get(); }

	void setBuffSkill(int32_t value) { buffSkill.set(value); }

	int32_t getRndPlumeBonusValue() const { return rndPlumeBonusValue.get(); }

	void setRndPlumeBonusValue(int32_t value) { rndPlumeBonusValue.set(value); }

	runtime::Ptr<items::PendingTuneResult> getPendingTuneResult() const { return pendingTuneResult.get(); }

	void setPendingTuneResult(runtime::Ptr<items::PendingTuneResult> pendingTuneResult);

	std::string toString() override;
};

} // namespace aion::gameserver::model::gameobjects
