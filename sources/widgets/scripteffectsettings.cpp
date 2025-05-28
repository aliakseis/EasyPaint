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
        // For easier comparisons, convert the annotation to lower case.
        QString annotationLower = param.annotation.toLower();
        QString paramNameLower = param.name.toLower();

        // We'll prepare a lambda for data exchange.
        std::function<void(QVariant&, bool)> dxLambda;

        if (annotationLower.contains("int")) {
            QSpinBox* spinBox = new QSpinBox(this);
            spinBox->setRange(0, 100); // Adjust range as needed.
            if (param.defaultValue.isValid())
                spinBox->setValue(param.defaultValue.toInt());
            control = spinBox;
            dxLambda = [spinBox](QVariant& var, bool save) {
                if (save)
                    var = spinBox->value();
                else
                    spinBox->setValue(var.toInt());
                };
        }
        // Numeric types: float/double.
        else if (annotationLower.contains("float") || annotationLower.contains("double")) {
            QDoubleSpinBox* doubleSpinBox = new QDoubleSpinBox(this);
            doubleSpinBox->setRange(0.0, 100.0); // Adjust range as needed.
            doubleSpinBox->setDecimals(2);
            if (param.defaultValue.isValid())
                doubleSpinBox->setValue(param.defaultValue.toDouble());
            control = doubleSpinBox;
            dxLambda = [doubleSpinBox](QVariant& var, bool save) {
                if (save)
                    var = doubleSpinBox->value();
                else
                    doubleSpinBox->setValue(var.toDouble());
                };
        }
        // Boolean type.
        else if (annotationLower.contains("bool")) {
            QCheckBox* checkBox = new QCheckBox(this);
            if (param.defaultValue.isValid())
                checkBox->setChecked(param.defaultValue.toBool());
            control = checkBox;
            dxLambda = [checkBox](QVariant& var, bool save) {
                if (save)
                    var = checkBox->isChecked();
                else
                    checkBox->setChecked(var.toBool());
                };
        }
        // String type.
        else if (annotationLower.contains("str")) {
            QLineEdit* lineEdit = new QLineEdit(this);
            if (param.defaultValue.isValid())
                lineEdit->setText(param.defaultValue.toString());
            control = lineEdit;
            dxLambda = [lineEdit](QVariant& var, bool save) {
                if (save)
                    var = lineEdit->text();
                else
                    lineEdit->setText(var.toString());
                };
        }
        // Tuple type: for generic purposes, use a QLineEdit to accept comma-separated input.
        else if (annotationLower.contains("tuple")) {
            QLineEdit* tupleEdit = new QLineEdit(this);
            if (param.defaultValue.isValid())
                tupleEdit->setText(param.defaultValue.toString());
            control = tupleEdit;
            dxLambda = [tupleEdit](QVariant& var, bool save) {
                if (save)
                    var = tupleEdit->text(); // You could further parse the text into a QVariantList if desired.
                else
                    tupleEdit->setText(var.toString());
                };
        }
        // Fallback: use a QLineEdit.
        else {
            QLineEdit* lineEdit = new QLineEdit(this);
            if (param.defaultValue.isValid())
                lineEdit->setText(param.defaultValue.toString());
            control = lineEdit;
            dxLambda = [lineEdit](QVariant& var, bool save) {
                if (save)
                    var = lineEdit->text();
                else
                    lineEdit->setText(var.toString());
                };
        }

        // Optionally set a tooltip based on parameter description.
        if (!param.description.isEmpty())
            control->setToolTip(param.description);

        // Add the label and control to the form layout.
        formLayout->addRow(new QLabel(param.fullName, this), control);

        // Save the data exchange lambda.
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
