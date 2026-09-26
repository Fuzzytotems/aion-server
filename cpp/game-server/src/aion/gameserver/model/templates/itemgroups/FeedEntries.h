#pragma once

#include "aion/gameserver/model/templates/itemgroups/ItemRaceEntry.h"

namespace aion::gameserver::model::templates::itemgroups {

/**
 * Java com.aionemu.gameserver.model.templates.itemgroups.FeedEntries: JAXB types of pet food entries.
 * <p>
 * C++: no static data root reaches the nested classes (xmlgen-report.md, unreachable types; the feed groups bind plain ItemRaceEntry items), so
 * they are declared here without binders, as plain subclasses like Java's.
 *
 * @author Rolandas
 */
class FeedEntries final {
public:
	class FeedFluid : public ItemRaceEntry {};

	class FeedArmor : public ItemRaceEntry {};

	class FeedThorn : public ItemRaceEntry {};

	class FeedBalaur : public ItemRaceEntry {};

	class FeedBone : public ItemRaceEntry {};

	class FeedSoul : public ItemRaceEntry {};

	class FeedExclude : public ItemRaceEntry {};

	class StinkingJunk : public ItemRaceEntry {};

	class HealthyFoodAll : public ItemRaceEntry {};

	class HealthyFoodSpicy : public ItemRaceEntry {};

	class AetherPowderBiscuit : public ItemRaceEntry {};

	class AetherCrystalBiscuit : public ItemRaceEntry {};

	class AetherGemBiscuit : public ItemRaceEntry {};

	class PoppySnack : public ItemRaceEntry {};

	class PoppySnackTasty : public ItemRaceEntry {};

	class PoppySnackNutritious : public ItemRaceEntry {};
};

} // namespace aion::gameserver::model::templates::itemgroups
