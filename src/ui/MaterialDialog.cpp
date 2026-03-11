#include "MaterialDialog.h"
#include <QGridLayout>

MaterialDialog::MaterialDialog(const Material& material, QWidget* parent)
    : QDialog(parent), m_material(material)
{
    setupUI();
    setWindowTitle("Material Settings");
    setModal(true);
}

void MaterialDialog::setupUI() {
    auto* layout = new QVBoxLayout(this);
    
    auto* gridLayout = new QGridLayout();
    
    // Ambient color
    m_ambientLabel = new QLabel(this);
    m_ambientLabel->setStyleSheet("background-color: " + QColor::fromRgbF(
        m_material.ambient.x(), m_material.ambient.y(), m_material.ambient.z()).name());
    m_ambientLabel->setMinimumSize(50, 25);
    m_ambientLabel->setFrameStyle(QFrame::Box);
    
    auto* ambientButton = new QPushButton("Ambient Color", this);
    connect(ambientButton, &QPushButton::clicked, this, &MaterialDialog::onAmbientColor);
    
    gridLayout->addWidget(ambientButton, 0, 0);
    gridLayout->addWidget(m_ambientLabel, 0, 1);
    
    // Diffuse color
    m_diffuseLabel = new QLabel(this);
    m_diffuseLabel->setStyleSheet("background-color: " + QColor::fromRgbF(
        m_material.diffuse.x(), m_material.diffuse.y(), m_material.diffuse.z()).name());
    m_diffuseLabel->setMinimumSize(50, 25);
    m_diffuseLabel->setFrameStyle(QFrame::Box);
    
    auto* diffuseButton = new QPushButton("Diffuse Color", this);
    connect(diffuseButton, &QPushButton::clicked, this, &MaterialDialog::onDiffuseColor);
    
    gridLayout->addWidget(diffuseButton, 1, 0);
    gridLayout->addWidget(m_diffuseLabel, 1, 1);
    
    // Specular color
    m_specularLabel = new QLabel(this);
    m_specularLabel->setStyleSheet("background-color: " + QColor::fromRgbF(
        m_material.specular.x(), m_material.specular.y(), m_material.specular.z()).name());
    m_specularLabel->setMinimumSize(50, 25);
    m_specularLabel->setFrameStyle(QFrame::Box);
    
    auto* specularButton = new QPushButton("Specular Color", this);
    connect(specularButton, &QPushButton::clicked, this, &MaterialDialog::onSpecularColor);
    
    gridLayout->addWidget(specularButton, 2, 0);
    gridLayout->addWidget(m_specularLabel, 2, 1);
    
    // Shininess
    auto* shininessLabel = new QLabel("Shininess:", this);
    m_shininessSpin = new QDoubleSpinBox(this);
    m_shininessSpin->setRange(1.0, 128.0);
    m_shininessSpin->setValue(m_material.shininess);
    
    gridLayout->addWidget(shininessLabel, 3, 0);
    gridLayout->addWidget(m_shininessSpin, 3, 1);
    
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

Material MaterialDialog::getMaterial() const {
    Material mat = m_material;
    mat.shininess = m_shininessSpin->value();
    return mat;
}

void MaterialDialog::onAmbientColor() {
    QColor color = QColorDialog::getColor(QColor::fromRgbF(
        m_material.ambient.x(), m_material.ambient.y(), m_material.ambient.z()), this);
    if (color.isValid()) {
        m_material.ambient = QVector3D(color.redF(), color.greenF(), color.blueF());
        m_ambientLabel->setStyleSheet("background-color: " + color.name());
    }
}

void MaterialDialog::onDiffuseColor() {
    QColor color = QColorDialog::getColor(QColor::fromRgbF(
        m_material.diffuse.x(), m_material.diffuse.y(), m_material.diffuse.z()), this);
    if (color.isValid()) {
        m_material.diffuse = QVector3D(color.redF(), color.greenF(), color.blueF());
        m_diffuseLabel->setStyleSheet("background-color: " + color.name());
    }
}

void MaterialDialog::onSpecularColor() {
    QColor color = QColorDialog::getColor(QColor::fromRgbF(
        m_material.specular.x(), m_material.specular.y(), m_material.specular.z()), this);
    if (color.isValid()) {
        m_material.specular = QVector3D(color.redF(), color.greenF(), color.blueF());
        m_specularLabel->setStyleSheet("background-color: " + color.name());
    }
}
