#include "mainwindow.h"
#include <QtWidgets>
#include <QDoubleValidator>
#include <QLocale>
#include <QFontDatabase>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    buildUI();
    connect(m_scheme, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onSchemeChanged);
    syncEnableStates();
    setWindowTitle("Расчёт шпунта методом предельных равновесий");
    resize(960, 540);
}

QLineEdit* MainWindow::addNum(QFormLayout* form, const QString& key,
                              const QString& labelText, double def,
                              const QString& tip) {
    auto* label = new QLabel(labelText);
    auto* edit  = new QLineEdit(QString::number(def, 'g', 12));
    auto* v = new QDoubleValidator;
    v->setNotation(QDoubleValidator::ScientificNotation);
    v->setLocale(QLocale::c());
    v->setBottom(-1e20);
    v->setTop(1e20);
    edit->setValidator(v);
    edit->setMinimumWidth(160);
    if (!tip.isEmpty()) { edit->setToolTip(tip); label->setToolTip(tip); }
    form->addRow(label, edit);
    m_fields[key] = edit;
    connect(edit, &QLineEdit::returnPressed, this, &MainWindow::onCalculate);
    return edit;
}
double MainWindow::getVal(const QString& key) const {
    bool ok; double v = m_fields[key]->text().toDouble(&ok);
    return ok ? v : 0.0;
}
void MainWindow::setVal(const QString& key, double v) {
    m_fields[key]->setText(QString::number(v, 'g', 12));
}

void MainWindow::buildUI() {
    auto* central = new QWidget;
    auto* rootLayout = new QVBoxLayout(central);
    auto* splitter = new QSplitter(Qt::Horizontal);
    rootLayout->addWidget(splitter, 1);

    // ---------------- LEFT: parameter tabs ----------------
    auto* paramTabs = new QTabWidget;
    splitter->addWidget(paramTabs);

    // ===== Tab: Схема =====
    {
        auto* w = new QWidget; auto* v = new QVBoxLayout(w);

        auto* sg = new QGroupBox("Расчётная схема");
        auto* sf = new QFormLayout(sg);
        m_scheme = new QComboBox;
        m_scheme->addItem("Консоль (без распорок)", "console");
        m_scheme->addItem("Распорка",                "strut");
        m_scheme->addItem("Прямоугольная обвязка",   "frame");
        m_scheme->addItem("Круговая обвязка",        "ring");
        sf->addRow("Схема:", m_scheme);
        v->addWidget(sg);

        auto* gg = new QGroupBox("Глубина котлована");
        auto* gf = new QFormLayout(gg);
        addNum(gf, "h", "h, м", 3.0, "Глубина котлована (м)");
        v->addWidget(gg);


        auto* ig = new QGroupBox("Итерационный подбор глубины заделки");
        auto* itf = new QFormLayout(ig);
        addNum(itf, "h_start", "h_start, м (начальная глубина перебора)", 0.10, "Начальная глубина перебора");
        addNum(itf, "h_step",  "h_step, м (шаг перебора)",  0.25, "Шаг перебора (должен быть > 0)");
        addNum(itf, "h_max",   "h_max, м (верхняя граница перебора)",   12.0, "Верхняя граница перебора");
        v->addWidget(ig);

        v->addStretch();
        paramTabs->addTab(w, "Схема");
    }

    // ===== Tab: Грунт =====
    {
        auto* w = new QWidget; auto* v = new QVBoxLayout(w);

        auto* g = new QGroupBox("Характеристики грунта");
        auto* f = new QFormLayout(g);
        addNum(f, "gamma", "γ, кН/м³ (удельный вес грунта)", 18.0, "Удельный вес грунта");
        addNum(f, "phi",   "φ, ° (угол внутренего трения)",     30.0, "Угол внутреннего трения (0…90)");
        addNum(f, "q",     "q, кПа (пригрузка на поверхности)",   0.0,  "Пригрузка на поверхности");
        addNum(f, "c",     "c, кПа (удельное сцепление)",   0.0,  "Удельное сцепление");
        v->addWidget(g);

        auto* g2 = new QGroupBox("Коэффициенты условий работы");
        auto* f2 = new QFormLayout(g2);
        addNum(f2, "m",       "m (коэффициент условий работ по СП 22.13330)",  0.95, "Коэффициент условий работы (СП 22.13330)");
        addNum(f2, "gamma_c", "γc (коэффициент условий работ стали)", 1.0,  "Коэффициент условий работы стали");
        v->addWidget(g2);

        auto* g3 = new QGroupBox("Коэффициенты несущей способности (СП 22.13330)");
        auto* f3 = new QFormLayout(g3);
        addNum(f3, "N_gamma", "N_γ", 15.0, "Зависит от φ; уточните по таблицам СП");
        addNum(f3, "N_c",     "N_c", 30.0);
        addNum(f3, "N_q",     "N_q", 18.0);
        v->addWidget(g3);

        v->addStretch();
        paramTabs->addTab(w, "Грунт");
    }

    // ===== Tab: Шпунт =====
    {
        auto* w = new QWidget; auto* v = new QVBoxLayout(w);
        auto* g = new QGroupBox("Характеристики шпунта (на 1 м стенки)");
        auto* f = new QFormLayout(g);
        addNum(f, "E",     "E, кПа",   2.06e8,   "Модуль упругости (сталь 2.06e8)");
        addNum(f, "I",     "I, м⁴/м",  3.0e-4,   "Момент инерции");
        addNum(f, "W",     "W, м³/м",  1.67e-3,  "Момент сопротивления");
        addNum(f, "Ry",    "Ry, кПа",  240000.0, "Расчётное сопротивление стали");
        addNum(f, "y_lim", "y_lim, м (допустимый прогиб)", 0.03,     "Предельный прогиб верха стенки");
        v->addWidget(g);
        v->addStretch();
        paramTabs->addTab(w, "Шпунт");
    }

    // ===== Tab: Усиление =====
    {
        auto* w = new QWidget; auto* v = new QVBoxLayout(w);

        m_supportGroup = new QGroupBox("Уровень установки раскрепления от верха стенки/котлована");
        auto* supF = new QFormLayout(m_supportGroup);
        addNum(supF, "support_z", "support_z, м", 0.0,
               "Уровень крепления распорки/обвязки от верха стенки/котлована");
        v->addWidget(m_supportGroup);

        m_strutGroup = new QGroupBox("Распорка (схема «strut»)");
        auto* f1 = new QFormLayout(m_strutGroup);
        addNum(f1, "strut_s",  "Шаг, м",  4.0);
        addNum(f1, "strut_A",  "A, м²",   56e-4);
        addNum(f1, "strut_Rc", "Rc, кПа", 200000.0);
        v->addWidget(m_strutGroup);

        m_frameGroup = new QGroupBox("Прямоугольная обвязка (схема «frame»)");
        auto* f2 = new QFormLayout(m_frameGroup);
        addNum(f2, "frame_B",  "Пролёт B, м", 6.0);
        addNum(f2, "frame_W",  "W, м³",       242e-6);
        addNum(f2, "frame_Ry", "Ry, кПа",     240000.0);
        addNum(f2, "frame_kM", "kM (доля момента на обвязке)", 0.6, // FIX
               "Коэффициент распределения момента (1.0 - свободно опертая балка, 0.5-0.7 - жесткий рамный узел)");
        v->addWidget(m_frameGroup);

        m_ringGroup = new QGroupBox("Круговая обвязка (схема «ring»)");
        auto* f3 = new QFormLayout(m_ringGroup);
        addNum(f3, "ring_R", "Радиус R, м", 8.0);
        addNum(f3, "ring_A", "A, м²",       61.2e-4);
        v->addWidget(m_ringGroup);

        v->addStretch();
        paramTabs->addTab(w, "Усиление");
    }

    // ---------------- RIGHT: results ----------------
    auto* resTabs = new QTabWidget;
    auto mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    //m_summary = new QTextEdit; m_summary->setReadOnly(true); m_summary->setFont(mono);
    m_summary = new QTextEdit;
    m_summary->setReadOnly(true);
    m_summary->setFont(mono);
    m_summary->setPlainText(
        "Здесь будет результат расчёта.\n\n"
        "1. Выберите расчётную схему из выпадающего списка.\n"
        "2. В левой части по очереди заполните все вкладки вашими параметрами. \n"
        "   Или нажмите «Загрузить параметры…» если у вас есть готовый файл с заполненными параметрами.\n"
        "3. Нажмите «Рассчитать» (или Enter в любом поле).");
    resTabs->addTab(m_summary, "Результат");

    m_log = new QTextEdit; m_log->setReadOnly(true); m_log->setFont(mono);
    resTabs->addTab(m_log, "Журнал итераций");
    splitter->addWidget(resTabs);
    splitter->setSizes({560, 640});

    // ---------------- Bottom buttons ----------------
    auto* btnRow = new QHBoxLayout;
    auto* btnCalc    = new QPushButton("▶ Рассчитать");
    auto* btnDef     = new QPushButton("Сброс к умолчаниям");
    auto* btnLoad    = new QPushButton("Загрузить параметры…");
    auto* btnSave    = new QPushButton("Сохранить параметры…");
    auto* btnSaveRes = new QPushButton("Сохранить результат…");
    auto* btnClose   = new QPushButton("Закрыть");
    btnCalc->setDefault(true);
    btnCalc->setStyleSheet("font-weight: bold; padding: 6px 18px;");
    btnRow->addWidget(btnCalc);
    btnRow->addStretch();
    btnRow->addWidget(btnDef);
    btnRow->addWidget(btnLoad);
    btnRow->addWidget(btnSave);
    btnRow->addWidget(btnSaveRes);
    btnRow->addWidget(btnClose);
    rootLayout->addLayout(btnRow);

    setCentralWidget(central);
    statusBar()->showMessage("Готово к расчёту");

    connect(btnCalc,    &QPushButton::clicked, this, &MainWindow::onCalculate);
    connect(btnDef,     &QPushButton::clicked, this, &MainWindow::onDefaults);
    connect(btnLoad,    &QPushButton::clicked, this, &MainWindow::onLoadParams);
    connect(btnSave,    &QPushButton::clicked, this, &MainWindow::onSaveParams);
    connect(btnSaveRes, &QPushButton::clicked, this, &MainWindow::onSaveResult);
    connect(btnClose,   &QPushButton::clicked, this, &QWidget::close);
}

void MainWindow::syncEnableStates() {
    QString s = m_scheme->currentData().toString();
    bool isConsole = (s == "console");
    m_supportGroup->setEnabled(!isConsole);
    m_strutGroup->setEnabled(s == "strut");
    m_frameGroup->setEnabled(s == "frame");
    m_ringGroup->setEnabled(s == "ring");
}

void MainWindow::onSchemeChanged() { syncEnableStates(); }

Params MainWindow::readParams() const {
    Params P;
    P.scheme    = m_scheme->currentData().toString();
    P.h         = getVal("h");
    P.support_z = getVal("support_z");
    P.gamma     = getVal("gamma");
    P.phi       = getVal("phi");
    P.q         = getVal("q");
    P.c         = getVal("c");
    P.m         = getVal("m");
    P.gamma_c   = getVal("gamma_c");
    P.E         = getVal("E");
    P.I         = getVal("I");
    P.W         = getVal("W");
    P.Ry        = getVal("Ry");
    P.y_lim     = getVal("y_lim");
    P.h_start   = getVal("h_start");
    P.h_step    = getVal("h_step");
    P.h_max     = getVal("h_max");
    P.strut_s   = getVal("strut_s");
    P.strut_A   = getVal("strut_A");
    P.strut_Rc  = getVal("strut_Rc");
    P.ring_R    = getVal("ring_R");
    P.ring_A    = getVal("ring_A");
    P.frame_B   = getVal("frame_B");
    P.frame_W   = getVal("frame_W");
    P.frame_Ry  = getVal("frame_Ry");
    P.frame_kM   = getVal("frame_kM");
    P.N_gamma   = getVal("N_gamma");
    P.N_c       = getVal("N_c");
    P.N_q       = getVal("N_q");
    return P;
}

void MainWindow::writeParams(const Params& P) {
    m_scheme->setCurrentIndex(m_scheme->findData(P.scheme));
    setVal("h", P.h);            setVal("support_z", P.support_z);
    setVal("gamma", P.gamma);    setVal("phi", P.phi);
    setVal("q", P.q);            setVal("c", P.c);
    setVal("m", P.m);            setVal("gamma_c", P.gamma_c);
    setVal("E", P.E);            setVal("I", P.I);
    setVal("W", P.W);            setVal("Ry", P.Ry);
    setVal("y_lim", P.y_lim);
    setVal("h_start", P.h_start); setVal("h_step", P.h_step); setVal("h_max", P.h_max);
    setVal("strut_s", P.strut_s); setVal("strut_A", P.strut_A); setVal("strut_Rc", P.strut_Rc);
    setVal("ring_R", P.ring_R);   setVal("ring_A", P.ring_A);
    setVal("frame_B", P.frame_B); setVal("frame_W", P.frame_W); setVal("frame_Ry", P.frame_Ry); setVal("frame_kM", P.frame_kM);
    setVal("N_gamma", P.N_gamma); setVal("N_c", P.N_c); setVal("N_q", P.N_q);
    syncEnableStates();
}

bool MainWindow::validateInputs(QString& err) const {
    for (auto it = m_fields.constBegin(); it != m_fields.constEnd(); ++it) {
        bool ok; it.value()->text().toDouble(&ok);
        if (!ok) {
            err = QString("Некорректное число в поле «%1»: «%2»")
                      .arg(it.key(), it.value()->text());
            return false;
        }
    }
    Params P = readParams();
    if (P.h <= 0)         { err = "h должно быть > 0"; return false; }
    if (P.gamma <= 0)     { err = "γ должно быть > 0"; return false; }
    if (P.phi <= 0 || P.phi >= 90) { err = "φ должно быть в диапазоне 0…90°"; return false; }
    if (P.h_step <= 0)    { err = "h_step должен быть > 0"; return false; }
    if (P.h_max <= P.h_start) { err = "h_max должно быть больше h_start"; return false; }
    return true;
}

void MainWindow::onCalculate() {
    QString err;
    if (!validateInputs(err)) {
        QMessageBox::warning(this, "Некорректные данные", err);
        return;
    }
    statusBar()->showMessage("Выполняется расчёт…");
    QApplication::setOverrideCursor(Qt::WaitCursor);

    Params P = readParams();
    m_lastResult = CalcEngine::calculate(P);

    QApplication::restoreOverrideCursor();

    m_log->setPlainText(m_lastResult.log.join("\n"));

    if (!m_lastResult.found) {
        m_summary->setPlainText("!!! РЕШЕНИЕ НЕ НАЙДЕНО\n\n" + m_lastResult.error);
        statusBar()->showMessage("Решение не найдено");
        QMessageBox::warning(this, "Расчёт",
                             m_lastResult.error + "\n\nСм. журнал итераций.");
        return;
    }
    m_summary->setPlainText(m_lastResult.summary);
    statusBar()->showMessage(
        QString("Расчёт завершён: h = %1 м").arg(m_lastResult.h_found, 0, 'f', 3));
}

void MainWindow::onDefaults() {
    writeParams(CalcEngine::defaults());
    m_summary->clear();
    m_log->clear();
    statusBar()->showMessage("Параметры сброшены к умолчаниям");
}

void MainWindow::onLoadParams() {
    QString startDir = m_lastPath.isEmpty() ? QDir::currentPath() : m_lastPath;
    QString f = QFileDialog::getOpenFileName(this,
                                             "Загрузить параметры", startDir,
                                             "Текстовые файлы (*.txt);;Все файлы (*)");
    if (f.isEmpty()) return;
    Params P; QString err;
    if (!CalcEngine::loadFromFile(f, P, err)) {
        QMessageBox::critical(this, "Ошибка", err); return;
    }
    writeParams(P);
    m_lastPath = f;
    statusBar()->showMessage("Загружено: " + f);
}

void MainWindow::onSaveParams() {
    QString startDir = m_lastPath.isEmpty() ? QDir::currentPath() : m_lastPath;
    QString f = QFileDialog::getSaveFileName(this,
                                             "Сохранить параметры", startDir, "Текстовые файлы (*.txt)");
    if (f.isEmpty()) return;
    QString err;
    if (!CalcEngine::saveToFile(f, readParams(), err)) {
        QMessageBox::critical(this, "Ошибка", err); return;
    }
    m_lastPath = f;
    statusBar()->showMessage("Сохранено: " + f);
}

void MainWindow::onSaveResult() {
    if (m_summary->toPlainText().isEmpty()) {
        QMessageBox::information(this, "Нет результата",
                                 "Сначала выполните расчёт.");
        return;
    }
    QString f = QFileDialog::getSaveFileName(this,
                                             "Сохранить результат", QDir::currentPath() + "/result.txt",
                                             "Текстовые файлы (*.txt)");
    if (f.isEmpty()) return;
    QFile file(f);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось открыть файл"); return;
    }
    QTextStream ts(&file);
    ts << m_summary->toPlainText() << "\n\n";
    ts << ">>> ЖУРНАЛ ИТЕРАЦИЙ\n\n";
    ts << m_log->toPlainText() << "\n";
    statusBar()->showMessage("Сохранено: " + f);
}