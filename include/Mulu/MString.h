#pragma once

#include <ostream>
#include <string>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

namespace mulu {

// Unicode string type used throughout MuluUI. UTF-8 on the inside;
// converts to/from UTF-16 (wchar_t) when talking to native toolkits.
class MString {
public:
    MString() = default;
    MString(const char* s) : m_data(s ? s : "") {}
    MString(const std::string& s) : m_data(s) {}
    MString(std::string&& s) : m_data(std::move(s)) {}
    MString(const wchar_t* s) { *this = fromWide(s ? s : L""); }
    MString(const std::wstring& s) { *this = fromWide(s); }

    // Accessors -----------------------------------------------------------
    const std::string& toStdString() const { return m_data; }
    const char* c_str() const { return m_data.c_str(); }

    std::wstring toStdWString() const {
#ifdef _WIN32
        if (m_data.empty()) return L"";
        const int size = MultiByteToWideChar(CP_UTF8, 0, m_data.c_str(),
                                             static_cast<int>(m_data.size()),
                                             nullptr, 0);
        std::wstring result(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, m_data.c_str(),
                            static_cast<int>(m_data.size()), &result[0], size);
        return result;
#else
        return std::wstring(m_data.begin(), m_data.end());
#endif
    }

    static MString fromWide(const std::wstring& ws) {
#ifdef _WIN32
        if (ws.empty()) return MString();
        const int size = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(),
                                             static_cast<int>(ws.size()),
                                             nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()),
                            &result[0], size, nullptr, nullptr);
        return MString(std::move(result));
#else
        return MString(std::string(ws.begin(), ws.end()));
#endif
    }

    bool isEmpty() const { return m_data.empty(); }
    size_t size() const { return m_data.size(); }

    // Mutators ------------------------------------------------------------
    MString& append(const MString& other) {
        m_data += other.m_data;
        return *this;
    }
    MString& operator+=(const MString& other) { return append(other); }
    void clear() { m_data.clear(); }

private:
    std::string m_data;
};

// Comparisons ----------------------------------------------------------------
inline bool operator==(const MString& a, const MString& b) {
    return a.toStdString() == b.toStdString();
}
inline bool operator!=(const MString& a, const MString& b) { return !(a == b); }
inline bool operator<(const MString& a, const MString& b) {
    return a.toStdString() < b.toStdString();
}

// Streaming (e.g. std::cerr << someMString).
inline std::ostream& operator<<(std::ostream& stream, const MString& s) {
    return stream << s.toStdString();
}

} // namespace mulu
