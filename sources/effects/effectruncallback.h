#pragma once

#include <QtCore/QObject>

class EffectRunCallback : public QObject
{
    Q_OBJECT

public:
    bool isInterrupted() { return mIsInterrupted;  }
    void interrupt() { mIsInterrupted = true; }

signals:
    void sendImage(const QImage& img);

private:
    std::atomic_bool mIsInterrupted = false;
};
