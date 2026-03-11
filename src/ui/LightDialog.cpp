#include "LightDialog.h"
#include <QGridLayout>

LightDialog::LightDialog(const Light& light, QWidget* parent)
    : QDialog(parent), m_light(light)
{
    setupUI();
    setWindowTitle("Light Settings");
    setModal(true);
}

void LightDialog::setupUI() {
    auto* layout = new QVBoxLayout(this);
    
    auto* gridLayout = new QGridLayout();
    
    // Light color
    m_colorLabel = new QLabel(this);
    m_colorLabel->setStyleSheet("background-color: " + QColor::fromRgbF(
        m_light.color.x(), m_light.color.y(), m_light.color.z()).name());
    m_colorLabel->setMinimumSize(50, 25);
    m_colorLabel->setFrameStyle(QFrame::Box);
    
    auto* colorButton = new QPushButton("Light Color", this);
    connect(colorButton, &QPushButton::clicked, this, &LightDialog::onLightColor);
    
    gridLayout->addWidget(colorButton, 0, 0);
    gridLayout->addWidget(m_colorLabel, 0, 1);
    
    // Position X
    auto* posXLabel = new QLabel("Position X:", this);
    m_posXSpin = new QDoubleSpinBox(this);
    m_posXSpin->setRange(-100.0, 100.0);
    m_posXSpin->setValue(m_light.position.x());
    m_posXSpin->setSingleStep(0.1);
    
    gridLayout->addWidget(posXLabel, 1, 0);
    gridLayout->addWidget(m_posXSpin, 1, 1);
    
    // Position Y
    auto* posYLabel = new QLabel("Position Y:", this);
    m_posYSpin = new QDoubleSpinBox(this);
    m_posYSpin->setRange(-100.0, 100.0);
    m_posYSpin->setValue(m_light.position.y());
    m_posYSpin->setSingleStep(0.1);
    
    gridLayout->addWidget(posYLabel, 2, 0);
    gridLayout->addWidget(m_posYSpin, 2, 1);
    
    // Position Z
    auto* posZLabel = new QLabel("Position Z:", this);
    m_posZSpin = new QDoubleSpinBox(this);
    m_posZSpin->setRange(-100.0, 100.0);
    m_posZSpin->setValue(m_light.position.z());
    m_posZSpin->setSingleStep(0.1);
    
    gridLayout->addWidget(posZLabel, 3, 0);
    gridLayout->addWidget(m_posZSpin, 3, 1);
    
    layout->addLayout(gridLayout);
    
    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    m_okButton = new QPushButton("OK", this);
    m_cancelButton = new QPushButton("Cancel", this);
    
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);
    
    connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    
    layout->addLayout(buttonLayout);
}

Light LightDialog::getLight() const {
    Light light = m_light;
    light.position = QVector3D(m_posXSpin->value(), m_posYSpin->value(), m_posZSpin->value());
    return light;
}

void LightDialog::onLightColor() {
    QColor color = QColorDialog::getColor(QColor::fromRgbF(
        m_light.color.x(), m_light.color.y(), m_light.color.z()), this);
    if (color.isValid()) {
        m_light.color = QVector3D(color.redF(), color.greenF(), color.blueF());
        m_colorLabel->setStyleSheet("background-color: " + color.name());
    }
}
