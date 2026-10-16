#include <gtest/gtest.h>

#include <vector>

#include "aion/commons/utils/ClassName.h"
#include "aion/commons/utils/Exception.h"

using namespace aion::commons::utils;

namespace {

struct LocalType {};

template <typename T>
struct Holder {};

} // namespace

namespace aion::gameserver::network {
class SM_TEST {};
} // namespace aion::gameserver::network

TEST(ClassNameTest, QualifiedAndSimpleNames) {
	EXPECT_EQ(getClassName(typeid(IllegalArgumentException)), "aion::commons::utils::IllegalArgumentException");
	EXPECT_EQ(getSimpleClassName(typeid(IllegalArgumentException)), "IllegalArgumentException");
	EXPECT_EQ(getClassName(typeid(aion::gameserver::network::SM_TEST)), "aion::gameserver::network::SM_TEST");
	EXPECT_EQ(getSimpleClassName(typeid(int)), "int");
	EXPECT_FALSE(isAnonymousClass(typeid(aion::gameserver::network::SM_TEST)));
}

TEST(ClassNameTest, AnonymousNamespacesAndTemplates) {
	std::string local = getClassName(typeid(LocalType));
	EXPECT_TRUE(local.ends_with("::LocalType")) << local;
	EXPECT_EQ(getSimpleClassName(typeid(LocalType)), "LocalType");
	EXPECT_EQ(getSimpleClassName(typeid(Holder<std::vector<int>>)).substr(0, 7), "Holder<");
}

TEST(ClassNameTest, Lambdas) {
	auto lambda = [] {};
	EXPECT_TRUE(isAnonymousClass(typeid(lambda))) << getClassName(typeid(lambda));
	std::string name = getClassName(typeid(lambda));
	EXPECT_NE(name.find("Lambdas"), std::string::npos) << name; // the enclosing function is part of the name
}

TEST(ClassNameTest, NormalizeMsvcNames) {
	EXPECT_EQ(detail::normalizeTypeName("class std::vector<struct Foo,class std::allocator<struct Foo> >"),
		"std::vector<Foo,std::allocator<Foo> >");
	EXPECT_EQ(detail::normalizeTypeName("enum Color"), "Color");
	EXPECT_EQ(detail::normalizeTypeName("struct Subclass"), "Subclass"); // "class" inside an identifier is kept
	EXPECT_EQ(detail::simpleTypeName("`void __cdecl Foo::bar(void)'::`2'::<lambda_1>"), "<lambda_1>");
	EXPECT_EQ(detail::simpleTypeName("a::b<c::d>"), "b<c::d>");
	EXPECT_EQ(detail::simpleTypeName("Foo::bar()::{lambda()#1}"), "{lambda()#1}");
	EXPECT_EQ(detail::simpleTypeName("Plain"), "Plain");
}
