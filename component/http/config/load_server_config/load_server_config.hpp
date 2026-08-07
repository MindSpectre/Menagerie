#pragma once

#include <cstddef>
#include <menagerie/beavers>
#include <string>
#include <string_view>

#include <json/json.h>
#include <server_config.hpp>

namespace menagerie::http {

    struct Response;  // conversions return it; defined in types/response

    /// Filesystem-level failure: missing / unreadable / not a regular file.
    struct ConfigFileError {
        std::string path;    ///< Path that was opened.
        std::string reason;  ///< Why the open/read failed.
    };

    /// Malformed JSON. `line` is best-effort (0 when it cannot be extracted
    /// from the reader's message); `detail` carries jsoncpp's full message.
    struct ConfigParseError {
        std::string path;        ///< Path that was parsed.
        std::size_t line = 0;    ///< Best-effort line number; 0 if unknown.
        std::string detail;      ///< jsoncpp's full parser message.
    };

    /// Well-formed JSON that does not describe a valid ServerConfig: a type
    /// mismatch, an unknown enum string, or a validate() failure. field_path
    /// is best-effort (often empty - `detail` names the offending field; full
    /// /listeners/1/tls/cert_file pointers would need path threading through
    /// every read_field overload, not implemented yet).
    struct ConfigSchemaError {
        std::string path;        ///< Path that was loaded.
        std::string field_path;  ///< Best-effort offending field path; often empty.
        std::string detail;      ///< Human-readable description of the mismatch.
    };

    /**
     * @brief Load + validate a ServerConfig from a JSON file.
     *
     * 1. Open the file (must be a regular file)   -> ConfigFileError
     * 2. Parse JSON (Json::CharReader pipeline)   -> ConfigParseError (+line)
     * 3. Deserialize via the fields() machinery   -> ConfigSchemaError
     *    (type mismatch / unknown enum string)
     * 4. validate() (run by Builder::finalize)    -> ConfigSchemaError
     *
     * Unknown JSON keys are ignored (the fields() walk reads known names
     * only). An empty file is a parse error, not an empty config.
     */
    beavers::Outcome<ServerConfig, ConfigFileError, ConfigParseError, ConfigSchemaError>
    load_server_config(std::string_view path);

    /// Round-trip companion: serialize - which validates first. Secret
    /// fields (tls.key_passphrase) are omitted by policy.
    Json::Value dump_server_config(const ServerConfig& cfg);

    // ADL conversions: config errors surfaced on admin endpoints. Cold path -
    // global heap via the static ResponseFactory (errors.cpp convention);
    // all three render as 500.
    /// Converts a file-open/read failure to a 500 response.
    Response to_http_response(const ConfigFileError& e);
    /// Converts a JSON parse failure to a 500 response.
    Response to_http_response(const ConfigParseError& e);
    /// Converts a schema/validation failure to a 500 response.
    Response to_http_response(const ConfigSchemaError& e);

}  // namespace menagerie::http
