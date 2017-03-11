#include "Tester.h"

#include <MultiLogger/Log.h>

#ifdef _WIN32
# include <conio.h>
#endif

#include <iostream>
#include <string>
#include <random>
#include <sstream>
#include <fstream>

namespace LogTester
{

//=============================================================================
//=============================================================================

namespace
{

int generic_exit(const int error_code_)
{
#ifdef _WIN32
    std::cout << "Press any key to continue...";
    while (!_kbhit()) {}
#endif

    return error_code_;
}

//=============================================================================

struct Person
{
    std::string         _firstName;
    std::string         _lastName;
    size_t              _age;
};

template <class Ostream>
Ostream& operator<<(Ostream& lhs_, const Person& rhs_)
{
    return lhs_ << '[' << rhs_._firstName << ' ' << rhs_._lastName << ':' << rhs_._age << ']';
}

//=============================================================================

template <class Engine, class Distribution>
std::string randomTextGen(Engine& engine_, Distribution&& LenDist_)
{
    Distribution charDist{97, 122};
    const auto len = LenDist_(engine_);
    std::ostringstream ost;
    for (auto i = 0; i < len; ++i) {
        ost << static_cast<char>(charDist(engine_));
    }
    return ost.str();
}

}

//=============================================================================
//=============================================================================

Test::Test(const size_t threadNum_, const size_t testRuns_)
    : _threadNum{threadNum_}
    , _testRuns{testRuns_}
{
    _threads.reserve(_threadNum);
    
    MultiLogger::globalLogger().category("tester");
    MultiLogger::globalLogger().threshold(MultiLogger::Priority::Debug);

    const auto allLogs1 = std::string{"all_logs1.txt"};
    const auto allLogs2 = std::string{"all_logs2.txt"};
    const auto errorLogs = std::string{"errors.txt"};
    const auto threadNumCpy = _threadNum;
    const auto testRunsCpy = _testRuns;

    MultiLogger::globalLogger().verifyCB([threadNumCpy, testRunsCpy, allLogs1, allLogs2, errorLogs](const size_t requestedErrors_) {
        std::cout << "total requests: " << (threadNumCpy * testRunsCpy) << std::endl;
        
        std::cout << "comparing the contents of " << allLogs1 << " and " << allLogs2 << ": ";
        {
            std::fstream l1{allLogs1, std::ios_base::in};
            std::fstream l2{allLogs2, std::ios_base::in};
            if (!(l1 && l2)) {
                throw std::runtime_error("cannot open files for comparison!");
            }
            std::string line1, line2;
            auto match = 0ul;
            auto count = 0ul;
            while (std::getline(l1, line1) && std::getline(l2, line2)) {
                if (line1 == line2) {
                    ++match;
                }
                ++count;
            }
            std::cout << "logged: " << count << " matched: " << match << std::endl;
        }

        std::cout << "verifying error threshold log file " << errorLogs << ": ";
        {
            std::fstream e{errorLogs, std::ios_base::in};
            if (!e) {
                throw std::runtime_error("cannot verify error threshold log file!");
            }
            std::string line;
            auto count = 0ul;
            while (std::getline(e, line)) {
                ++count;
            }
            std::cout << "requested: " << requestedErrors_ << " logged: " << count << std::endl;
        }

        generic_exit(0);
    });

    MultiLogger::globalLogger().addDest(allLogs1, MultiLogger::cpp14::imp::make_unique<MultiLogger::FileDest>(allLogs1));
    MultiLogger::globalLogger().addDest(allLogs2, MultiLogger::cpp14::imp::make_unique<MultiLogger::FileDest>(allLogs2));
    MultiLogger::globalLogger().addDest(errorLogs, MultiLogger::globalLogger().errorThreshold(), MultiLogger::cpp14::imp::make_unique<MultiLogger::FileDest>(errorLogs));
    MultiLogger::globalLogger().addDest("stdout", MultiLogger::cpp14::imp::make_unique<MultiLogger::StdOutDest>());
    MultiLogger::globalLogger().addDest("stderr", MultiLogger::Priority::Warning, MultiLogger::cpp14::imp::make_unique<MultiLogger::StdErrDest>());
}

Test::~Test()
{
    for (auto& thread : _threads) {
        thread.join();
    }
}

void Test::operator()()
{
    for (auto i = 0ul; i < _threadNum; ++i) {
        _threads.emplace_back([this]() {
            std::random_device rd;
            std::mt19937 mt{rd()};
            std::uniform_real_distribution<double> readDist{1.0, 10.0};
            std::uniform_int_distribution<int> intDist{10000, 1000000000};
            std::uniform_int_distribution<size_t> ageDist{1, 100};
            std::uniform_int_distribution<size_t> priorityDist{0, (static_cast<size_t>(MultiLogger::Priority::__Size) - 1)};
            std::uniform_int_distribution<size_t> workDist{100, 500};

            for (auto i = 0ul; i < _testRuns; ++i) {
                // random work simulation
                std::this_thread::sleep_for(std::chrono::milliseconds{workDist(mt)});

                auto randPerson = Person{randomTextGen(mt, std::uniform_int_distribution<int>{4, 8})
                    , randomTextGen(mt, std::uniform_int_distribution<int>{8, 16})
                    , ageDist(mt)};
                MRLogG(static_cast<MultiLogger::Priority>(priorityDist(mt)),
                    i << ": Let's log some random text: " << randomTextGen(mt, std::uniform_int_distribution<int>{1, 20}) <<
                    " then a random number " << intDist(mt) <<
                    " then another random number " << readDist(mt) <<
                    " then a user class instantiated with random values " <<
                    std::move(randPerson));
            }
        });
    }
}

}
