#include <QtTest>

#include "../src/utils/AuthIncidentState.h"

class TestAuthIncidentState : public QObject {
    Q_OBJECT
   private slots:
    void testIncidentLifecycle() {
        AuthIncidentState state;

        QVERIFY(!state.inIncident());

        // 1. First auth failure in a continuous incident -> desktop notification
        state.recordAuthFailure("Invalid token");
        QVERIFY(state.inIncident());
        QCOMPARE(state.latestReason(), QString("Invalid token"));
        QVERIFY(state.shouldNotify());

        // 2. Repeated auth failures in that same incident -> no repeated popup
        state.recordAuthFailure("Still invalid");
        QVERIFY(state.inIncident());
        QCOMPARE(state.latestReason(), QString("Still invalid"));
        QVERIFY(!state.shouldNotify());

        // 3. Ordinary network errors don't affect auth state
        state.recordNetworkError();
        QVERIFY(state.inIncident());
        QCOMPARE(state.latestReason(), QString("Still invalid"));
        QVERIFY(!state.shouldNotify());

        // 4. Verified successful authenticated operation -> recovery/re-arm
        state.recordAuthenticatedSuccess();
        QVERIFY(!state.inIncident());

        // 5. Later independent auth failure -> notify again
        state.recordAuthFailure("Token expired");
        QVERIFY(state.inIncident());
        QCOMPARE(state.latestReason(), QString("Token expired"));
        QVERIFY(state.shouldNotify());
        QVERIFY(!state.shouldNotify());
    }

    void testReplacementTokenDoesNotClearIncident() {
        AuthIncidentState state;

        // First auth failure
        state.recordAuthFailure("Initial Bad Token");
        QVERIFY(state.inIncident());
        QVERIFY(state.shouldNotify());

        // Saving / loading replacement token happens in UI without touching AuthIncidentState
        // If the replacement token also fails, it remains in the same incident:
        state.recordAuthFailure("Replacement token also invalid");
        QVERIFY(state.inIncident());
        QCOMPARE(state.latestReason(), QString("Replacement token also invalid"));
        QVERIFY(!state.shouldNotify());

        // Network error occurs
        state.recordNetworkError();
        QVERIFY(state.inIncident());
        QVERIFY(!state.shouldNotify());

        // Only genuine authenticated success clears the incident
        state.recordAuthenticatedSuccess();
        QVERIFY(!state.inIncident());
        QVERIFY(state.latestReason().isEmpty());

        // Next failure will notify again
        state.recordAuthFailure("New failure");
        QVERIFY(state.inIncident());
        QVERIFY(state.shouldNotify());
    }
};

QTEST_MAIN(TestAuthIncidentState)
#include "TestAuthIncidentState.moc"
