#ifndef SECURESTRING_H
#define SECURESTRING_H

#include <vector>
#include <QString>
#include <QByteArray>
#include <cstring>
#include <algorithm>
#include <utility>

// Add POSIX includes for mlock
#ifdef __unix__
#include <sys/mman.h>
#include <unistd.h>
#endif

class SecureString {
public:
    SecureString() = default;

    explicit SecureString(const QString &str) {
        set(str);
    }

    ~SecureString() {
        clear();
    }

    // Disable copy
    SecureString(const SecureString&) = delete;
    SecureString& operator=(const SecureString&) = delete;

    // Enable move
    SecureString(SecureString &&other) noexcept : m_data(std::move(other.m_data)) {
        // Since m_data was moved, other.m_data is now empty.
    }

    SecureString& operator=(SecureString &&other) noexcept {
        if (this != &other) {
            clear();
            m_data = std::move(other.m_data);
        }
        return *this;
    }

    void set(const QString &str) {
        clear();
        QByteArray bytes = str.toUtf8();
        if (bytes.isEmpty()) return;

        m_data.resize(bytes.size());

        #ifdef __unix__
        // Try to lock memory to prevent swapping
        mlock(m_data.data(), m_data.size());
        #endif

        std::memcpy(m_data.data(), bytes.constData(), bytes.size());

        // Attempt to clear temporary QByteArray buffer
        volatile char *p = bytes.data();
        size_t s = bytes.size();
        while (s--) *p++ = 0;
    }

    bool isEmpty() const {
        return m_data.empty();
    }

    QByteArray toQByteArray() const {
        if (m_data.empty()) return QByteArray();
        return QByteArray(m_data.data(), static_cast<int>(m_data.size()));
    }

    QString toQString() const {
        if (m_data.empty()) return QString();
        return QString::fromUtf8(m_data.data(), static_cast<int>(m_data.size()));
    }

private:
    std::vector<char> m_data;

    void clear() {
        if (!m_data.empty()) {
            #ifdef __unix__
            munlock(m_data.data(), m_data.size());
            #endif

            volatile char *p = m_data.data();
            size_t s = m_data.size();
            while (s--) *p++ = 0;
            m_data.clear();
        }
    }
};

#endif // SECURESTRING_H
