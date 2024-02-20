/*
 * Basic implementation of string_view
 *
 * (C) 2015 Julian Andres Klode <jak@debian.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#if !defined(APT_STRINGVIEW_H)
#define APT_STRINGVIEW_H
#include <apt-pkg/macros.h>
#include <cstring>
#include <string>

namespace APT {

/**
 * \brief Simple subset of std::string_view from C++17
 *
 * This is an internal implementation of the subset of std::string_view
 * used by APT. It is not meant to be used in programs, only inside the
 * library for performance critical paths.
 */
class StringView {
    const char *data_;
    size_t size_;

public:
    static constexpr size_t npos = static_cast<size_t>(-1);
    static_assert(APT::StringView::npos == std::string::npos, "npos values are different");

    /* Constructors */
    constexpr StringView() : data_(""), size_(0) {}
    constexpr StringView(const char *data, size_t size) : data_(data), size_(size) {}

    StringView(const char *data) : data_(data), size_(strlen(data)) {}
    StringView(std::string const & str): data_(str.data()), size_(str.size()) {}

    /* Modifiers */
    void remove_prefix(size_t n) { data_ += n; size_ -= n; }
    void remove_suffix(size_t n) { size_ -= n; }
    void clear() { size_ = 0; }

    /* Viewers */
    constexpr StringView substr(size_t pos, size_t n = npos) const {
        return StringView(data_ + pos, n > (size_ - pos) ? (size_ - pos) : n);
    }

    size_t find(int c, size_t pos) const {
       if (pos == 0)
	  return find(c);
       size_t const found = substr(pos).find(c);
       if (found == npos)
	  return npos;
       return pos + found;
    }
    size_t find(int c) const {
        const char *found = static_cast<const char*>(memchr(data_, c, size_));

        if (found == NULL)
            return npos;

        return found - data_;
    }

    size_t rfind(int c, size_t pos) const {
       if (pos == npos)
	  return rfind(c);
       return APT::StringView(data_, pos).rfind(c);
    }
    size_t rfind(int c) const {
        const char *found = static_cast<const char*>(memrchr(data_, c, size_));

        if (found == NULL)
            return npos;

        return found - data_;
    }

    size_t find(APT::StringView const needle) const {
       if (needle.empty())
	  return npos;
       if (needle.length() == 1)
	  return find(*needle.data());
       size_t found = 0;
       while ((found = find(*needle.data(), found)) != npos) {
	  if (compare(found, needle.length(), needle) == 0)
	     return found;
	  ++found;
       }
       return found;
    }
    size_t find(APT::StringView const needle, size_t pos) const {
       if (pos == 0)
	  return find(needle);
       size_t const found = substr(pos).find(needle);
       if (found == npos)
	  return npos;
       return pos + found;
    }

    /* Conversions */
    std::string to_string() const {
        return std::string(data_, size_);
    }

    /* Comparisons */
    int compare(size_t pos, size_t n, StringView other) const {
        return substr(pos, n).compare(other);
    }

    int compare(StringView other) const {
        int res;

        res = memcmp(data_, other.data_, std::min(size_, other.size_));
        if (res != 0)
            return res;
        if (size_ == other.size_)
            return res;

        return (size_ > other.size_) ? 1 : -1;
    }

    /* Optimization: If size not equal, string cannot be equal */
    bool operator ==(StringView other) const { return size_ == other.size_ && compare(other) == 0; }
    bool operator !=(StringView other) const { return !(*this == other); }

    /* Accessors */
    constexpr bool empty() const { return size_ == 0; }
    constexpr const char* data() const { return data_; }
    constexpr const char* begin() const { return data_; }
    constexpr const char* end() const { return data_ + size_; }
    constexpr char operator [](size_t i) const { return data_[i]; }
    constexpr size_t size() const { return size_; }
    constexpr size_t length() const { return size_; }
};

/**
 * \brief Faster comparison for string views (compare size before data)
 *
 * Still stable, but faster than the normal ordering. */
static inline int StringViewCompareFast(StringView a, StringView b) {
    if (a.size() != b.size())
        return a.size() - b.size();

    return memcmp(a.data(), b.data(), a.size());
}

static constexpr inline APT::StringView operator""_sv(const char *data, size_t size)
{
   return APT::StringView(data, size);
}
}

inline bool operator ==(const char *other, APT::StringView that);
inline bool operator ==(const char *other, APT::StringView that) { return that.operator==(other); }
inline bool operator ==(std::string const &other, APT::StringView that);
inline bool operator ==(std::string const &other, APT::StringView that) { return that.operator==(other); }

#endif
