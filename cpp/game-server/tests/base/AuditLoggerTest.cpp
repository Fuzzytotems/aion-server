// AuditLogger.log with a null player (header request 5a-pre-4, docs/porting/header-requests.md): CM_PING passes the connection's active
// player, which is null before enter world. Expectations from AuditLogger.java with player == null: AutoBan.punishment(null) throws
// NullPointerException, the audit line is "null " + message.

#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>
#include <string_view>

#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PunishmentConfig.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::utils::audit {
namespace {

using configs::main::LoggingConfig;
using configs::main::PunishmentConfig;
using model::gameobjects::player::Player;

class AuditLogCapture {
public:
	AuditLogCapture() {
		auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
		sink->set_pattern("%l|%v");
		commons::logging::LoggerFactory::configure("AUDIT_LOG", {.sinks = {sink}, .additive = false});
	}
	~AuditLogCapture() { commons::logging::LoggerFactory::removeConfig("AUDIT_LOG"); }
	AuditLogCapture(const AuditLogCapture&) = delete;
	AuditLogCapture& operator=(const AuditLogCapture&) = delete;

	std::string text() const { return stream.str(); }

private:
	std::ostringstream stream;
};

class AuditLoggerTest : public testing::Test {
protected:
	void SetUp() override {
		punishment = PunishmentConfig::PUNISHMENT_ENABLE.load();
		audit = LoggingConfig::LOG_AUDIT.load();
	}
	void TearDown() override {
		PunishmentConfig::PUNISHMENT_ENABLE.store(punishment);
		LoggingConfig::LOG_AUDIT.store(audit);
	}

	bool punishment = false;
	bool audit = false;
};

TEST_F(AuditLoggerTest, NullPlayerWritesTheJavaNullLine) {
	PunishmentConfig::PUNISHMENT_ENABLE.store(false);
	LoggingConfig::LOG_AUDIT.store(true);
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	AuditLogCapture capture;
	try {
		AuditLogger::log(runtime::Ptr<Player>(), "possibly using time/speed hack (client ping interval: 1000/180000)");
	} catch (const runtime::NullPointerException&) {
		// the staff member loop needs GMService, whose constructor reads DataManager.SKILL_DATA (not published in this test); the audit line
		// is written before
	}
	std::string text = capture.text();
	while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
		text.pop_back(); // the sink's platform line ending
	EXPECT_EQ(text, "info|null possibly using time/speed hack (client ping interval: 1000/180000)");
}

TEST_F(AuditLoggerTest, NullPlayerWithPunishmentsThrowsBeforeLogging) {
	PunishmentConfig::PUNISHMENT_ENABLE.store(true);
	LoggingConfig::LOG_AUDIT.store(true);
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	AuditLogCapture capture;
	EXPECT_THROW(AuditLogger::log(runtime::Ptr<Player>(), "kicking player"), runtime::NullPointerException);
	EXPECT_EQ(capture.text(), "");
}

TEST_F(AuditLoggerTest, NullPlayerWithoutAuditLogWritesNothing) {
	PunishmentConfig::PUNISHMENT_ENABLE.store(false);
	LoggingConfig::LOG_AUDIT.store(false);
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	AuditLogCapture capture;
	try {
		AuditLogger::log(runtime::Ptr<Player>(), "message");
	} catch (const runtime::NullPointerException&) {
		// GMService without skill data, see above
	}
	EXPECT_EQ(capture.text(), "");
}

} // namespace
} // namespace aion::gameserver::utils::audit
