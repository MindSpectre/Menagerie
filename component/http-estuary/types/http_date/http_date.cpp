#include "http_date.hpp"

#include <ctime>
#include <menagerie/cuckoo>

namespace menagerie::estuary {

    std::string_view imf_fixdate_now() noexcept {
        // `!=` rather than `<`: an NTP step backwards must still rebuild.
        thread_local std::time_t cached_second = -1;
        thread_local char buf[cuckoo::IMF_FIXDATE_LEN + 1];
        thread_local std::size_t len = 0;
        // One rollover per second vs ~half a million calls: as cold as it gets.
        if (const std::time_t now = std::time(nullptr); now != cached_second) [[unlikely]] {
            len           = cuckoo::format_imf_fixdate(now, std::span{buf});
            cached_second = now;
        }
        return {buf, len};
    }

}  // namespace menagerie::estuary
