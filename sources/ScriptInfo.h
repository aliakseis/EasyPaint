#pragma once

#include <QString>
#include <QVariant>

struct ParameterInfo {
    QString name;
    QString fullName;
    QString kind; // POSITIONAL_OR_KEYWORD etc.
    QString description;
    QVariant defaultValue;
    QString annotation; // for example <class 'float'>
};

struct FunctionInfo {
    QString name;
    QString fullName;
    QString signature;
    QString doc;
    std::vector<ParameterInfo> parameters;
};
