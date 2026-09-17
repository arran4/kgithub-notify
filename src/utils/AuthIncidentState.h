#ifndef KGHN_AUTHINCIDENTSTATE_H
#define KGHN_AUTHINCIDENTSTATE_H

#include <QString>

class AuthIncidentState {
   public:
    void recordAuthFailure(const QString& reason) {
        if (!m_inIncident) {
            m_inIncident = true;
            m_notified = false;
        }
        m_latestReason = reason;
    }

    void recordAuthenticatedSuccess() {
        m_inIncident = false;
        m_notified = false;
        m_latestReason.clear();
    }

    void recordNetworkError() {
        // Network errors don't affect auth state
    }

    bool shouldNotify() {
        if (m_inIncident && !m_notified) {
            m_notified = true;
            return true;
        }
        return false;
    }

    bool inIncident() const { return m_inIncident; }
    QString latestReason() const { return m_latestReason; }

    // Test helper to check if notification would be sent without mutating state
    bool willNotify() const { return m_inIncident && !m_notified; }

   private:
    bool m_inIncident = false;
    bool m_notified = false;
    QString m_latestReason;
};

#endif  // KGHN_AUTHINCIDENTSTATE_H
