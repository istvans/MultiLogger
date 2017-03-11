#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../../../lib/MultiLogger/Log.cpp"

#include <fstream>
#include <cstdio>

TEST_CASE("Debug logger", "[debugger]")
{
    const std::string testFile{"test1"};
    const std::string category{"debugger"};
    {
        MultiLogger::Logger log{MultiLogger::Priority::Debug, category};
        log.addDest(testFile, MultiLogger::cpp14::imp::make_unique<MultiLogger::FileDest>(testFile));
        MRLogL(log, MultiLogger::Priority::Debug, testFile);
    }
    {
        std::fstream t{testFile, std::ios_base::in};
        CHECK(static_cast<bool>(t));
        std::string line;
        CHECK(static_cast<bool>(std::getline(t, line)));
        REQUIRE_THAT(line,
            Catch::Matchers::Contains(category) && Catch::Matchers::Contains("Debug: ") && Catch::Matchers::Contains(testFile));
    }
    std::remove(testFile.c_str());
}

TEST_CASE("Disable destination", "[disable-dest]")
{
    const std::string testFile{"test2"};
    const std::string category{"debugger"};
    {
        MultiLogger::Logger log{MultiLogger::Priority::Debug, category};
        log.addDest(testFile, MultiLogger::cpp14::imp::make_unique<MultiLogger::FileDest>(testFile));
        log.permitDest(testFile, false);
        CHECK_FALSE(log.logging(testFile));
        MRLogL(log, MultiLogger::Priority::Debug, testFile);
    }
    {
        std::fstream t{testFile, std::ios_base::in};
        CHECK(static_cast<bool>(t));
        std::string line;
        CHECK_FALSE(static_cast<bool>(std::getline(t, line)));
    }
    std::remove(testFile.c_str());
}

TEST_CASE("Disable then enable destination", "[disable-enable-dest]")
{
    const std::string testFile{"test3"};
    const std::string category{"debugger"};
    {
        MultiLogger::Logger log{MultiLogger::Priority::Debug, category};
        log.addDest(testFile, MultiLogger::cpp14::imp::make_unique<MultiLogger::FileDest>(testFile));
        log.permitDest(testFile, false);
        CHECK_FALSE(log.logging(testFile));
        MRLogL(log, MultiLogger::Priority::Debug, testFile);
        log.permitDest(testFile, true);
        CHECK(log.logging(testFile));
        MRLogL(log, MultiLogger::Priority::Debug, testFile);
    }
    {
        std::fstream t{testFile, std::ios_base::in};
        CHECK(static_cast<bool>(t));
        std::string line;
        CHECK(static_cast<bool>(std::getline(t, line)));
        REQUIRE_THAT(line,
            Catch::Matchers::Contains(category) && Catch::Matchers::Contains("Debug: ") && Catch::Matchers::Contains(testFile));
    }
    std::remove(testFile.c_str());
}

TEST_CASE("Set category", "[set-category]")
{
    const std::string testFile{"test4"};
    std::string category{"debugger"};
    {
        MultiLogger::Logger log{MultiLogger::Priority::Debug, category};
        log.addDest(testFile, MultiLogger::cpp14::imp::make_unique<MultiLogger::FileDest>(testFile));
        REQUIRE_THAT(log.category(), Catch::Matchers::Equals(category));
        category = "debuggger";
        log.category(category);
        REQUIRE_THAT(log.category(), Catch::Matchers::Equals(category));
        MRLogL(log, MultiLogger::Priority::Debug, testFile);
    }
    {
        std::fstream t{testFile, std::ios_base::in};
        CHECK(static_cast<bool>(t));
        std::string line;
        CHECK(static_cast<bool>(std::getline(t, line)));
        REQUIRE_THAT(line,
            Catch::Matchers::Contains(category) && Catch::Matchers::Contains("Debug: ") && Catch::Matchers::Contains(testFile));
    }
    std::remove(testFile.c_str());
}

TEST_CASE("Log below threshold", "[below-thresh]")
{
    const std::string testFile{"test5"};
    std::string category{"info"};
    {
        MultiLogger::Logger log{MultiLogger::Priority::Info, category};
        log.addDest(testFile, MultiLogger::cpp14::imp::make_unique<MultiLogger::FileDest>(testFile));
        MRLogDebugL(log, testFile);
    }
    {
        std::fstream t{testFile, std::ios_base::in};
        CHECK(static_cast<bool>(t));
        std::string line;
        CHECK_FALSE(static_cast<bool>(std::getline(t, line)));
    }
    std::remove(testFile.c_str());
}

TEST_CASE("Log on threshold", "[on-thresh]")
{
    const std::string testFile{"test6"};
    std::string category{"info"};
    {
        MultiLogger::Logger log{MultiLogger::Priority::Info, category};
        log.addDest(testFile, MultiLogger::cpp14::imp::make_unique<MultiLogger::FileDest>(testFile));
        MRLogInfoL(log, testFile);
    }
    {
        std::fstream t{testFile, std::ios_base::in};
        CHECK(static_cast<bool>(t));
        std::string line;
        CHECK(static_cast<bool>(std::getline(t, line)));
        REQUIRE_THAT(line,
            Catch::Matchers::Contains(category) && Catch::Matchers::Contains("Info: ") && Catch::Matchers::Contains(testFile));
    }
    std::remove(testFile.c_str());
}

TEST_CASE("Log above threshold", "[above-thresh]")
{
    const std::string testFile{"test7"};
    std::string category{"info"};
    {
        MultiLogger::Logger log{MultiLogger::Priority::Info, category};
        log.addDest(testFile, MultiLogger::cpp14::imp::make_unique<MultiLogger::FileDest>(testFile));
        MRLogWarningL(log, testFile);
    }
    {
        std::fstream t{testFile, std::ios_base::in};
        CHECK(static_cast<bool>(t));
        std::string line;
        CHECK(static_cast<bool>(std::getline(t, line)));
        REQUIRE_THAT(line,
            Catch::Matchers::Contains(category) && Catch::Matchers::Contains("Warning: ") && Catch::Matchers::Contains(testFile));
    }
    std::remove(testFile.c_str());
}

TEST_CASE("Set threshold", "[set-thresh]")
{
    const std::string testFile{"test8"};
    std::string category{"thresh"};
    {
        MultiLogger::Logger log{MultiLogger::Priority::Error, category};
        log.addDest(testFile, MultiLogger::cpp14::imp::make_unique<MultiLogger::FileDest>(testFile));
        MRLogInfoL(log, testFile);
        MRLogWarningL(log, testFile);
        MRLogErrorL(log, testFile);
        log.threshold(MultiLogger::Priority::Info);
        log.threshold(testFile, MultiLogger::Priority::Info);
        MRLogInfoL(log, testFile);
        MRLogWarningL(log, testFile);
        MRLogErrorL(log, testFile);
    }
    {
        std::fstream t{testFile, std::ios_base::in};
        CHECK(static_cast<bool>(t));
        std::string line;
        CHECK(static_cast<bool>(std::getline(t, line)));
        REQUIRE_THAT(line,
            Catch::Matchers::Contains(category) && Catch::Matchers::Contains("Error: ") && Catch::Matchers::Contains(testFile));
        CHECK(static_cast<bool>(std::getline(t, line)));
        REQUIRE_THAT(line,
            Catch::Matchers::Contains(category) && Catch::Matchers::Contains("Info: ") && Catch::Matchers::Contains(testFile));
        CHECK(static_cast<bool>(std::getline(t, line)));
        REQUIRE_THAT(line,
            Catch::Matchers::Contains(category) && Catch::Matchers::Contains("Warning: ") && Catch::Matchers::Contains(testFile));
        CHECK(static_cast<bool>(std::getline(t, line)));
        REQUIRE_THAT(line,
            Catch::Matchers::Contains(category) && Catch::Matchers::Contains("Error: ") && Catch::Matchers::Contains(testFile));
    }
    std::remove(testFile.c_str());
}
