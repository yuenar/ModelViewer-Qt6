#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QColorDialog>
#include "../core/Light.h"

class LightDialog : public QDialog {
    Q_OBJECT
public:
    explicit LightDialog(const Light& light, QWidget* parent = nullptr);
    Light getLight() const;
    
private slots:
    void onLightColor();
    
private:
    void setupUI();
    
    Light m_light;
    
    QLabel* m_colorLabel = nullptr;
    QDoubleSpinBox* m_posXSpin = nullptr;
    QDoubleSpinBox* m_posYSpin = nullptr;
    QDoubleSpinBox* m_posZSpin = nullptr;
    
    QPushButton* m_okButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
};
