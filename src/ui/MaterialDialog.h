#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QColorDialog>
#include "../core/Material.h"

class MaterialDialog : public QDialog {
    Q_OBJECT
public:
    explicit MaterialDialog(const Material& material, QWidget* parent = nullptr);
    Material getMaterial() const;
    
private slots:
    void onAmbientColor();
    void onDiffuseColor();
    void onSpecularColor();
    
private:
    void setupUI();
    
    Material m_material;
    
    QLabel* m_ambientLabel = nullptr;
    QLabel* m_diffuseLabel = nullptr;
    QLabel* m_specularLabel = nullptr;
    QDoubleSpinBox* m_shininessSpin = nullptr;
    
    QPushButton* m_okButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
};
