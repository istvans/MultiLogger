#include "Log.h"

#include <time.h>

#include <vector>
#include <queue>
#include <chrono>
#include <utility>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <iostream>
#include <atomic>
#include <condition_variable>
#include <algorithm>

#ifdef _WIN32
/// thread-safe cross-platform gmtime
struct tm* gmtime_r(const time_t* time_, struct tm* result_)
{
    return (0 == gmtime_s(result_, time_)) ? result_ : 0;
}
#endif

namespace MultiLogger
{

/// Wrapper class with meaningful member variables.
/// Used instead of a std::tuple for readability.
struct LogTarget
{
    LogTarget(const std::string& name_
        , LogDest::ptr_t&& dest_
        , const Priority threshold_
        , const bool enabled_)
        : _name{name_}
        , _dest{std::move(dest_)}
        , _threshold{threshold_}
        , _enabled{enabled_}
    {}
    ~LogTarget()
    {}

    LogTarget(const LogTarget&) = delete;
    LogTarget& operator=(const LogTarget&) = delete;
    LogTarget(LogTarget&&) = default;
    LogTarget& operator=(LogTarget&&) = default;

    const std::string       _name;
    LogDest::ptr_t          _dest;
    Priority                _threshold;
    bool                    _enabled;
};

/**
 * This struct is the actual Logger implementation.
 * 
 * The main goal of this implementation is to preserve the chronological
 * order of the messages across all destinations regardless how many we have.
 * To achieve this a priority queue is used which can always return the
 * earliest log message as its top element.
 * 
 * A vector is used to store the log destinations which needs to be
 * derived from LogDest.
 * 
 * There are some known shortcoming with the current implementation:
 *   * queuing is not the most efficient way to handle concurrent writers
 *   * text-based logging wastes resources on formatting probably never checked
 *     log-lines (see related TODO)
 *   * if the user application crashes we possibly lose the latest, most important
 *     log messages (see related TODO)
 * 
 * @todo Add mechanism to prevent losing messages even if the user application crashes.
 * @todo Add binary logging where string formatting can be omitted all together. We format
 *       every line now while usually only a few log lines are important/checked. The binary
 *       format is more concise. It can be written and read faster and consumes less space.
 *       With adequate facilities to convert it to human readable format and to allow quick
 *       searching or even issue reporting it can and should replace text-base logging.
 */
struct Logger::Impl
{
    using time_point_t = std::chrono::system_clock::time_point;
    using queue_element_t = std::pair<time_point_t, std::pair<Priority, std::string>>;
    using priority_queue_t = std::priority_queue<queue_element_t
        , std::vector<queue_element_t>
        , std::greater<queue_element_t>>;
    using dests_t = std::vector<LogTarget>;

    Impl(const Priority globalThreshold_
        , const std::string& category_)
        : _globalThreshold{globalThreshold_}
        , _category{category_}
    {
        _logger = std::thread{[this]() {
            while (true) {
                std::unique_lock<std::mutex> ulw{_writeMutex};
                
                if (_queue.empty() && !_log) {
                    break;
                }
                
                _writeCond.wait_for(ulw, _maxWait, [this]() { return !_queue.empty(); });
                priority_queue_t localQueue;
                localQueue.swap(_queue);
                ulw.unlock();

                std::lock_guard<std::mutex> lgd{_destMutex};
                while (!localQueue.empty()) {
                    const auto msg = localQueue.top();
                    localQueue.pop();
                    for (auto& target : _dests) {
                        if (target._enabled && !(msg.second.first < target._threshold) && target._dest) {
                            target._dest->write(msg.second.second);
                        }
                    }
                }
            }
        }};
    }
    ~Impl()
    {
        _log = false;
        _logger.join();

        if (_verifCB) {
            for (auto& target : _dests) {
                if (target._dest) {
                    target._dest->flush();
                }
            }
            _verifCB(_requestedErrors.load());
        }
    }

    void log(std::string&& message_
        , const Priority pri_
        , const char* function_
        , const char* file_
        , int line_
        , const std::thread::id threadId_)
    {
        {
            std::lock_guard<std::mutex> lg{_writeMutex};
            if (pri_ < _globalThreshold) {
                return;
            }

            if (_verifCB && !(pri_ < _errorThreshold)) {
                ++_requestedErrors;
            }
        }

        /// @todo Every log call starts up a new thread to push into the queue.
        ///       Using a thread-pool would prevent starting up too many threads.
        auto now = std::chrono::system_clock::now();
        std::thread{[this, now, pri_, function_, file_, line_, threadId_](std::string&& message_) {
            std::ostringstream formattedMsg;
            auto time = std::chrono::system_clock::to_time_t(now);
            struct tm tm;
            if (!gmtime_r(&time, &tm)) {
                throw std::runtime_error("cannot get time for logging!");
            }
            const auto total_nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
            const auto total_seconds_in_nanos = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count() * 1000 * 1000 * 1000;
            const auto nanos = total_nanos - total_seconds_in_nanos;

            /// @todo Accessing _category here is not thread-safe, but always locking to build the message sounds too expansive
            ///       to make the 0.001% case thread-safe. A better solution needed.
            formattedMsg << std::put_time(&tm, "%b %e %T") << '.' << nanos << ' ' << threadId_ << ' ' << _category << ' ' << function_ << ' ' << pri_ <<
                ": " << std::move(message_) << " (" << file_ << ':' << line_ << ")\n";
            
            {
                std::lock_guard<std::mutex> lg{_writeMutex};
                _queue.push(std::make_pair(std::move(now), std::make_pair(pri_, formattedMsg.str())));
            }
            _writeCond.notify_one();
        }
        , std::move(message_)}.detach();
    }

    void category(const std::string& category_)
    {
        std::lock_guard<std::mutex> lg{_writeMutex};
        _category = category_;
    }

    void addDest(const std::string& name_, LogDest::ptr_t&& dest_)
    {
        std::lock_guard<std::mutex> lg{_destMutex};
        _dests.emplace_back(name_, std::move(dest_), _globalThreshold, true);
    }

    void addDest(const std::string& name_, const Priority thresHold_, LogDest::ptr_t&& dest_)
    {
        std::lock_guard<std::mutex> lg{_destMutex};
        _dests.emplace_back(name_, std::move(dest_), thresHold_, true);
    }

    void permitDest(const std::string& name_, const bool enable_)
    {
        std::lock_guard<std::mutex> lg{_destMutex};
        const auto it = std::find_if(_dests.begin(), _dests.end(), [&name_](const dests_t::value_type& target_) {
            return name_ == target_._name;
        });
        if (it != _dests.end()) {
            it->_enabled = enable_;
        }
    }

    void threshold(const Priority globalThreshold_)
    {
        /// @todo When we set the global threshold it won't affect the destinations'
        ///       thresholds which is good because they can remain independent and
        ///       bad because after lowering the global threshold, all the destination
        ///       thresholds need to be lowered to where we want to introduce this new
        ///       setting. It is quiet inconvenient.<br/>
        ///       There should be a setting for every individual LogDest to specify
        ///       whether it follows the global threshold or not.
        std::lock_guard<std::mutex> lg{_writeMutex};
        _globalThreshold = globalThreshold_;
    }

    void threshold(const std::string& destName_, const Priority threshold_)
    {
        std::lock_guard<std::mutex> lg{_destMutex};
        const auto it = std::find_if(_dests.begin(), _dests.end(), [&destName_](const dests_t::value_type& target_) {
            return destName_ == target_._name;
        });
        if (it != _dests.end()) {
            it->_threshold = threshold_;
        }
    }

    void verifyCB(const verif_cb_t& cb_)
    {
        _verifCB = cb_;
    }

    void errorThreshold(const Priority errorThreshold_)
    {
        std::lock_guard<std::mutex> lg{_writeMutex};
        _errorThreshold = errorThreshold_;
    }

    const std::string& category() const
    {
        std::lock_guard<std::mutex> lg{_writeMutex};
        return _category;
    }

    Priority errorThreshold() const
    {
        std::lock_guard<std::mutex> lg{_writeMutex};
        return _errorThreshold;
    }

    bool logging(const Priority pri_) const
    {
        std::lock_guard<std::mutex> lg{_writeMutex};
        return !(pri_ < _globalThreshold);
    }

    bool logging(const std::string& destName_) const
    {
        std::lock_guard<std::mutex> lg{_destMutex};
        const auto cit = std::find_if(_dests.cbegin(), _dests.cend(), [&destName_](const dests_t::value_type& target_) {
            return destName_ == target_._name;
        });
        return (cit == _dests.cend()) ? false : cit->_enabled;
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    std::string                     _category;
    Priority                        _globalThreshold;
    dests_t                         _dests;
    priority_queue_t                _queue;

    mutable std::mutex              _writeMutex;
    std::condition_variable         _writeCond;
    mutable std::mutex              _destMutex;

    std::thread                     _logger;
    std::atomic_bool                _log{true};

    Priority                        _errorThreshold{MultiLogger::Priority::Error};
    std::atomic_size_t              _requestedErrors{0};
    verif_cb_t                      _verifCB;

    std::chrono::seconds            _maxWait{1ul};
};

//=============================================================================

LogDest::~LogDest()
{}

FileDest::FileDest(const std::string& fname_)
    : _file{fname_, std::ios_base::out}
{
    if (!_file) {
        throw std::runtime_error("cannot open file " + fname_ + " for logging!");
    }
}

FileDest::~FileDest()
{}

void FileDest::write(const std::string& msg_)
{
    if (_file) {
        _file << msg_;
    }
}

void FileDest::flush()
{
    _file.flush();
}

StdOutDest::~StdOutDest()
{}

void StdOutDest::write(const std::string& msg_)
{
    std::cout << msg_;
}

void StdOutDest::flush()
{
    std::cout.flush();
}

StdErrDest::~StdErrDest()
{}

void StdErrDest::write(const std::string& msg_)
{
    std::cerr << msg_;
}

void StdErrDest::flush()
{
    std::cerr.flush();
}

//=============================================================================

Logger::Logger(const Priority globalThreshold_
    , const std::string& category_)
    : _pImpl{MultiLogger::cpp14::imp::make_unique<Impl>(globalThreshold_, category_)}
{}

Logger::~Logger()
{}

void Logger::operator()(std::string&& message_
    , const Priority pri_
    , const char* function_
    , const char* file_
    , int line_
    , const std::thread::id threadId_)
{
    _pImpl->log(std::move(message_), pri_, function_, file_, line_, threadId_);
}

void Logger::category(const std::string& category_)
{
    _pImpl->category(category_);
}

void Logger::addDest(const std::string& name_, LogDest::ptr_t&& dest_)
{
    _pImpl->addDest(name_, std::move(dest_));
}

void Logger::addDest(const std::string& name_, const Priority thresHold_, LogDest::ptr_t&& dest_)
{
    _pImpl->addDest(name_, thresHold_, std::move(dest_));
}

void Logger::permitDest(const std::string& name_, const bool enable_)
{
    _pImpl->permitDest(name_, enable_);
}

void Logger::threshold(const Priority globalThreshold_)
{
    _pImpl->threshold(globalThreshold_);
}

void Logger::threshold(const std::string& destName_, const Priority threshold_)
{
    _pImpl->threshold(destName_, threshold_);
}

void Logger::verifyCB(const verif_cb_t& cb_)
{
    _pImpl->verifyCB(cb_);
}

void Logger::errorThreshold(const Priority errorThreshold_)
{
    _pImpl->errorThreshold(errorThreshold_);
}

const std::string& Logger::category() const
{
    return _pImpl->category();
}

Priority Logger::errorThreshold() const
{
    return _pImpl->errorThreshold();
}

bool Logger::logging(const Priority pri_) const
{
    return _pImpl->logging(pri_);
}

bool Logger::logging(const std::string& destName_) const
{
    return _pImpl->logging(destName_);
}

//=============================================================================

Logger& globalLogger()
{
    static Logger logger;
    return logger;
}

} // namespace MultiLogger
