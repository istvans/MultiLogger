#pragma once

#include <thread>
#include <vector>

namespace LogTester
{

/**
 * @class Test
 * A test application to verify the @ref MultiLogger::Logger "Logger".
 * 
 * The constructor of the Test expects two parameters:
 *   * threadNum: number of threads to be used to simultaneously log messages
 *   * testRuns: number of random log messages to be generated and logged by each thread
 *   .
 * The test can be started with a functor call.<br/>
 * 
 * The application uses the Logger to log into the following targets:
 *   -# all_logs1.txt
 *   -# all_logs2.txt
 *   -# errors.txt (only <i>Error</i> and <i>Critical</i> messages)
 *   -# standard output
 *   -# standard error (only messages with priority above or equal to <i>Warning</i>)
 *   .
 * After the Logger finished the writing the Test reads back the written log files
 * and performs the following verifications:
 *   * the same rows in the same order have been written in all_logs1.txt and all_logs2.txt
 *   * how many rows are written into these log files (also shows how many were requested)
 *   * compares the number of rows in errors.txt against the number of requested log messages
 *     on <i>Error</i> or <i>Critical</i> level
 */
class Test
{
    using thread_storage_t = std::vector<std::thread>;

public:
    /// Constructs the Test to log threadNum_ * testRuns_ random messages.
    Test(const size_t threadNum_, const size_t testRuns_);
    ~Test();

    /// Start the test.
    void operator()();

    Test(const Test&) = delete;
    Test& operator=(const Test&) = delete;
    Test(Test&&) = delete;
    Test& operator=(Test&&) = delete;

private:
    const size_t                    _threadNum;
    const size_t                    _testRuns;
    thread_storage_t                _threads;
};

} // namespace LogTester
