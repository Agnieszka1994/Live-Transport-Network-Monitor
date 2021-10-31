#ifndef WEBSOCKET_CLIENT_FILE_DOWNLOADER_H
#define WEBSOCKET_CLIENT_FILE_DOWNLOADER_H

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>

namespace NetworkMonitor {

/*! \brief Download a file from a remote HTTPS URL.
 *
 *  \param destination The full path and filename of the output file. The path
 *                     to the file must exist.
 *  \param caCertFile  The path to a cacert.pem file to perform certificate
 *                     verification in an HTTPS connection.
 */
bool DownloadFile(
    const std::string& fileUrl,
    const std::filesystem::path& destination,
    const std::filesystem::path& caCertFile = {}
);

/*! \brief Parse a local file into a JSON object.
 *
 *  \param source The path to the JSON file to load and parse.
 */
nlohmann::json ParseJsonFile(
    const std::filesystem::path& source
);

} // namespace NetworkMonitor

#endif // WEBSOCKET_CLIENT_FILE_DOWNLOADER_H