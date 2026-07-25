#pragma once

#include <string>

#include "interface/entry_interface.hpp"

namespace menagerie::crow {
    /**
     * @brief Entry type formatting only level and message ("WRN message\n"), with no
     *        timestamp, source location, or thread/process id.
     *
     * Carries no Meta* mixins, so it is the lighter of the two built-in entry types;
     * see DetailedEntry when timestamp/source/thread/prefix are needed.
     */
    class LightEntry final : public detail::EntryBase<detail::MetaNone>  // only level+msg
    {
    public:
        using EntryBase::EntryBase;
        /// Constructs from a level and message; no metadata mixins to forward.
        LightEntry(const LogLevel lvl, const std::string_view msg)
            : EntryBase(lvl, msg, MetaNone{}) {
        }

        void format_into(std::string& out) const override {
            out.clear();
            out.append(log_level_to_string(level_));
            out.push_back(' ');
            out.append(message_);
            out.push_back('\n');
        }

        /// Always throws: LightEntry carries no timestamp to order by.
        /// @throw std::logic_error unconditionally.
        static bool comp(const LightEntry& lhs, const LightEntry& rhs) {
            beavers::unused_value(lhs, rhs);
            static_assert(true, "Can not be sorted");
            throw std::logic_error("Can not be sorted");
        }
    };
    /// entry_traits specialization telling make_entry() to build LightEntry with no
    /// Meta* mixins at all.
    template <>
    struct detail::entry_traits<LightEntry> {
        using wants = beavers::type_list<>;  ///< Empty: LightEntry composes no mixins.
    };
}  // namespace menagerie::crow
