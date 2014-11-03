#ifndef PLOG_TEST_H
#define PLOG_TEST_H

#include "test.h"

TEST(PLogTest, WriteLog) {
    std::fstream file("/tmp/a/file/that/does/not/exist.txt", std::fstream::in);
    if (file.is_open()) {
        // We dont expect to open file
        FAIL();
    }
    PLOG(INFO) << "This is plog";
    std::string expected = BUILD_STR(getDate() << " This is plog: No such file or directory [2]\n");
    std::string actual = tail(1);
    EXPECT_EQ(expected, actual);
}

TEST(PLogTest, DebugVersionLogs) {
    // Test enabled
    #undef _ELPP_DEBUG_LOG
    #define _ELPP_DEBUG_LOG 0

    std::string currentTail = tail(1);

    DPLOG(INFO) << "No DPLOG should be resolved";
    EXPECT_EQ(currentTail, tail(1));
    DPLOG_IF(true, INFO) << "No DPLOG_IF should be resolved";
    EXPECT_EQ(currentTail, tail(1));
    DCPLOG(INFO, "performance") << "No DPLOG should be resolved";
    EXPECT_EQ(currentTail, tail(1));
    DCPLOG(INFO, "performance") << "No DPLOG should be resolved";
    EXPECT_EQ(currentTail, tail(1));
    // Reset
    #undef _ELPP_DEBUG_LOG
    #define _ELPP_DEBUG_LOG 1

    // Now test again
    DPLOG(INFO) << "DPLOG should be resolved";
    std::string expected = BUILD_STR(getDate() << " DPLOG should be resolved: Success [0]\n");
    EXPECT_EQ(expected, tail(1));
    DPLOG_IF(true, INFO) << "DPLOG_IF should be resolved";
    expected = BUILD_STR(getDate() << " DPLOG_IF should be resolved: Success [0]\n");
    EXPECT_EQ(expected, tail(1));
    DCPLOG(INFO, "default") << "DCPLOG should be resolved";
    expected = BUILD_STR(getDate() << " DCPLOG should be resolved: Success [0]\n");
    EXPECT_EQ(expected, tail(1));
    DCPLOG(INFO, "default") << "DCPLOG should be resolved";
    expected = BUILD_STR(getDate() << " DCPLOG should be resolved: Success [0]\n");
    EXPECT_EQ(expected, tail(1));
}

#endif // PLOG_TEST_H
