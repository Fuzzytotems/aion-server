// P4-05 bodies ported with header requests pre-1 and pre-2 (docs/porting/header-requests.md): the GMService constructor over
// DataManager.SKILL_DATA (GMService.java) and ChatUtil.path(int, boolean) over DataManager.NPC_DATA (ChatUtil.java).
// Test doubles: SkillData bound from XML text and an empty NpcData (NpcData.init is not ported yet), published into DataManager.

#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>
#include <string_view>

#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/audit/GMService.h"

namespace aion::gameserver::utils {
namespace {

/** Captures the messages of one logger ("level|message" per line) while it exists */
class LogCapture {
public:
	explicit LogCapture(std::string loggerName) : name(std::move(loggerName)) {
		auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
		sink->set_pattern("%l|%v");
		commons::logging::LoggerFactory::configure(name, {.sinks = {sink}, .additive = false});
	}
	~LogCapture() { commons::logging::LoggerFactory::removeConfig(name); }
	LogCapture(const LogCapture&) = delete;
	LogCapture& operator=(const LogCapture&) = delete;

	bool contains(std::string_view text) const { return stream.str().find(text) != std::string::npos; }

private:
	std::string name;
	std::ostringstream stream;
};

TEST(GMServiceTest, ConstructorFiltersGmSkillsOfTheSkillData) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// Java: DataManager.SKILL_DATA is null before the static data is loaded; the singleton is created again on the next call
	EXPECT_THROW(audit::GMService::getInstance(), runtime::NullPointerException);

	// no template matches "group starts with GM_ or stack starts with GM_" (case-sensitive, underscore required), so the constructor warns
	xml::LoadContext context;
	dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(context, R"(<skill_data>)"
		R"(<skill_template skill_id="1" name="a" nameId="1" skilltype="MAGICAL" skillsubtype="BUFF" activation="ACTIVE" duration="0")"
		R"( stack="XGM_A" group="gm_group"/>)"
		R"(<skill_template skill_id="2" name="b" nameId="1" skilltype="MAGICAL" skillsubtype="BUFF" activation="ACTIVE" duration="0")"
		R"( stack="gm_B" group="GM"/>)"
		R"(<skill_template skill_id="3" name="c" nameId="1" skilltype="MAGICAL" skillsubtype="BUFF" activation="ACTIVE" duration="0")"
		R"( stack="NORMAL_GM_"/>)"
		R"(</skill_data>)"));
	LogCapture log("com.aionemu.gameserver.utils.audit.GMService");
	audit::GMService& service = audit::GMService::getInstance();
	EXPECT_TRUE(log.contains("warning|No GM skills found, possibly because of changed or missing skill templates."));
	EXPECT_EQ(&audit::GMService::getInstance(), &service);
	EXPECT_TRUE(service.getOnlineStaffMembers().empty());
	dataholders::DataManager::SKILL_DATA.resetForTests();
}

TEST(ChatUtilTest, PathOfAnNpcId) {
	EXPECT_THROW(ChatUtil::path(210671, true), runtime::NullPointerException) << "NPC_DATA is not published";
	dataholders::DataManager::NPC_DATA.publish(std::make_unique<dataholders::NpcData>());
	// Java: path(withIdInName ? "Unknown ID " + npcId : "Unknown ID", npcId) for an npc without a template
	EXPECT_EQ(ChatUtil::path(210671, true), "[where:Unknown ID 210671;210671]");
	EXPECT_EQ(ChatUtil::path(210671, false), "[where:Unknown ID;210671]");
	dataholders::DataManager::NPC_DATA.resetForTests();
}

} // namespace
} // namespace aion::gameserver::utils
