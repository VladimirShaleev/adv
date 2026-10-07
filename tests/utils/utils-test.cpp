#include <gtest/gtest.h>
#include <utils/utils.hpp>

TEST(Utils, GetModuleName) {
    EXPECT_EQ(utils::getModuleName("src/my-module.adv"), "my_module");
    EXPECT_EQ(utils::getModuleName("src/My.Module-name.adv"), "my_module_name");
    EXPECT_EQ(utils::getModuleName("src/my-_mo__dule__.adv"), "my_mo_dule");
    EXPECT_EQ(utils::getModuleName("src/module.tar.adv"), "module_tar");
    EXPECT_EQ(utils::getModuleName(""), "");
    EXPECT_EQ(utils::getModuleName("src/"), "");
}
