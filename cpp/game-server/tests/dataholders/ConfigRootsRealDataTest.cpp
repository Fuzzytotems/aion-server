// P4-09: the JAXB roots of config/ingameshop and config/schedule (Java InGameShopProperty.load, RiftSchedule.load, SiegeSchedules.load,
// WorldRaidSchedules.load) bind the Java tree's config files strictly. Expectations come from the files themselves (element counts through
// pugixml, independent of the binders) and a few values read from them by hand. The load functions use paths relative to the game server
// directory, so the test runs them with the Java module directory as working directory.

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/configs/ingameshop/InGameShopProperty.h"
#include "aion/gameserver/configs/schedule/RiftSchedule.h"
#include "aion/gameserver/configs/schedule/SiegeSchedules.h"
#include "aion/gameserver/configs/schedule/WorldRaidSchedules.h"
#include "aion/gameserver/model/templates/ingameshop/IGCategory.h"
#include "aion/gameserver/model/templates/rift/OpenRift.h"

namespace aion::gameserver::configs {
namespace {

const std::filesystem::path GAME_SERVER = std::filesystem::path(AION_GAMESERVER_JAVA_DIR);

size_t countChildren(const std::filesystem::path& file, const char* child) {
	pugi::xml_document document;
	if (!document.load_file(file.c_str()))
		return static_cast<size_t>(-1);
	size_t count = 0;
	for ([[maybe_unused]] pugi::xml_node node : document.document_element().children(child))
		++count;
	return count;
}

/** Runs the test body with the game server directory as working directory, restoring the previous one */
class WorkingDirectory {
public:
	explicit WorkingDirectory(const std::filesystem::path& directory) : previous(std::filesystem::current_path()) {
		std::filesystem::current_path(directory);
	}
	~WorkingDirectory() {
		std::error_code ignored;
		std::filesystem::current_path(previous, ignored);
	}
	WorkingDirectory(const WorkingDirectory&) = delete;
	WorkingDirectory& operator=(const WorkingDirectory&) = delete;

private:
	std::filesystem::path previous;
};

class ConfigRootsRealDataTest : public testing::Test {
protected:
	void SetUp() override {
		if (!std::filesystem::exists(GAME_SERVER / "config/schedule/rift_schedule.xml"))
			GTEST_SKIP() << "Java config tree not found: " << GAME_SERVER;
	}
};

TEST_F(ConfigRootsRealDataTest, InGameShopProperty) {
	WorkingDirectory directory(GAME_SERVER);
	std::unique_ptr<ingameshop::InGameShopProperty> property = ingameshop::InGameShopProperty::load();
	ASSERT_NE(property, nullptr);
	EXPECT_EQ(static_cast<size_t>(property->size()), countChildren("config/ingameshop/in_game_shop.xml", "category"));
	EXPECT_GT(property->size(), 0);
	property->clear();
	EXPECT_EQ(property->size(), 0);
}

TEST_F(ConfigRootsRealDataTest, Schedules) {
	WorkingDirectory directory(GAME_SERVER);
	std::unique_ptr<schedule::RiftSchedule> rifts = schedule::RiftSchedule::load();
	EXPECT_EQ(rifts->getRiftsList().size(), countChildren("config/schedule/rift_schedule.xml", "rift"));
	std::unique_ptr<schedule::SiegeSchedules> sieges = schedule::SiegeSchedules::load();
	EXPECT_EQ(sieges->getFortresses().size(), countChildren("config/schedule/siege_schedule.xml", "fortress"));
	EXPECT_EQ(sieges->getAgentFights().size(), countChildren("config/schedule/siege_schedule.xml", "agent_fight"));
	std::unique_ptr<schedule::WorldRaidSchedules> raids = schedule::WorldRaidSchedules::load();
	ASSERT_EQ(raids->getWorldRaidSchedules().size(), countChildren("config/schedule/world_raid_schedule.xml", "world_raid_schedule"));
	// <world_raid_schedule id="veteron_all" locations="1 2 3" min_count="2">
	const schedule::WorldRaidSchedules::WorldRaidSchedule& first = raids->getWorldRaidSchedules().at(0);
	EXPECT_EQ(first.getId(), "veteron_all");
	EXPECT_EQ(first.getMinCount(), 2);
	ASSERT_TRUE(first.getLocations().has_value());
	EXPECT_EQ(*first.getLocations(), (std::vector<int32_t>{1, 2, 3}));
}

TEST_F(ConfigRootsRealDataTest, MissingFileFailsLikeJaxbUtil) {
	// the working directory is not the game server directory: ./config/schedule/rift_schedule.xml does not exist
	WorkingDirectory directory(std::filesystem::temp_directory_path());
	try {
		static_cast<void>(schedule::RiftSchedule::load());
		FAIL() << "expected the load to fail";
	} catch (const commons::utils::Exception& e) {
		EXPECT_EQ(std::string(e.what()).rfind("Failed to unmarshal class com.aionemu.gameserver.configs.schedule.RiftSchedule from ", 0), 0u) << e.what();
	}
}

} // namespace
} // namespace aion::gameserver::configs
