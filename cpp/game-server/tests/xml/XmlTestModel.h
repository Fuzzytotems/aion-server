#pragma once

// A small JAXB-like model with hand-written XmlBinding specializations in the exact shape tools/xmlgen generates (XmlBinding.h contract):
// behaviour classes with private members and a friend binder, a data-only struct, an @XmlElements hierarchy, a holder with an XmlID index,
// a holder with IDREFs, wrappers, @XmlList, adapters and hooks that record their calls.

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/dataholders/loadingutils/BindContext.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/dataholders/loadingutils/adapters/LocalDateTimeAdapter.h"
#include "aion/gameserver/dataholders/loadingutils/adapters/SpaceSeparatedBytesAdapter.h"

namespace aion::gameserver::xml::test {

enum class Race : uint8_t { ELYOS, ASMODIANS, PC_ALL };
/** like ZoneAttributes: constants with @XmlEnumValue */
enum class ZoneAttribute : uint8_t { BIND, RECALL, GLIDE };

/** hook calls of the current test, in call order (tests run on one thread) */
inline std::vector<std::string> hookCalls;

} // namespace aion::gameserver::xml::test

namespace aion::gameserver::xml {

template <>
struct EnumTraits<test::Race> {
	static constexpr std::string_view javaName = "Race";
	static constexpr std::array<std::string_view, 3> names{"ELYOS", "ASMODIANS", "PC_ALL"};
	static constexpr std::array<EnumEntry<test::Race>, 3> xmlSorted{{
	  {"ASMODIANS", test::Race::ASMODIANS},
	  {"ELYOS", test::Race::ELYOS},
	  {"PC_ALL", test::Race::PC_ALL},
	}};
};
static_assert(verifyEnumTraits<test::Race>());

template <>
struct EnumTraits<test::ZoneAttribute> {
	static constexpr std::string_view javaName = "ZoneAttributes";
	static constexpr std::array<std::string_view, 3> names{"BIND", "RECALL", "GLIDE"};
	static constexpr std::array<EnumEntry<test::ZoneAttribute>, 3> xmlSorted{{
	  {"bind", test::ZoneAttribute::BIND},
	  {"glide", test::ZoneAttribute::GLIDE},
	  {"recall", test::ZoneAttribute::RECALL},
	}};
};
static_assert(verifyEnumTraits<test::ZoneAttribute>());

} // namespace aion::gameserver::xml

namespace aion::gameserver::xml::test {

/** data-only struct (generated header) */
struct Stat {
	std::string name;
	int32_t value = 0;
};

/** data-only struct with a required attribute */
struct Weapon {
	int32_t minDamage = 0;
	int32_t maxDamage = 0;
	std::optional<float> attackSpeed;
};

/** root of an @XmlElements hierarchy (behaviour class) */
class Action {
	friend struct XmlBinding<Action>;

public:
	virtual ~Action() = default;
	virtual std::string_view javaClassName() const = 0;
	int32_t getDelay() const { return delay; }

protected:
	int32_t delay = 0;
};

class SkillAction : public Action {
	friend struct XmlBinding<SkillAction>;

public:
	std::string_view javaClassName() const override { return "SkillAction"; }
	int32_t getSkillId() const { return skillId; }
	bool parentWasItem() const { return parentItem; }

private:
	int32_t skillId = 0;
	int8_t level = 1;
	bool parentItem = false;

	void afterUnmarshal(LoadContext&, const XmlParent& parent);
};

class DyeAction : public Action {
	friend struct XmlBinding<DyeAction>;

public:
	std::string_view javaClassName() const override { return "DyeAction"; }
	const std::string& getColor() const { return color; }

private:
	std::string color;
};

/** behaviour class with an @XmlID setter, adapters, lists, a choice list and a hook */
class ItemTemplate {
	friend struct XmlBinding<ItemTemplate>;

public:
	int32_t getTemplateId() const { return itemId; }
	const std::string& getName() const { return name; }
	int8_t getLevel() const { return level; }
	int16_t getWeight() const { return weight; }
	int64_t getPrice() const { return price; }
	float getSpeed() const { return speed; }
	double getRatio() const { return ratio; }
	bool isTradable() const { return tradable; }
	Race getRace() const { return race; }
	const std::optional<Race>& getOptionalRace() const { return optionalRace; }
	const std::optional<int32_t>& getRobotId() const { return robotId; }
	const std::optional<std::string>& getAlias() const { return alias; }
	const std::optional<std::vector<int32_t>>& getPreEffects() const { return preEffects; }
	const std::unordered_set<ZoneAttribute>& getZones() const { return zones; }
	const std::vector<int8_t>& getRestrictions() const { return restrictions; }
	const std::optional<adapters::LocalDateTime>& getStart() const { return start; }
	const std::string& getDescription() const { return description; }
	const std::optional<int32_t>& getCooldown() const { return cooldown; }
	const std::vector<Stat>& getStats() const { return stats; }
	const std::vector<std::unique_ptr<Stat>>& getBonusStats() const { return bonusStats; }
	const Weapon* getWeapon() const { return weapon.get(); }
	const std::vector<std::unique_ptr<Action>>& getActions() const { return actions; }
	const std::unique_ptr<Action>& getUseAction() const { return useAction; }
	size_t getStatsCapacityAtHook() const { return statsCapacityAtHook; }

private:
	int32_t itemId = 0;
	std::string name;
	int8_t level = 1;
	int16_t weight = 0;
	int64_t price = 0;
	float speed = 1.5f;
	double ratio = 0.0;
	bool tradable = true;
	Race race = Race::PC_ALL;
	std::optional<Race> optionalRace;
	std::optional<int32_t> robotId;
	std::optional<std::string> alias;
	std::optional<std::vector<int32_t>> preEffects;
	std::unordered_set<ZoneAttribute> zones;
	std::vector<int8_t> restrictions = std::vector<int8_t>(3, 1);
	std::optional<adapters::LocalDateTime> start;
	std::string description;
	std::optional<int32_t> cooldown;
	std::vector<Stat> stats;
	std::vector<std::unique_ptr<Stat>> bonusStats;
	std::unique_ptr<Weapon> weapon;
	std::vector<std::unique_ptr<Action>> actions;
	std::unique_ptr<Action> useAction;
	size_t statsCapacityAtHook = 0;

	void setXmlUid(std::string_view uid) { itemId = xml::parseInt32(uid); }
	void afterUnmarshal(LoadContext& ctx, const XmlParent& parent);
};

/** holder: <item_templates version=".."> with an index built by its hook */
class ItemData {
	friend struct XmlBinding<ItemData>;

public:
	const ItemTemplate* getItemTemplate(int32_t id) const {
		auto it = index.find(id);
		return it == index.end() ? nullptr : it->second;
	}
	size_t size() const { return index.size(); }
	const std::vector<ItemTemplate>& getItems() const { return items; }
	const std::optional<std::string>& getVersion() const { return version; }
	bool hookSawStaticDataRoot() const { return rootParent; }

private:
	std::optional<std::string> version;
	std::vector<ItemTemplate> items;
	std::unordered_map<int32_t, const ItemTemplate*> index;
	bool rootParent = false;

	void afterUnmarshal(LoadContext& ctx, const XmlParent& parent);
};

struct Location {
	int32_t mapId = 0;
	float x = 0;
	float y = 0;
	float z = 0;
};

/** like NpcEquipmentList: the value type of a class-level @XmlJavaTypeAdapter, an IDREF list */
struct GearList {
	std::vector<const ItemTemplate*> items;
};

/** like NpcEquippedGear: hand-written, built from the adapter value type and initialized eagerly after IDREF resolution */
class Gear {
public:
	explicit Gear(std::unique_ptr<GearList> list) : list(std::move(list)) {}
	void init(LoadContext& ctx) {
		ctx.runAfterIdRefResolution([this] {
			for (const ItemTemplate* item : list->items)
				names += item->getName() + ";";
		});
	}
	const std::string& getNames() const { return names; }

private:
	std::unique_ptr<GearList> list;
	std::string names;
};

/** data-only struct with IDREFs, an @XmlList element, wrappers and an adapter */
struct PlayerCreationData {
	Race race = Race::PC_ALL;
	const ItemTemplate* weapon = nullptr;
	std::vector<const ItemTemplate*> items;
	std::vector<const ItemTemplate*> gifts;
	std::optional<std::vector<Race>> allies;
	std::optional<std::vector<std::string>> properties;
	std::optional<std::vector<Stat>> bonuses;
	std::unique_ptr<Gear> gear;
};

/** holder with a declared dependency on ItemData and a required element */
class PlayerInitialData {
	friend struct XmlBinding<PlayerInitialData>;

public:
	const std::vector<PlayerCreationData>& getPlayers() const { return players; }
	const Location* getElyosSpawn() const { return elyosSpawn.get(); }
	size_t itemsSeenByHook() const { return itemCount; }
	bool idRefTaskRan() const { return afterIdRefs; }

private:
	std::vector<PlayerCreationData> players;
	std::unique_ptr<Location> elyosSpawn;
	size_t itemCount = 0;
	bool afterIdRefs = false;

	void afterUnmarshal(LoadContext& ctx, const XmlParent& parent);
};

/** holder whose hook reads ItemData without declaring the dependency */
class UndeclaredReader {
	friend struct XmlBinding<UndeclaredReader>;

private:
	void afterUnmarshal(LoadContext& ctx, const XmlParent& parent);
};

/** the StaticData root parent stand-in */
struct StaticDataRoot {};

/** single elements and a wrapper whose objects register XmlIDs, IDREF slots and after-IDREF tasks (repeated-element lifetime tests) */
struct Loadout {
	std::unique_ptr<ItemTemplate> item;
	std::unique_ptr<PlayerCreationData> player;
	std::optional<std::vector<PlayerCreationData>> reserves;
};

// ---- hooks ------------------------------------------------------------------------------------------------------------------------------------

inline void SkillAction::afterUnmarshal(LoadContext&, const XmlParent& parent) {
	parentItem = parent.is<ItemTemplate>();
	hookCalls.push_back("SkillAction:" + std::to_string(skillId));
}

inline void ItemTemplate::afterUnmarshal(LoadContext& ctx, const XmlParent& parent) {
	statsCapacityAtHook = stats.capacity();
	if (level == -128)
		ctx.fail("level -128 is reserved");
	hookCalls.push_back("ItemTemplate:" + std::to_string(itemId) + (parent.is<ItemData>() ? "@ItemData" : ""));
}

inline void ItemData::afterUnmarshal(LoadContext&, const XmlParent& parent) {
	for (const ItemTemplate& item : items)
		index[item.getTemplateId()] = &item;
	rootParent = parent.is<StaticDataRoot>();
	hookCalls.push_back("ItemData:" + std::to_string(items.size()));
}

inline void PlayerInitialData::afterUnmarshal(LoadContext& ctx, const XmlParent&) {
	itemCount = ctx.holder<ItemData>()->size();
	ctx.runAfterIdRefResolution([this] { afterIdRefs = !players.empty() && players.front().weapon != nullptr; });
	hookCalls.push_back("PlayerInitialData");
}

inline void UndeclaredReader::afterUnmarshal(LoadContext& ctx, const XmlParent&) {
	static_cast<void>(ctx.holder<ItemData>());
}

} // namespace aion::gameserver::xml::test

// ---- "generated" bindings -------------------------------------------------------------------------------------------------------------------

namespace aion::gameserver::xml {

template <>
struct XmlBinding<test::Stat> {
	static constexpr std::string_view CLASS_NAME = "Stat";
	static bool attribute(test::Stat& o, BindContext& c, std::string_view n, std::string_view v) {
		switch (nameHash(n)) {
			case "name"_xh:
				if (n == "name") {
					c.assign(o.name, v);
					return true;
				}
				break;
			case "value"_xh:
				if (n == "value") {
					c.assign(o.value, v);
					return true;
				}
				break;
		}
		return false;
	}
	static void finish(test::Stat&, BindContext& c, const XmlParent&) { c.checkRequiredAttributes({"name"}); }
};

template <>
struct XmlBinding<test::Weapon> {
	static constexpr std::string_view CLASS_NAME = "WeaponStats";
	static bool attribute(test::Weapon& o, BindContext& c, std::string_view n, std::string_view v) {
		if (n == "min_damage") {
			c.assign(o.minDamage, v);
			return true;
		}
		if (n == "max_damage") {
			c.assign(o.maxDamage, v);
			return true;
		}
		if (n == "attack_speed") {
			c.assign(o.attackSpeed, v);
			return true;
		}
		return false;
	}
	static void finish(test::Weapon&, BindContext& c, const XmlParent&) { c.checkRequiredAttributes({"min_damage", "max_damage"}); }
};

template <>
struct XmlBinding<test::Action> {
	static constexpr std::string_view CLASS_NAME = "Action";
	static bool attribute(test::Action& o, BindContext& c, std::string_view n, std::string_view v) {
		if (n == "delay") {
			c.assign(o.delay, v);
			return true;
		}
		return false;
	}
	static bool element(test::Action&, BindContext&, pugi::xml_node, std::string_view) { return false; }
};

template <>
struct XmlBinding<test::SkillAction> {
	static constexpr std::string_view CLASS_NAME = "SkillAction";
	static bool attribute(test::SkillAction& o, BindContext& c, std::string_view n, std::string_view v) {
		if (n == "skill_id") {
			c.assign(o.skillId, v);
			return true;
		}
		if (n == "level") {
			c.assign(o.level, v);
			return true;
		}
		return XmlBinding<test::Action>::attribute(o, c, n, v);
	}
	static bool element(test::SkillAction& o, BindContext& c, pugi::xml_node e, std::string_view n) {
		return XmlBinding<test::Action>::element(o, c, e, n);
	}
	static void finish(test::SkillAction& o, BindContext& c, const XmlParent& p) {
		c.checkRequiredAttributes({"skill_id"});
		if (c.hooksEnabled())
			o.afterUnmarshal(c.load(), p);
	}
};

template <>
struct XmlBinding<test::DyeAction> {
	static constexpr std::string_view CLASS_NAME = "DyeAction";
	static bool attribute(test::DyeAction& o, BindContext& c, std::string_view n, std::string_view v) {
		if (n == "color") {
			c.assign(o.color, v);
			return true;
		}
		return XmlBinding<test::Action>::attribute(o, c, n, v);
	}
};

namespace test {
inline constexpr ChoiceEntry<Action> ACTION_CHOICES[] = {
  choice<Action, DyeAction>("dye"),
  choice<Action, SkillAction>("skilllearn"),
};
static_assert(isStrictlySortedByName(ACTION_CHOICES));
inline constexpr ElementFactory<Action> ACTION_FACTORY{ACTION_CHOICES};

inline constexpr ChoiceEntry<Action> USE_ACTION_CHOICES[] = {
  choice<Action, SkillAction>("use_skill"),
};
inline constexpr ElementFactory<Action> USE_ACTION_FACTORY{USE_ACTION_CHOICES};
} // namespace test

template <>
struct XmlBinding<test::ItemTemplate> {
	static constexpr std::string_view CLASS_NAME = "ItemTemplate";
	static bool attribute(test::ItemTemplate& o, BindContext& c, std::string_view n, std::string_view v) {
		switch (nameHash(n)) {
			case "id"_xh:
				if (n == "id") {
					o.setXmlUid(v);
					c.registerXmlId(v, o);
					return true;
				}
				break;
			case "name"_xh:
				if (n == "name") {
					c.assign(o.name, v);
					return true;
				}
				break;
			case "level"_xh:
				if (n == "level") {
					c.assign(o.level, v);
					return true;
				}
				break;
			case "weight"_xh:
				if (n == "weight") {
					c.assign(o.weight, v);
					return true;
				}
				break;
			case "price"_xh:
				if (n == "price") {
					c.assign(o.price, v);
					return true;
				}
				break;
			case "speed"_xh:
				if (n == "speed") {
					c.assign(o.speed, v);
					return true;
				}
				break;
			case "ratio"_xh:
				if (n == "ratio") {
					c.assign(o.ratio, v);
					return true;
				}
				break;
			case "tradable"_xh:
				if (n == "tradable") {
					c.assign(o.tradable, v);
					return true;
				}
				break;
			case "race"_xh:
				if (n == "race") {
					c.assign(o.race, v);
					return true;
				}
				break;
			case "optional_race"_xh:
				if (n == "optional_race") {
					c.assign(o.optionalRace, v);
					return true;
				}
				break;
			case "robot_id"_xh:
				if (n == "robot_id") {
					c.assign(o.robotId, v);
					return true;
				}
				break;
			case "alias"_xh:
				if (n == "alias") {
					c.assign(o.alias, v);
					return true;
				}
				break;
			case "pre_effects"_xh:
				if (n == "pre_effects") {
					c.assignList(o.preEffects, v);
					return true;
				}
				break;
			case "zones"_xh:
				if (n == "zones") {
					c.assignList(o.zones, v);
					return true;
				}
				break;
			case "restrict"_xh:
				if (n == "restrict") {
					o.restrictions = adapters::parseSpaceSeparatedBytes(c, v);
					return true;
				}
				break;
			case "start"_xh:
				if (n == "start") {
					o.start = c.adapt([](std::string_view s) { return adapters::parseLocalDateTime(s); }, v);
					return true;
				}
				break;
			case "c_name"_xh:
				if (n == "c_name") {
					static_cast<void>(v); // not bound by Java, ignored like JAXB (xmlgen.toml [ignore_attributes])
					c.ignoreAttribute();
					return true;
				}
				break;
		}
		return false;
	}
	static bool element(test::ItemTemplate& o, BindContext& c, pugi::xml_node e, std::string_view n) {
		switch (nameHash(n)) {
			case "desc"_xh:
				if (n == "desc") {
					c.bindText(o.description, e);
					return true;
				}
				break;
			case "cooldown"_xh:
				if (n == "cooldown") {
					c.bindText(o.cooldown, e);
					return true;
				}
				break;
			case "stat"_xh:
				if (n == "stat") {
					c.bindList(o.stats, e);
					return true;
				}
				break;
			case "bonus_stat"_xh:
				if (n == "bonus_stat") {
					c.bindList(o.bonusStats, e);
					return true;
				}
				break;
			case "weapon"_xh:
				if (n == "weapon") {
					c.bindSingle(o.weapon, e);
					return true;
				}
				break;
			case "comment"_xh:
				if (n == "comment") {
					c.ignoreElement(e);
					return true;
				}
				break;
		}
		if (c.bindChoice(o.actions, test::ACTION_FACTORY, e, n))
			return true;
		if (c.bindChoice(o.useAction, test::USE_ACTION_FACTORY, e, n))
			return true;
		return false;
	}
	static void reserve(test::ItemTemplate& o, const ChildCounts& counts) { o.stats.reserve(counts["stat"]); }
	static void finish(test::ItemTemplate& o, BindContext& c, const XmlParent& p) {
		c.checkRequiredAttributes({"id", "name"});
		if (c.hooksEnabled())
			o.afterUnmarshal(c.load(), p);
	}
};

template <>
struct XmlBinding<test::ItemData> {
	static constexpr std::string_view CLASS_NAME = "ItemData";
	static bool attribute(test::ItemData& o, BindContext& c, std::string_view n, std::string_view v) {
		if (n == "version") {
			c.assign(o.version, v);
			return true;
		}
		return false;
	}
	static bool element(test::ItemData& o, BindContext& c, pugi::xml_node e, std::string_view n) {
		if (n == "item_template") {
			c.bindList(o.items, e);
			return true;
		}
		return false;
	}
	static void reserve(test::ItemData& o, const ChildCounts& counts) { o.items.reserve(counts["item_template"]); }
	static void finish(test::ItemData& o, BindContext& c, const XmlParent& p) {
		if (c.hooksEnabled())
			o.afterUnmarshal(c.load(), p);
	}
};

template <>
struct XmlBinding<test::Location> {
	static constexpr std::string_view CLASS_NAME = "LocationData";
	static bool attribute(test::Location& o, BindContext& c, std::string_view n, std::string_view v) {
		if (n == "map_id") {
			c.assign(o.mapId, v);
			return true;
		}
		if (n == "x") {
			c.assign(o.x, v);
			return true;
		}
		if (n == "y") {
			c.assign(o.y, v);
			return true;
		}
		if (n == "z") {
			c.assign(o.z, v);
			return true;
		}
		return false;
	}
};

template <>
struct XmlBinding<test::GearList> {
	static constexpr std::string_view CLASS_NAME = "NpcEquipmentList";
	static bool element(test::GearList& o, BindContext& c, pugi::xml_node e, std::string_view n) {
		if (n == "item") {
			c.idRefText(o.items, e);
			return true;
		}
		return false;
	}
};

template <>
struct XmlBinding<test::PlayerCreationData> {
	static constexpr std::string_view CLASS_NAME = "PlayerCreationData";
	static bool attribute(test::PlayerCreationData& o, BindContext& c, std::string_view n, std::string_view v) {
		if (n == "race") {
			c.assign(o.race, v);
			return true;
		}
		if (n == "weapon") {
			c.idRef(o.weapon, v);
			return true;
		}
		if (n == "gifts") {
			c.idRefList(o.gifts, v);
			return true;
		}
		return false;
	}
	static bool element(test::PlayerCreationData& o, BindContext& c, pugi::xml_node e, std::string_view n) {
		if (n == "item") {
			c.idRefText(o.items, e);
			return true;
		}
		if (n == "allies") {
			c.bindTextList(o.allies, e);
			return true;
		}
		if (n == "properties") {
			c.bindWrapper(o.properties, e, "property");
			return true;
		}
		if (n == "bonuses") {
			c.bindWrapper(o.bonuses, e, "stat");
			return true;
		}
		if (n == "equipment") { // class-level adapter: bind the value type on the heap (IDREF slots must not move), then convert
			auto list = std::make_unique<test::GearList>();
			c.bindObject(*list, e, c.currentObject());
			c.replaceSingle(o.gear, std::make_unique<test::Gear>(std::move(list)), e);
			o.gear->init(c.load());
			return true;
		}
		return false;
	}
	static void finish(test::PlayerCreationData&, BindContext& c, const XmlParent&) { c.checkRequiredAttributes({"race"}); }
};

template <>
struct XmlBinding<test::PlayerInitialData> {
	static constexpr std::string_view CLASS_NAME = "PlayerInitialData";
	static bool element(test::PlayerInitialData& o, BindContext& c, pugi::xml_node e, std::string_view n) {
		if (n == "player_data") {
			c.bindList(o.players, e);
			return true;
		}
		if (n == "elyos_spawn_location") {
			c.bindSingle(o.elyosSpawn, e);
			return true;
		}
		return false;
	}
	static void reserve(test::PlayerInitialData& o, const ChildCounts& counts) { o.players.reserve(counts["player_data"]); }
	static void finish(test::PlayerInitialData& o, BindContext& c, const XmlParent& p) {
		c.checkRequiredElements({"elyos_spawn_location"});
		if (c.hooksEnabled())
			o.afterUnmarshal(c.load(), p);
	}
};

template <>
struct XmlBinding<test::Loadout> {
	static constexpr std::string_view CLASS_NAME = "Loadout";
	static bool element(test::Loadout& o, BindContext& c, pugi::xml_node e, std::string_view n) {
		if (n == "item_template") {
			c.bindSingle(o.item, e);
			return true;
		}
		if (n == "player_data") {
			c.bindSingle(o.player, e);
			return true;
		}
		if (n == "reserves") {
			c.bindWrapper(o.reserves, e, "player_data");
			return true;
		}
		return false;
	}
};

template <>
struct XmlBinding<test::UndeclaredReader> {
	static constexpr std::string_view CLASS_NAME = "UndeclaredReader";
	static void finish(test::UndeclaredReader& o, BindContext& c, const XmlParent& p) { o.afterUnmarshal(c.load(), p); }
};

} // namespace aion::gameserver::xml
