#include "scripteffectsettings.h"

#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpacerItem>

ScriptEffectSettings::ScriptEffectSettings(const FunctionInfo& functionInfo, QWidget* parent /*= 0*/)
    : AbstractEffectSettings(parent)
{
    // Create a form layout to pair parameter label with control.
    QFormLayout* formLayout = new QFormLayout(this);

    // Iterate over each parameter from the function info (starting at index 1 as before).
    for (int i = 1; i < static_cast<int>(functionInfo.parameters.size()); ++i) {
        const auto& param = functionInfo.parameters[i];
        QWidget* control = nullptr;
        QString annotationLower = param.annotation.toLower();

        std::function<void(QVariant&, bool)> dxLambda;

        if (annotationLower.contains("int")) {
            QSpinBox* spinBox = new QSpinBox(this);
            spinBox->setRange(-100000, 100000);
            spinBox->setValue(param.defaultValue.isValid() ? param.defaultValue.toInt() : 0);
            control = spinBox;
            dxLambda = [spinBox](QVariant& var, bool save) {
                save ? var = spinBox->value() : spinBox->setValue(var.toInt());
                };

            // Connect signal to parameterless slot
            connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ScriptEffectSettings::parametersChanged);
        }
        else if (annotationLower.contains("float") || annotationLower.contains("double")) {
            QLineEdit* floatInput = new QLineEdit(this);

            // Apply a validator for basic float/double validation (no range restrictions)
            QDoubleValidator* validator = new QDoubleValidator(this);
            validator->setNotation(QDoubleValidator::StandardNotation); // Allow standard notation
            floatInput->setValidator(validator);

            if (param.defaultValue.isValid())
                floatInput->setText(QString::number(param.defaultValue.toDouble(), 'f', 6)); // Format consistently

            control = floatInput;

            dxLambda = [floatInput](QVariant& var, bool save) {
                if (save)
                    var = floatInput->text().toDouble();
                else
                    floatInput->setText(QString::number(var.toDouble(), 'f', 6));  // Keep formatting uniform
                };

            connect(floatInput, &QLineEdit::textChanged, this, &ScriptEffectSettings::parametersChanged);
        }
        else if (annotationLower.contains("bool")) {
            QCheckBox* checkBox = new QCheckBox(this);
            checkBox->setChecked(param.defaultValue.isValid() ? param.defaultValue.toBool() : false);
            control = checkBox;
            dxLambda = [checkBox](QVariant& var, bool save) {
                save ? var = checkBox->isChecked() : checkBox->setChecked(var.toBool());
                };

            connect(checkBox, &QCheckBox::stateChanged, this, &ScriptEffectSettings::parametersChanged);
        }
        else if (annotationLower.contains("str")) {
            QLineEdit* lineEdit = new QLineEdit(this);
            lineEdit->setText(param.defaultValue.isValid() ? param.defaultValue.toString() : "");
            control = lineEdit;
            dxLambda = [lineEdit](QVariant& var, bool save) {
                save ? var = lineEdit->text() : lineEdit->setText(var.toString());
                };

            connect(lineEdit, &QLineEdit::textChanged, this, &ScriptEffectSettings::parametersChanged);
        }
        else if (annotationLower.contains("tuple")) {
            QLineEdit* tupleEdit = new QLineEdit(this);
            tupleEdit->setText(param.defaultValue.isValid() ? param.defaultValue.toString() : "");
            control = tupleEdit;
            dxLambda = [tupleEdit](QVariant& var, bool save) {
                save ? var = tupleEdit->text() : tupleEdit->setText(var.toString());
                };

            connect(tupleEdit, &QLineEdit::textChanged, this, &ScriptEffectSettings::parametersChanged);
        }
        else {
            QLineEdit* lineEdit = new QLineEdit(this);
            lineEdit->setText(param.defaultValue.isValid() ? param.defaultValue.toString() : "");
            control = lineEdit;
            dxLambda = [lineEdit](QVariant& var, bool save) {
                save ? var = lineEdit->text() : lineEdit->setText(var.toString());
                };

            connect(lineEdit, &QLineEdit::textChanged, this, &ScriptEffectSettings::parametersChanged);
        }

        if (!param.description.isEmpty()) {
            control->setToolTip(param.description);
        }

        formLayout->addRow(new QLabel(param.fullName, this), control);
        mDataExchange.push_back(dxLambda);
    }

    // Add a spacer to push controls to the top.
    formLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    setLayout(formLayout);
}

QList<QVariant> ScriptEffectSettings::getEffectSettings()
{
    QList<QVariant> settings;

    // Iterate through the stored data exchange functions and extract values.
    for (auto& exchangeFunc : mDataExchange)
    {
        QVariant value;
        exchangeFunc(value, true);  // `true` means "retrieve value from control"
        settings.append(std::move(value));
    }

    return settings;
}
