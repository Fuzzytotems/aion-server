#include "aion/commons/configs/DatabaseConfig.h"

#include <sstream>

#include <gtest/gtest.h>

#include "aion/commons/configs/CommonsConfig.h"
#include "aion/commons/configuration/ConfigurableProcessor.h"

using namespace aion::commons;
using namespace aion::commons::configs;
using configuration::ConfigurableProcessor;
using configuration::Properties;

namespace {

class DatabaseConfigTest : public testing::Test {
protected:
	void SetUp() override { reset(); }
	void TearDown() override { reset(); }

	static void reset() {
		DatabaseConfig::DATABASE_URL.clear();
		DatabaseConfig::DATABASE_USER.clear();
		DatabaseConfig::DATABASE_PASSWORD.clear();
		DatabaseConfig::DATABASE_CONNECTIONS_MAX = 0;
		DatabaseConfig::DATABASE_TIMEOUT = 0;
	}
};

} // namespace

TEST_F(DatabaseConfigTest, Defaults) {
	std::set<std::string> unused = ConfigurableProcessor::process(Properties(), {&DatabaseConfig::bind});
	EXPECT_TRUE(unused.empty());
	EXPECT_EQ(DatabaseConfig::DATABASE_URL, ""); // no default value: unchanged
	EXPECT_EQ(DatabaseConfig::DATABASE_USER, "");
	EXPECT_EQ(DatabaseConfig::DATABASE_PASSWORD, "");
	EXPECT_EQ(DatabaseConfig::DATABASE_CONNECTIONS_MAX, 5);
	EXPECT_EQ(DatabaseConfig::DATABASE_TIMEOUT, 5000);
}

TEST_F(DatabaseConfigTest, Values) {
	Properties p;
	std::istringstream in("database.url = jdbc:mysql://localhost:3306/aion_ls?serverTimezone=&characterEncoding=UTF-8\n"
	                      "database.user = root\n"
	                      "database.password = \n"
	                      "database.connectionpool.connections.max = 10\n"
	                      "database.connectionpool.timeout = 0x10\n"
	                      "unknown = 1\n");
	p.load(in);
	std::set<std::string> unused = ConfigurableProcessor::process(p, {&DatabaseConfig::bind});
	EXPECT_EQ(unused, std::set<std::string>{"unknown"});
	EXPECT_EQ(DatabaseConfig::DATABASE_URL, "jdbc:mysql://localhost:3306/aion_ls?serverTimezone=&characterEncoding=UTF-8");
	EXPECT_EQ(DatabaseConfig::DATABASE_USER, "root");
	EXPECT_EQ(DatabaseConfig::DATABASE_PASSWORD, "");
	EXPECT_EQ(DatabaseConfig::DATABASE_CONNECTIONS_MAX, 10);
	EXPECT_EQ(DatabaseConfig::DATABASE_TIMEOUT, 16);
}

TEST_F(DatabaseConfigTest, InvalidValue) {
	Properties p;
	p.setProperty("database.connectionpool.timeout", "5s");
	EXPECT_THROW(ConfigurableProcessor::process(p, {&DatabaseConfig::bind}), configuration::TransformationException);
	EXPECT_EQ(DatabaseConfig::DATABASE_CONNECTIONS_MAX, 5); // bound before the failing field
}
