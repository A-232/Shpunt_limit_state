#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QString>
#include "calcengine.h"

class QComboBox;
class QLineEdit;
class QTextEdit;
class QGroupBox;
class QFormLayout;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onSchemeChanged();
    void onCalculate();
    void onSaveResult();
    void onLoadParams();
    void onSaveParams();
    void onDefaults();

private:
    void buildUI();
    void syncEnableStates();
    Params readParams() const;
    void   writeParams(const Params& P);
    bool   validateInputs(QString& err) const;

    QLineEdit* addNum(QFormLayout* form, const QString& key,
                      const QString& labelText, double def,
                      const QString& tip = "");
    double getVal(const QString& key) const;
    void   setVal(const QString& key, double v);

    QComboBox*                m_scheme = nullptr;
    QMap<QString, QLineEdit*> m_fields;
    QGroupBox*                m_supportGroup = nullptr;
    QGroupBox*                m_strutGroup   = nullptr;
    QGroupBox*                m_frameGroup   = nullptr;
    QGroupBox*                m_ringGroup    = nullptr;
    QTextEdit*                m_summary      = nullptr;
    QTextEdit*                m_log          = nullptr;
    CalcResult                m_lastResult;
    QString                   m_lastPath;
};

#endif