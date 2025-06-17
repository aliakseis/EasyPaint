#pragma once

#include <QString>
#include <QVariant>

struct ParameterInfo {
    QString name;
    QString fullName;
    QString kind; // POSITIONAL_OR_KEYWORD etc.
    QString description;
    QVariant defaultValue;
    QString annotation; // for example <class 'float'>,<class 'numpy.ndarray'>
};

struct FunctionInfo {
    QString name;
    QString fullName;
    QString signature;
    QString doc;
    std::vector<ParameterInfo> parameters;

    bool isCreatingFunction() const
    {
        return parameters.empty() || parameters[0].annotation != "<class 'numpy.ndarray'>";
    }
    bool usesMarkup() const
    {
        return parameters.size() > 1 && parameters[1].annotation == "<class 'numpy.ndarray'>";
    }

};
