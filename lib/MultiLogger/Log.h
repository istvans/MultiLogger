#pragma once // not standard, but widely supported and better than include guards

#include <memory>
#include <string>
#include <thread>
#include <fstream>
#include <functional>

// C++11 backward-compatibility
#ifdef _MSC_VER
# if (__cplusplus >= 201402L) || (_MSC_VER > 1800)
#  define USING_CPP14 1
# endif
#elif defined(__GNUC__) || defined(__cplusplus)
# if (__cplusplus >= 201402L)
#  define USING_CPP14 1
# endif
#endif

namespace MultiLogger
{

#ifdef USING_CPP14
namespace cpp14
{ namespace imp = std; }
#else
namespace cpp14
{
namespace imp
{
/// Basic version without array & custom deleter support.
/// Source: Effective Modern C++ - Item 21: Prefer std::make_unique and std::make_shared to direct use of new.
template <class T, class... Ts>
std::unique_ptr<T> make_unique(Ts&&... params_)
{
    return std::unique_ptr<T>(new T{std::forward<Ts>(params)...});
}
}
}
#endif

enum class Priority
{
    Debug,
    Info,
    Warning,
    Error,
    Critical,
    __Size,
};

template <class Ostream>
Ostream& operator<<(Ostream& lhs_, const Priority rhs_)
{
    switch (rhs_) {
        case Priority::Debug: return lhs_ << "Debug";
        case Priority::Info: return lhs_ << "Info";
        case Priority::Warning: return lhs_ << "Warning";
        case Priority::Error: return lhs_ << "Error";
        case Priority::Critical: return lhs_ << "Critical";
    }
    throw std::runtime_error("unknown priority!");
}

//=============================================================================

/// @todo Add rolling file destination.
/// @todo Add compressed file destination.

/**
 * This abstract class makes the Logger able to
 * log messages to arbitrary targets.<br/>
 * It specifies the common interface for
 * log destinations and allows their dynamic
 * storage.
 * @todo Add setting to specify whether the LogDest
 *       automatically follows the global threshold
 *       setting or its threshold can be changed
 *       manually only.
 */
struct LogDest
{
    using ptr_t = std::unique_ptr<LogDest>;

    virtual ~LogDest();
    virtual void write(const std::string&) = 0;
    virtual void flush() = 0;
};

/// Log to a file.
/// @todo Always truncates the file. Add an option to allow append.
struct FileDest : public LogDest
{
    FileDest(const std::string& fname_);
    ~FileDest() override;
    void write(const std::string& msg_) override;
    void flush() override;
private:
    std::fstream        _file;
};

/// Log to stdout.
struct StdOutDest : public LogDest
{
    ~StdOutDest() override;
    void write(const std::string& msg_) override;
    void flush() override;
};

/// Log to stderr.
struct StdErrDest : public LogDest
{
    ~StdErrDest() override;
    void write(const std::string& msg_) override;
    void flush() override;
};

//=============================================================================

using verif_cb_t = std::function<void(const size_t)>;

/**
 * @mainpage
 * 
 * @section intro_sec Introduction
 * 
 * This is a simple thread-safe logger library which supports multiple,
 * arbitrary log targets.
 * 
 * @section overview_sec Overview
 * 
 * This Logger library can be used to log anything compatible with std::ostringstream.
 * It is guaranteed that the log messages are in chronological order in every log
 * destination. This means the logs are in the same order in every output.
 * 
 * The supported log priorities are:
 *   * <i>Debug</i>
 *   * <i>Info</i>
 *   * <i>Warning</i>
 *   * <i>Error</i>
 *   * <i>Critical</i>
 *   .
 * If the threshold is <i>Info</i> and we try to log a <i>Debug</i> message
 * it won't be logged.
 * 
 * Each Logger has its own category, so they can be differentiated
 * from the log. This is important because this Logger library is not a
 * singleton so it does not restrict the users unnecessarily to have
 * only one logger. However, it is generally useful to access a global logger,
 * so it is provided with a non-member function called <i>globalLogger</i>.
 * It has <i>Info</i> threshold and the <i>global</i> category by default.<br/>
 * There are convenience macros named <b>MRLog*L</b> and <b>MRLog*G</b> where
 * <b>L</b> stands for local and <b>G</b> for global Logger instance.
 * (@ref examples_sec "see examples below")
 * 
 * The Logger was implemented using the PImpl idiom to provide stable ABI
 * for the library.
 * 
 * @section build_sec Build
 * 
 * <b>This library requires a compiler supporting at least C++11.</b><br/>
 * If you are on Windows, simply open the VS project file and hit F5 to
 * run the Test application (@ref test_sec "see below").<br/>
 * The Logger uses standard library only so it should compile fine on other
 * platforms as well.
 * 
 * @section examples_sec Examples
 *
 * ### Log an info level message using the global logger with its default settings:
 * 
 @code
 MRLogInfoG("The value of the x variable is " << x);
 @endcode
 * 
 * ### Set the category and log priority threshold on the global Logger:
 * 
 @code
 MultiLogger::globalLogger().category("tester");
 MultiLogger::globalLogger().threshold(MultiLogger::Priority::Debug);
 @endcode
 * 
 * ### Create your own local Logger instance:
 * 
 @code
 MultiLogger::Logger debugger{MultiLogger::Priority::Debug, "debugger"};
 @endcode
 * 
 * ### Add a new file target:
 * 
 @code
 // std::make_unique is only available from C++14!
 debugger.addDest("dest-name-1", std::make_unique<MultiLogger::FileDest>("out.txt"));
 // If you have only C++11 compiler try this:
 debugger.addDest("dest-name-1", MultiLogger::cpp14::imp::make_unique<MultiLogger::FileDest>("out.txt"));
 @endcode
 * 
 * ### Enable logging to the standard output:
 * 
 @code
 debugger.addDest("stdout", std::make_unique<MultiLogger::StdOutDest>());
 @endcode
 * 
 * @section test_sec Tests
 * 
 * To try out and verify the library a @ref LogTester::Test "tester application" is provided.<br/>
 * To run the <a href="https://github.com/philsquared/Catch">Catch</a> unit tests go to the CatchUnitTests project under tests/UnitTests.<br/>
 * <b>Tip:</b> From VS run the unit test by hitting Ctrl + F5 to prevent the console to disappear at the end.
 * 
 */
class Logger
{
    struct Impl;
    std::unique_ptr<Impl> _pImpl;
public:
    /// Create a logger with the specified global threshold and
    /// category.
    explicit Logger(const Priority globalThreshold_ = Priority::Info
        , const std::string& category_ = "global");
    ~Logger();

    /// Log a message with the given parameters.
    /// Instead of directly calling this method
    /// use the MRLogL, MRLogG or one of the other
    /// priority-specific macros.
    void operator()(std::string&& message_
        , const Priority pri_
        , const char* function_
        , const char* file_
        , int line_
        , const std::thread::id threadId_);
    
    /// Set the logger's category so it will be distinguishable.
    void category(const std::string& category_);
    /// Add a new log destination aka log target.
    void addDest(const std::string& name_, LogDest::ptr_t&& dest_);
    /// Add a new log target and specify its threshold.<br/>
    /// <b>Important Note:</b> The log destinations cannot log
    /// messages with lower priority than the global threshold.
    void addDest(const std::string& name_, const Priority thresHold_, LogDest::ptr_t&& dest_);
    /// Enabled/disable log destination.
    void permitDest(const std::string& name_, const bool enable_);
    /// Set the global log priority threshold. Messages with lower priority than
    /// this threshold won't be logged.
    void threshold(const Priority globalThreshold_);
    /// Set the log priority threshold of a log target. Messages with lower priority than
    /// this threshold won't be logged.<br/>
    /// <b>Important Note:</b> Even if we set it the log destinations cannot log
    /// messages with lower priority than the global threshold.
    void threshold(const std::string& destName_, const Priority threshold_);
    /// Set a callback function which is called when the logger
    /// destructs itself. It is ensured that all the destinations
    /// are flushed before this call.
    void verifyCB(const verif_cb_t& cb_);
    /// Set the minimum log priority which should be considered error.
    /// It is important for the <i>verifyCB</i>.<br/>
    /// Its default value is <i>Error</i>.
    void errorThreshold(const Priority errorThreshold_);

    /// @return the current category of the logger
    const std::string& category() const;
    /// Get the currently set minimum log priority which we consider an error.
    /// @return the error threshold priority
    Priority errorThreshold() const;
    /// @return true if a message with the specified priority would be logged
    ///         based on the global threshold
    bool logging(const Priority pri_) const;
    /// @return true if the specified destination is enabled
    bool logging(const std::string& destName_) const;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;
};

//=============================================================================
// Local loggers' macro helpers

#define MRLogL(__LoggeR__, __PrioritY__, __MessagE__)           \
    __LoggeR__(                                                 \
        static_cast<std::ostringstream&>(                       \
          std::ostringstream().flush() << __MessagE__           \
        ).str()                                                 \
        ,__PrioritY__                                           \
        ,__FUNCTION__                                           \
        ,__FILE__                                               \
        ,__LINE__                                               \
        ,std::this_thread::get_id()                             \
    )

#define MRLogDebugL(__LoggeR__, __MessagE__)        MRLogL(__LoggeR__, ::MultiLogger::Priority::Debug, __MessagE__)
#define MRLogInfoL(__LoggeR__, __MessagE__)         MRLogL(__LoggeR__, ::MultiLogger::Priority::Info, __MessagE__)
#define MRLogWarningL(__LoggeR__, __MessagE__)      MRLogL(__LoggeR__, ::MultiLogger::Priority::Warning, __MessagE__)
#define MRLogErrorL(__LoggeR__, __MessagE__)        MRLogL(__LoggeR__, ::MultiLogger::Priority::Error, __MessagE__)
#define MRLogCriticalL(__LoggeR__, __MessagE__)     MRLogL(__LoggeR__, ::MultiLogger::Priority::Critical, __MessagE__)

//=============================================================================
// The global logger and its macro helpers

/// Call this method or one of the MRLog*G macros to use
/// the globally accessible static logger instance.
Logger& globalLogger();

#define MRLogG(__PrioritY__, __MessagE__)           MRLogL(::MultiLogger::globalLogger(), __PrioritY__, __MessagE__)
#define MRLogDebugG(__MessagE__)                    MRLogDebugL(::MultiLogger::globalLogger(), __MessagE__)
#define MRLogInfoG(__MessagE__)                     MRLogInfoL(::MultiLogger::globalLogger(), __MessagE__)
#define MRLogWarningG(__MessagE__)                  MRLogWarningL(::MultiLogger::globalLogger(), __MessagE__)
#define MRLogErrorG(__MessagE__)                    MRLogErrorL(::MultiLogger::globalLogger(), __MessagE__)
#define MRLogCriticalG(__MessagE__)                 MRLogCriticalL(::MultiLogger::globalLogger(), __MessagE__)

} // namespace MultiLogger
