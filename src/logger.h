#ifndef GODWHALE_LOGGER_H
#define GODWHALE_LOGGER_H

namespace godwhale {

/**
 * @brief ログレベル
 */
enum SeverityLevel
{
    Debug,
    Notification,
    Warning,
    Error,
    Critical,
};

BOOST_LOG_ATTRIBUTE_KEYWORD(Severity, "Severity", SeverityLevel);
BOOST_LOG_GLOBAL_LOGGER(Logger, boost::log::sources::severity_logger_mt<SeverityLevel>);

#define LOG(severity)                                                                       \
    BOOST_LOG_SEV(::godwhale::Logger::get(), severity)                                      \
        << boost::log::add_value("SrcFile", ::godwhale::log_filename(__FILE__))             \
        << boost::log::add_value("RecordLine", std::to_string(__LINE__))                    \
        << boost::log::add_value("CurrentFunction", ::godwhale::log_funcname(__FUNCTION__)) \
        << boost::log::add_value("SeverityName", ::godwhale::severity_name(severity))

#define LOG_DEBUG()        LOG(::godwhale::Debug)
#define LOG_NOTIFICATION() LOG(::godwhale::Notification)
#define LOG_WARNING()      LOG(::godwhale::Warning)
#define LOG_ERROR()        LOG(::godwhale::Error)
#define LOG_CRITICAL()     LOG(::godwhale::Critical)

/// ログを初期化します。
extern void init_log();
extern std::string log_filename(std::string const & filepath);
extern std::string log_funcname(std::string const & funcname);
extern std::string severity_name(SeverityLevel severity);

} // namespace godwhale

#endif
