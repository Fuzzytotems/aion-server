#include <gtest/gtest.h>

#include "aion/commons/database/ConnectionProperties.h"
#include "aion/commons/database/SQLException.h"

using namespace aion::commons::database;

TEST(ConnectionPropertiesTest, ParsesLoginServerUrl) {
	auto props = ConnectionProperties::parse("jdbc:mysql://localhost:3306/aion_ls?serverTimezone=&characterEncoding=UTF-8");
	EXPECT_EQ(props.host, "localhost");
	EXPECT_EQ(props.port, 3306);
	EXPECT_EQ(props.database, "aion_ls");
	EXPECT_EQ(props.characterSet, "utf8mb4");
	EXPECT_EQ(props.timeZone.getId(), ConnectionTimeZone::systemDefault().getId());
	EXPECT_EQ(props.sslMode, SslMode::PREFERRED);
	EXPECT_EQ(props.zeroDateTimeBehavior, ZeroDateTimeBehavior::EXCEPTION);
	EXPECT_FALSE(props.useAffectedRows);
	ASSERT_EQ(props.parameters.size(), 2u);
	EXPECT_EQ(props.parameters[0], (std::pair<std::string, std::string>{"serverTimezone", ""}));
	EXPECT_EQ(props.getParameter("CHARACTERENCODING"), "UTF-8");
	EXPECT_FALSE(props.getParameter("user"));
}

TEST(ConnectionPropertiesTest, Defaults) {
	auto props = ConnectionProperties::parse("jdbc:mariadb://");
	EXPECT_EQ(props.host, "localhost");
	EXPECT_EQ(props.port, 3306);
	EXPECT_EQ(props.database, "");
	props = ConnectionProperties::parse("jdbc:mysql://db.example.com");
	EXPECT_EQ(props.host, "db.example.com");
	EXPECT_EQ(props.port, 3306);
	props = ConnectionProperties::parse("JDBC:MySQL://:3307/");
	EXPECT_EQ(props.host, "localhost");
	EXPECT_EQ(props.port, 3307);
	EXPECT_EQ(props.database, "");
}

TEST(ConnectionPropertiesTest, UserInfoIpv6AndMultipleHosts) {
	auto props = ConnectionProperties::parse("jdbc:mysql://us%40er:p%3Ass@[::1]:3310,other:3306/my%20db?useSSL=false");
	EXPECT_EQ(props.user, "us@er");
	EXPECT_EQ(props.password, "p:ss");
	EXPECT_EQ(props.host, "::1");
	EXPECT_EQ(props.port, 3310);
	EXPECT_EQ(props.database, "my db");
	EXPECT_EQ(props.sslMode, SslMode::DISABLED);
}

TEST(ConnectionPropertiesTest, CredentialsOverrideUrl) {
	auto props = ConnectionProperties::parse("jdbc:mysql://localhost/db?user=a&password=b", "root", "");
	EXPECT_EQ(props.user, "root");
	EXPECT_EQ(props.password, "b"); // empty strings count as "not configured"
}

TEST(ConnectionPropertiesTest, SupportedParameters) {
	auto props = ConnectionProperties::parse("jdbc:mysql://h/d?connectTimeout=1500&socketTimeout=0&serverTimezone=%2B02:00&zeroDateTimeBehavior=convertToNull"
																					 "&useAffectedRows=YES&sslMode=verify_identity&unknownThing=1&flag");
	EXPECT_EQ(props.connectTimeout, std::chrono::milliseconds(1500));
	EXPECT_EQ(props.socketTimeout, std::chrono::milliseconds(0));
	EXPECT_EQ(props.timeZone.getId(), "+02:00");
	EXPECT_EQ(props.zeroDateTimeBehavior, ZeroDateTimeBehavior::CONVERT_TO_NULL);
	EXPECT_TRUE(props.useAffectedRows);
	EXPECT_EQ(props.sslMode, SslMode::VERIFY_IDENTITY);
	EXPECT_EQ(props.getParameter("flag"), "");

	EXPECT_EQ(ConnectionProperties::parse("jdbc:mysql://h/d?requireSSL=true").sslMode, SslMode::REQUIRED);
	EXPECT_EQ(ConnectionProperties::parse("jdbc:mysql://h/d?useSSL=true&verifyServerCertificate=true").sslMode, SslMode::VERIFY_CA);
	EXPECT_EQ(ConnectionProperties::parse("jdbc:mysql://h/d?useSSL=false&sslMode=REQUIRED").sslMode, SslMode::REQUIRED);
	EXPECT_EQ(ConnectionProperties::parse("jdbc:mysql://h/d?zeroDateTimeBehavior=ROUND").zeroDateTimeBehavior, ZeroDateTimeBehavior::ROUND);
	EXPECT_EQ(ConnectionProperties::parse("jdbc:mysql://h/d?characterEncoding=latin1").characterSet, "utf8mb4");
}

TEST(ConnectionPropertiesTest, RejectsInvalidUrls) {
	for (const char* url : {"", "mysql://localhost/db", "jdbc:postgresql://localhost/db", "jdbc:mysql://localhost:abc/db", "jdbc:mysql://localhost:0/db",
			 "jdbc:mysql://localhost:70000/db", "jdbc:mysql://[::1/db", "jdbc:mysql://h/d?x=%zz"}) {
		EXPECT_THROW(ConnectionProperties::parse(url), SQLException) << url;
	}
	try {
		ConnectionProperties::parse("jdbc:mysql://h/d?connectTimeout=soon");
		FAIL();
	} catch (const SQLException& e) {
		EXPECT_EQ(e.getSQLState(), "01S00");
		EXPECT_EQ(std::string(e.what()), "The connection property 'connectTimeout' only accepts integer values. The value 'soon' can not be converted to an integer.");
	}
	EXPECT_THROW(ConnectionProperties::parse("jdbc:mysql://h/d?useSSL=maybe"), SQLException);
	EXPECT_THROW(ConnectionProperties::parse("jdbc:mysql://h/d?sslMode=sometimes"), SQLException);
	EXPECT_THROW(ConnectionProperties::parse("jdbc:mysql://h/d?serverTimezone=Nowhere/Nothing"), SQLException);
}
