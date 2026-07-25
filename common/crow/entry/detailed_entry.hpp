#pragma once

#include <string>

#include "interface/entry_interface.hpp"

namespace menagerie::crow {
    /**
     * @brief Entry type formatting timestamp, source location, thread/process id, and
     *        prefix alongside level and message.
     *
     * Carries every Meta* mixin (MetaTimePoint, MetaSource, MetaThread, MetaProcess,
     * MetaPrefix), so it is the heavier of the two built-in entry types; see LightEntry
     * for a level+message-only alternative.
     */
    class DetailedEntry final : public detail::EntryBase<detail::MetaTimePoint,
                                                         detail::MetaSource,
                                                         detail::MetaThread,
                                                         detail::MetaProcess,
                                                         detail::MetaPrefix> {
    public:
        using EntryBase::EntryBase;

        void format_into(std::string& out) const override;

        /// Orders entries by timestamp, then by level for entries with equal timestamps.
        static bool comp(const DetailedEntry& lhs, const DetailedEntry& rhs) {
            // Optimized comparison using single 64-bit compare when possible
            if (lhs.time_point != rhs.time_point) {
                return lhs.time_point < rhs.time_point;
            }
            return lhs.level() < rhs.level();
        }
    };

    /// entry_traits specialization telling make_entry() to build DetailedEntry with
    /// every Meta* mixin (timestamp, source location, thread id, process id, prefix).
    template <>
    struct detail::entry_traits<DetailedEntry> {
        using wants = beavers::type_list<MetaTimePoint, MetaSource, MetaThread, MetaProcess, MetaPrefix>;  ///< Mixins DetailedEntry composes.
    };
}  // namespace menagerie::crow
