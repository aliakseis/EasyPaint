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
    // Use a form layout to pair each parameter label with an input control.
    QFormLayout* formLayout = new QFormLayout(this);

    // Iterate over each parameter from the function info.
    for (int i = 1; i < functionInfo.parameters.size(); ++i) {
        const auto& param = functionInfo.parameters[i];
        QWidget* control = nullptr;
        // To make type comparisons easier, convert the annotation to lower case.
        QString annotationLower = param.annotation.toLower();

        // Check for common type indications.
        if (annotationLower.contains("int")) {
            // If the parameter is an integer, create a QSpinBox.
            QSpinBox* spinBox = new QSpinBox(this);
            spinBox->setMinimum(0);   // You might want to set dynamic ranges.
            spinBox->setMaximum(100);
            if (param.defaultValue.isValid())
                spinBox->setValue(param.defaultValue.toInt());
            control = spinBox;
        }
        else if (annotationLower.contains("float") || annotationLower.contains("double")) {
            // For float/double parameters, use QDoubleSpinBox.
            QDoubleSpinBox* doubleSpinBox = new QDoubleSpinBox(this);
            doubleSpinBox->setMinimum(0.0);
            doubleSpinBox->setMaximum(100.0);
            doubleSpinBox->setDecimals(2);
            if (param.defaultValue.isValid())
                doubleSpinBox->setValue(param.defaultValue.toDouble());
            control = doubleSpinBox;
        }
        else if (annotationLower.contains("bool")) {
            // For booleans, a QCheckBox works naturally.
            QCheckBox* checkBox = new QCheckBox(this);
            if (param.defaultValue.isValid())
                checkBox->setChecked(param.defaultValue.toBool());
            control = checkBox;
        }
        else if (annotationLower.contains("str")) {
            // For strings, use a QLineEdit.
            QLineEdit* lineEdit = new QLineEdit(this);
            if (param.defaultValue.isValid())
                lineEdit->setText(param.defaultValue.toString());
            control = lineEdit;
        }
        else {
            // Fallback: use a QLineEdit for any other type.
            QLineEdit* lineEdit = new QLineEdit(this);
            if (param.defaultValue.isValid())
                lineEdit->setText(param.defaultValue.toString());
            control = lineEdit;
        }

        // Optionally, you could add tooltips or additional properties based on param.description.
        if (!param.description.isEmpty()) {
            control->setToolTip(param.description);
        }
        // Add label and the control to the form layout.
        formLayout->addRow(new QLabel(param.fullName, this), control);
    }

    // Add a spacer to push the controls to the top.
    formLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    // Set the layout.
    setLayout(formLayout);
}

QList<QVariant> ScriptEffectSettings::getEffectSettings()
{
    return QList<QVariant>();
}
