#pragma once

#include "abstracteffectsettings.h"
#include "../ScriptInfo.h"

class ScriptEffectSettings : public AbstractEffectSettings
{
public:
    ScriptEffectSettings(const FunctionInfo& functionInfo, QWidget* parent = 0);

private:
    // Inherited via AbstractEffectSettings
    QList<QVariant> getEffectSettings() override;

    std::vector<std::function<void(QVariant&, bool)>> mDataExchange;
};

