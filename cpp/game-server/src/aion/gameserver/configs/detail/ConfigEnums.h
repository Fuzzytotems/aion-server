#pragma once

/**
 * PLACEHOLDER ENUMS - replace with the real game server types once they are ported.
 * <p>
 * Minimal declarations of the enums that config fields use but that do not exist in the C++ port yet (wave 1 has no model code). They only carry
 * the constant names in Java ordinal order, which is all the config transformers need (magic_enum names; std::map iteration in ordinal order
 * like Java's EnumMap). Constructor data and methods of the Java enums are intentionally missing.
 * <p>
 * When the static data generator (T2) or the model port creates the real enums, delete the matching declaration here and change the config
 * headers to the real type:
 * <ul>
 * <li>ItemQuality: com.aionemu.gameserver.model.templates.item.ItemQuality (generated, @XmlEnum) - DropConfig::MIN_ANNOUNCE_QUALITY</li>
 * <li>HouseType: com.aionemu.gameserver.model.templates.housing.HouseType (hand-written) - HousingConfig::AUCTION_AUTO_FILL_LIMITS</li>
 * <li>NpcRating: com.aionemu.gameserver.model.templates.npc.NpcRating (generated, @XmlEnum) - InstanceConfig::INSTANCE_SCALING_NPC_MIN_RATING</li>
 * <li>AbyssRankEnum: com.aionemu.gameserver.utils.stats.AbyssRankEnum (generated, @XmlEnum) - RankingConfig::XFORM_MIN_RANK,
 * TOP_RANKING_QUOTA, TOP_RANKING_GP_LOSS</li>
 * </ul>
 * If a generated enum replaces magic_enum with EnumTraits (static-data.md §2.5), commons' EnumTransformer must learn EnumTraits first.
 * <p>
 * Not in Java.
 */
namespace aion::gameserver::configs::detail {

/** Placeholder for com.aionemu.gameserver.model.templates.item.ItemQuality */
enum class ItemQuality { JUNK, COMMON, RARE, LEGEND, UNIQUE, EPIC, MYTHIC };

/** Placeholder for com.aionemu.gameserver.model.templates.housing.HouseType */
enum class HouseType { ESTATE, MANSION, HOUSE, STUDIO, PALACE };

/** Placeholder for com.aionemu.gameserver.model.templates.npc.NpcRating */
enum class NpcRating { JUNK, NORMAL, ELITE, HERO, LEGENDARY };

/** Placeholder for com.aionemu.gameserver.utils.stats.AbyssRankEnum */
enum class AbyssRankEnum {
	GRADE9_SOLDIER,
	GRADE8_SOLDIER,
	GRADE7_SOLDIER,
	GRADE6_SOLDIER,
	GRADE5_SOLDIER,
	GRADE4_SOLDIER,
	GRADE3_SOLDIER,
	GRADE2_SOLDIER,
	GRADE1_SOLDIER,
	STAR1_OFFICER,
	STAR2_OFFICER,
	STAR3_OFFICER,
	STAR4_OFFICER,
	STAR5_OFFICER,
	GENERAL,
	GREAT_GENERAL,
	COMMANDER,
	SUPREME_COMMANDER
};

} // namespace aion::gameserver::configs::detail
