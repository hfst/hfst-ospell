#ifndef HFST_OSPELL_OFFICE_H_
#define HFST_OSPELL_OFFICE_H_

#include <memory>
#include <vector>
#include <string>
#include <iosfwd>

#ifdef _WIN32
    #ifdef LIBHFSTOSPELLOFFICE_EXPORTS
        #define OSPELL_OFFICE_API __declspec(dllexport)
    #else
        #define OSPELL_OFFICE_API __declspec(dllimport)
    #endif
#else
    #ifdef LIBHFSTOSPELLOFFICE_EXPORTS
        #define OSPELL_OFFICE_API __attribute__ ((visibility ("default")))
    #else
        #define OSPELL_OFFICE_API
    #endif
#endif

namespace hfst_ospell {

template<typename C>
inline auto is_space(const C& c) {
    return ((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n'));
}

template<typename Str>
inline void trim(Str& str) {
    while (!str.empty() && is_space(str.back())) {
        str.pop_back();
    }
    while (!str.empty() && is_space(str.front())) {
        str.erase(str.begin());
    }
}

template<typename Str>
inline void normalize(Str& str) {
    // NBSP
    size_t s = 0;
    while ((s = str.find("\xc2\xa0", s)) != Str::npos) {
        str.replace(s, 2, " ");
    }
    // Soft-hyphen
    s = 0;
    while ((s = str.find("\xc2\xad", s)) != Str::npos) {
        str.erase(s, 2);
    }
    // Tab
    s = 0;
    while ((s = str.find("\t", s)) != Str::npos) {
        str[s] = ' ';
    }
    // Carriage return
    s = 0;
    while ((s = str.find("\r", s)) != Str::npos) {
        str[s] = ' ';
    }
    // Multiple spaces
    s = 0;
    while ((s = str.find("  ", s)) != Str::npos) {
        str.replace(s, 2, " ");
    }
}

struct OfficeSpellerOptions {
    bool verbatim = false;
    bool debug = false;
    float max_weight = -1.0;
    float beam = -1.0;
    float time_cutoff = 6.0;
};

using Alt = std::pair<double, std::string>;

class OfficeSpeller {
public:
    OSPELL_OFFICE_API OfficeSpeller(const char* zhfst, OfficeSpellerOptions opts = {});
    OSPELL_OFFICE_API ~OfficeSpeller();

    OSPELL_OFFICE_API OfficeSpeller(OfficeSpeller&&) noexcept;
    OSPELL_OFFICE_API OfficeSpeller& operator=(OfficeSpeller&&) noexcept;

    OSPELL_OFFICE_API void ispell(std::istream& in, std::ostream& out);
    OSPELL_OFFICE_API void stream(std::istream& in, std::ostream& out);
    OSPELL_OFFICE_API bool check(const std::string& word);
    OSPELL_OFFICE_API const std::vector<Alt>& correct(const std::string& word, size_t suggs = 5);

private:
    class Impl;
    std::unique_ptr<Impl> im;
};

}

#endif
