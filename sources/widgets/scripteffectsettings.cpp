#include "scripteffectsettings.h"

#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpacerItem>

ScriptEffectSettings::ScriptEffectSettings(const FunctionInfo& functionInfo, QVariantList& effectSettings, QWidget* parent /*= 0*/)
    : AbstractEffectSettings(parent)
{
    // Create a form layout to pair parameter labels with controls.
    QFormLayout* formLayout = new QFormLayout(this);

    // Use the C locale for formatting conversions - it always uses a dot for decimals.
    QLocale cLocale = QLocale::c();

    // Iterate over each parameter (starting at index 1 if not creating a function).
    for (int i = functionInfo.isCreatingFunction() ? 0 : (1 + functionInfo.usesMarkup()); i < static_cast<int>(functionInfo.parameters.size()); ++i) {
        const auto& param = functionInfo.parameters[i];
        QWidget* control = nullptr;
        QString annotationLower = param.annotation.toLower();

        std::function<void(QVariant&, bool)> dxLambda;

        if (annotationLower.contains("int")) {
            QSpinBox* spinBox = new QSpinBox(this);
            spinBox->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
            spinBox->setValue(param.defaultValue.isValid() ? param.defaultValue.toInt() : 0);
            control = spinBox;
            dxLambda = [spinBox](QVariant& var, bool save) {
                if (save) {
                    var = spinBox->value();
                }
                else {
                    spinBox->setValue(var.toInt());
                }
                };
            connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ScriptEffectSettings::parametersChanged);
        }
        else if (annotationLower.contains("float") || annotationLower.contains("double")) {
            QLineEdit* floatInput = new QLineEdit(this);

            // Set up a validator with the C locale to ensure a dot decimal separator.
            QDoubleValidator* validator = new QDoubleValidator(this);
            validator->setNotation(QDoubleValidator::StandardNotation);
            validator->setLocale(QLocale::c());
            floatInput->setValidator(validator);

            if (param.defaultValue.isValid()) {
                double d = param.defaultValue.toDouble();
                floatInput->setText(cLocale.toString(d, 'f', 6));
            }
            control = floatInput;
            dxLambda = [floatInput](QVariant& var, bool save) {
                if (save) {
                    // Convert using the C locale for consistent parsing.
                    double d = QLocale::c().toDouble(floatInput->text());
                    var = d;
                }
                else {
                    double d = var.toDouble();
                    floatInput->setText(QLocale::c().toString(d, 'f', 6));
                }
                };
            connect(floatInput, &QLineEdit::textChanged, this, &ScriptEffectSettings::parametersChanged);
        }
        else if (annotationLower.contains("bool")) {
            QCheckBox* checkBox = new QCheckBox(this);
            checkBox->setChecked(param.defaultValue.isValid() ? param.defaultValue.toBool() : false);
            control = checkBox;
            dxLambda = [checkBox](QVariant& var, bool save) {
                if (save) {
                    var = checkBox->isChecked();
                }
                else {
                    checkBox->setChecked(var.toBool());
                }
                };
            connect(checkBox, &QCheckBox::stateChanged, this, &ScriptEffectSettings::parametersChanged);
        }
        else if (annotationLower.contains("str")) {
            QLineEdit* lineEdit = new QLineEdit(this);
            lineEdit->setText(param.defaultValue.isValid() ? param.defaultValue.toString() : "");
            control = lineEdit;
            dxLambda = [lineEdit](QVariant& var, bool save) {
                if (save) {
                    var = lineEdit->text();
                }
                else {
                    lineEdit->setText(var.toString());
                }
                };
            connect(lineEdit, &QLineEdit::textChanged, this, &ScriptEffectSettings::parametersChanged);
        }
        else if (annotationLower.contains("tuple")) {
            QLineEdit* tupleEdit = new QLineEdit(this);
            tupleEdit->setText(param.defaultValue.isValid() ? param.defaultValue.toString() : "");
            control = tupleEdit;
            dxLambda = [tupleEdit](QVariant& var, bool save) {
                if (save) {
                    var = tupleEdit->text();
                }
                else {
                    tupleEdit->setText(var.toString());
                }
                };
            connect(tupleEdit, &QLineEdit::textChanged, this, &ScriptEffectSettings::parametersChanged);
        }
        else if (annotationLower.contains("complex")) {
            QLineEdit* complexInput = new QLineEdit(this);
            // Regex validator for basic complex number formatting.
            QRegularExpression complexRegex(R"(^[-+]?\d+(\.\d+)?([-+]\d+(\.\d+)?[ij])?$)");
            QRegularExpressionValidator* validator = new QRegularExpressionValidator(complexRegex, this);
            complexInput->setValidator(validator);

            if (param.defaultValue.isValid()) {
                // Remove parentheses for a cleaner display.
                complexInput->setText(param.defaultValue.toString().remove('(').remove(')'));
            }
            control = complexInput;
            dxLambda = [complexInput](QVariant& var, bool save) {
                if (save) {
                    QString text = complexInput->text();
                    QRegularExpression complexRegex(R"(^([-+]?\d+(\.\d+)?)([-+]\d+(\.\d+)?)[ij]?$)");
                    QRegularExpressionMatch match = complexRegex.match(text);
                    double real = match.hasMatch() ? match.captured(1).toDouble() : 0.0;
                    double imag = match.hasMatch() ? match.captured(3).toDouble() : 0.0;
                    var = QVariant(QPointF(real, imag));  // Store as QPointF
                }
                else {
                    QPointF c = var.toPointF();
                    // Format using the C locale to ensure a dot as the decimal separator.
                    complexInput->setText(QLocale::c().toString(c.x(), 'f', 6)
                        + "+" + QLocale::c().toString(c.y(), 'f', 6) + "i");
                }
                };
            connect(complexInput, &QLineEdit::textChanged, this, &ScriptEffectSettings::parametersChanged);
        }
        else {
            QLineEdit* lineEdit = new QLineEdit(this);
            lineEdit->setText(param.defaultValue.isValid() ? param.defaultValue.toString() : "");
            control = lineEdit;
            dxLambda = [lineEdit](QVariant& var, bool save) {
                if (save) {
                    var = lineEdit->text();
                }
                else {
                    lineEdit->setText(var.toString());
                }
                };
            connect(lineEdit, &QLineEdit::textChanged, this, &ScriptEffectSettings::parametersChanged);
        }

        // Set the tooltip with parameter description if available.
        if (!param.description.isEmpty()) {
            control->setToolTip(param.description);
        }

        formLayout->addRow(new QLabel(param.fullName, this), control);
        mDataExchange.push_back(dxLambda);
    }

    // Add a spacer to push controls to the top.
    formLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    setLayout(formLayout);

    // Apply initial values from effectSettings to the controls.
    if (mDataExchange.size() == effectSettings.size()) {
        auto it = mDataExchange.begin();
        auto effectIt = effectSettings.begin();
        while (it != mDataExchange.end() && effectIt != effectSettings.end()) {
            (*it)(*effectIt, false);
            ++it;
            ++effectIt;
        }
    }
}

QVariantList ScriptEffectSettings::getEffectSettings()
{
    QVariantList settings;

    // Iterate through the stored data exchange functions and extract values.
    for (auto& exchangeFunc : mDataExchange)
    {
        QVariant value;
        exchangeFunc(value, true);  // `true` means "retrieve value from control"
        settings.append(std::move(value));
    }

    return settings;
}
