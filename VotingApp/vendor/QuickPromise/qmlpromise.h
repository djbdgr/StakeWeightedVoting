#ifndef PROMISE_HPP
#define PROMISE_HPP

#include <QObject>
#include <QVariant>

class QJSValue;

class QmlPromise : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isFulfilled READ isFulfilled NOTIFY settled)
    Q_PROPERTY(bool isRejected READ isRejected NOTIFY settled)
    Q_PROPERTY(bool isSettled READ isSettled NOTIFY settled)
    QObject* internalPromise;
    bool wasFulfilled = false;
    bool wasRejected = false;

public:
    QmlPromise(QObject* parent);
    virtual ~QmlPromise();

    bool isFulfilled();
    bool isRejected();
    bool isSettled();
    /// Returns true if the javascript promise was already garbage collected; false otherwise
    /// @note This call is probably not terribly useful, as promise deletion depends on the vagaries of javascript
    /// garbage collection, but it's provided as it's unambiguously determined and may provide useful insight into the
    /// behavior of an app
    bool wasForgotten() { return internalPromise == nullptr; }

    operator QJSValue();

public slots:
    void resolve(QVariant args = QVariant());
    void reject(QVariant reason = QVariant());

signals:
    void fulfilled(QVariant);
    void rejected(QVariant);
    void settled();
};

#endif // PROMISE_HPP
