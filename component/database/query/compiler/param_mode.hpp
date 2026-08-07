#pragma once

#include <cstdint>

namespace menagerie::db {

    /// How QueryCompiler/SqlGeneratorVisitor handle literal values while compiling an expression tree.
    enum class ParamMode : std::uint8_t {
        Inline,  ///< Values are formatted directly into the SQL text; no params collected.
        Tuple,   ///< Placeholders are written into the SQL; params are returned as a std::tuple.
        Sink     ///< Placeholders are written into the SQL; params are pushed to a ParamSink.
    };

}  // namespace menagerie::db
