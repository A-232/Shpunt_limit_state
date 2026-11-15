#include "calcengine.h"
#include <cmath>
#include <QFile>
#include <QTextStream>
#include <QMap>

static const double PI_ = 3.14159265358979;
struct Coef { double A, B; };

/* Проверка M_opr ≤ m·M_ud с m = 0.95 означает: допускается мобилизовать 95 %
 * теоретического пассивного отпора. При этом порог ровно m = 0.75. Для распорной
 * схемы при Na > 0 (распорка только на сжатие) геометрия требует h < H/2. Если
 * подставить это в моментное равновесие, получается: при m < 0.75 решения нет ни
 * при какой H, при m > 0.75 решение есть всегда. Никакое одно значение m не даст
 * нам «МПС держит при H=5, но не держит при H=6» — это противоречит самой структуре
 * метода свободного опирания.Поэтому искусственно введено минимальное отношение h/H
 * (g_h_ratio_min) параметр h_ratio_min (новая проверка h/H ≥ 0.45) — это эмпирическая
 * заплатка, которая отсекает те решения МПС (метод предельных состояний), где заделка
 * слишком мала для реальной ситуации. Т.е.если МПС всё ещё «держит», но МКЭ (метод
 * конечных элементов) уже валит — следует поднять h_ratio_min до 0.5–0.55; если МПС
 * начал валить случай, который МКЭ держит — опустите до 0.4 или ниже.
    Значение 0.45 — стартовая точка для песка;
    для глинистых грунтов (c > 0) имеет смысл 0.5–0.6.*/
static double g_h_ratio_min = 0.45;

static double calcKa(double phi_deg) {
    double phi = phi_deg * PI_ / 180.0;
    return std::pow(std::tan(PI_/4 - phi/2), 2);
}
static double calcKp(double phi_deg) {
    double phi = phi_deg * PI_ / 180.0;
    return std::pow(std::tan(PI_/4 + phi/2), 2);
}
static Coef activeCoef(const Params& P, double Kav) {
    Coef c; c.A = P.gamma * Kav;
    c.B = P.q * Kav - 2.0 * P.c * std::sqrt(Kav);
    return c;
}
static Coef passiveCoef(const Params& P, double Kpv) {
    Coef c; c.A = P.gamma * Kpv;
    c.B = 2.0 * P.c * std::sqrt(Kpv);
    return c;
}

// FIX #1: зона отрыва (B<0) — интегрируем от z0 = -B/A
static void forceCentroid(const Coef& c, double L, double& F, double& zc) {
    double z0 = 0.0;
    if (c.A > 1e-12 && c.B < 0) z0 = -c.B / c.A;
    if (z0 >= L) { F = 0.0; zc = 0.0; return; }
    F = c.A * (L*L - z0*z0) / 2.0 + c.B * (L - z0);
    double M0 = c.A * (L*L*L - z0*z0*z0) / 3.0 + c.B * (L*L - z0*z0) / 2.0;
    zc = (std::fabs(F) > 1e-12) ? M0 / F : 0.0;
}

static double MmaxNumerical(double h, const Params& P, const Coef& ca, const Coef& cp,
                            double Na, double& z_out) {
    double L = P.h + h, Mmax = 0.0; z_out = 0.0;
    const int N = 1000;
    // FIX #1: зона отрыва активного давления
    double z0a = (ca.A > 1e-12 && ca.B < 0) ? -ca.B / ca.A : 0.0;
    for (int i = 0; i <= N; ++i) {
        double z = L * i / double(N);
        double Ma = 0.0;
        if (z > z0a) {
            double u = z - z0a;
            // ∫_{z0a}^{z} (A·ξ+B)·(z-ξ)dξ  — аналитически
            Ma = ca.A * u*u*(z + 2.0*z0a) / 6.0 + ca.B * u*u / 2.0;
        }
        double M = -Ma;
        if (Na > 0 && z > P.support_z) M += Na * (z - P.support_z);
        if (z > P.h) {
            double zp = z - P.h;
            M += cp.A * zp*zp*zp / 6.0 + cp.B * zp*zp / 2.0;
        }
        if (std::fabs(M) > std::fabs(Mmax)) { Mmax = M; z_out = z; }
    }
    return Mmax;
}

struct SoilCheck { double maxRatio = 0, z_crit = 0, p_act = 0, p_lim = 0; bool ok = true; };

// FIX #3: физически осмысленная проверка. Активное давление берём по полной
// эпюре (с учётом γ·(H+z)), несущую способность — по стандартной формуле
// с загружением от вышележащего грунта (q + γ·H)·Nq.
static SoilCheck checkSoil(const Params& P, double h, double Kav) {
    SoilCheck r; const int N = 100;
    double sqrtKa = std::sqrt(Kav);
    double z0a = (P.gamma > 1e-9) ? (2.0*P.c/sqrtKa - P.q)/P.gamma : 0.0;
    if (z0a < 0) z0a = 0;
    for (int i = 1; i <= N; ++i) {
        double z = h * i / double(N);
        double ztop = P.h + z;
        double p_active = 0.0;
        if (ztop > z0a) {
            p_active = (P.gamma*ztop + P.q)*Kav - 2.0*P.c*sqrtKa;
            if (p_active < 0) p_active = 0;
        }
        double p_bearing = P.gamma*z*P.N_gamma + P.c*P.N_c
                           + (P.q + P.gamma*P.h)*P.N_q;
        double ratio = (p_bearing > 1e-9) ? p_active / p_bearing : 999.0;
        if (ratio > r.maxRatio) {
            r.maxRatio = ratio; r.z_crit = z;
            r.p_act = p_active; r.p_lim = p_bearing;
        }
    }
    r.ok = (r.maxRatio <= 1.0);
    return r;
}

static QString fn(double v, int prec = 4) {
    return QString::number(v, 'f', prec);
}
static QString fg(double v) { return QString::number(v, 'g', 6); }

CalcResult CalcEngine::calculate(const Params& P) {
    CalcResult R;
    double Kav = calcKa(P.phi), Kpv = calcKp(P.phi);
    R.Ka = Kav; R.Kp = Kpv;
    Coef ca = activeCoef(P, Kav), cp = passiveCoef(P, Kpv);
    bool isStrutScheme = (P.scheme != "console");
    bool found = false;
    double h_found = -1.0;

    int iter = 0;
    for (double h = P.h_start; h <= P.h_max + 1e-9; h += P.h_step) {
        if (++iter > 5000) break;
        double L = P.h + h;
        double Ea, za, Ep, zp;
        forceCentroid(ca, L, Ea, za);
        forceCentroid(cp, h, Ep, zp);

        double M_opr, M_ud;
        if (P.scheme == "console") {
            M_opr = Ea * (L - za);
            M_ud  = Ep * (h - zp);
        } else {
            M_opr = Ea * (za - P.support_z);
            M_ud  = Ep * ((P.h - P.support_z) + zp);
        }
        bool eq_ok = (M_opr <= P.m * M_ud + 1e-6);

        double Na = (P.scheme == "console") ? 0.0 : (Ea - Ep);
        bool na_ok = !isStrutScheme || (Na > 1e-9);

        double zM;
        double Mmax = MmaxNumerical(h, P, ca, cp, Na, zM);
        double sigma = std::fabs(Mmax) / P.W;
        bool str_ok = (sigma <= P.Ry * P.gamma_c + 1e-6);

        SoilCheck sc = checkSoil(P, h, Kav);
        bool soil_ok = sc.ok;

        // FIX #2: прогиб.
        // console — консоль с треугольной нагрузкой: y = q_max·H⁴/(30EI)
        // propped — шарнирно-опёртая с треугольной: 5·q_avg·L⁴/(384EI)
        double qa_H = ca.A * P.h + ca.B;
        if (qa_H < 0) qa_H = 0;
        double y_max;
        if (P.scheme == "console") {
            y_max = qa_H * std::pow(P.h, 4) / (30.0 * P.E * P.I);
        } else {
            double q_avg = 0.5 * qa_H;
            double l_span = (P.h - P.support_z > 1e-9) ? (P.h - P.support_z) : P.h;
            y_max = 5.0 * q_avg * std::pow(l_span, 4) / (384.0 * P.E * P.I);
        }
        bool defl_ok = (y_max <= P.y_lim + 1e-9);

        // NEW: проверка минимального относительного заглубления.
        // Ниже h/H ≈ 0.42 МПС-решение существует, но пассив не мобилизуется
        // (МКЭ в этой зоне показывает недопустимые перемещения/моменты).
        double h_ratio = (P.h > 1e-9) ? h / P.h : 0.0;
        bool h_ratio_ok = (h_ratio + 1e-9 >= g_h_ratio_min);

        bool sup_ok = true;
        double N_strut = 0, N_cap_strut = 0;
        double M_frame = 0, M_cap_frame = 0;
        double N_ring  = 0, N_cap_ring  = 0;
        if (P.scheme == "strut") {
            N_strut = std::fabs(Na) * P.strut_s;
            N_cap_strut = P.strut_Rc * P.strut_A;
            sup_ok = (N_strut <= N_cap_strut + 1e-6);
        } else if (P.scheme == "frame") {
            M_frame = std::fabs(Na) * P.frame_B * P.frame_B / 8.0 * P.frame_kM;
            M_cap_frame = P.frame_Ry * P.frame_W;
            sup_ok = (M_frame <= M_cap_frame + 1e-6);
        } else if (P.scheme == "ring") {
            N_ring = std::fabs(Na) * P.ring_R;
            N_cap_ring = P.Ry * P.ring_A;
            sup_ok = (N_ring <= N_cap_ring + 1e-6);
        }

        QString line = QString("  h = %1 м:  равн.%2  расп.%3  прочн.%4  грунт%5  прогиб%6  h/H%7  подп.%8")
                           .arg(h, 6, 'f', 4)
                           .arg(eq_ok      ? "[+]" : "[-]")
                           .arg(na_ok      ? "[+]" : "[-]")
                           .arg(str_ok     ? "[+]" : "[-]")
                           .arg(soil_ok    ? "[+]" : "[-]")
                           .arg(defl_ok    ? "[+]" : "[-]")
                           .arg(h_ratio_ok ? "[+]" : "[-]")
                           .arg(sup_ok     ? "[+]" : "[-]");

        if (eq_ok && na_ok && str_ok && soil_ok && defl_ok && h_ratio_ok && sup_ok) {
            line += "   <<< ОК";
            R.log << line;
            found = true; h_found = h;
            R.Ea = Ea; R.za = za; R.Ep = Ep; R.zp = zp; R.Na = Na;
            R.Mmax = Mmax; R.zM = zM; R.sigma = sigma; R.y_max = y_max;
            R.ratio = sc.maxRatio;
            R.N_strut = N_strut; R.N_cap_strut = N_cap_strut;
            R.M_frame = M_frame; R.M_cap_frame = M_cap_frame;
            R.N_ring = N_ring;   R.N_cap_ring = N_cap_ring;
            break;
        }
        line += "   (";
        if (!eq_ok)      line += "момент, ";
        if (!na_ok)      line += "распорка в растяжении, ";
        if (!str_ok)     line += QString("σ>%1, ").arg(P.Ry * P.gamma_c, 0, 'f', 0);
        if (!soil_ok)    line += "грунт, ";
        if (!defl_ok)    line += QString("прогиб %1>%2 мм, ")
                        .arg(y_max*1000, 0, 'f', 1).arg(P.y_lim*1000, 0, 'f', 1);
        if (!h_ratio_ok) line += QString("h/H=%1<%2, ")
                        .arg(h_ratio, 0, 'f', 3).arg(g_h_ratio_min, 0, 'f', 2);
        if (!sup_ok)     line += "подпор, ";
        line += ")";
        R.log << line;
    }

    if (!found) {
        R.found = false;
        R.error = QString("Решение не найдено в диапазоне до %1 м.\n"
                          "Увеличьте h_max, измените профиль шпунта или схему.")
                      .arg(P.h_max);
        return R;
    }
    R.found = true; R.h_found = h_found;

    QStringList s;
    s << "============================================================";
    s << "  РАСЧЁТ ШПУНТОВОЙ СТЕНКИ (предельное равновесие)";
    s << "============================================================";
    s << QString("Схема: %1").arg(P.scheme);
    s << QString("H = %1 м; γ = %2 кН/м³; φ = %3°; c = %4 кПа; q = %5 кПа")
             .arg(fn(P.h, 3)).arg(fn(P.gamma, 2)).arg(fn(P.phi, 1))
             .arg(fn(P.c, 2)).arg(fn(P.q, 2));
    s << QString("Ka = %1; Kp = %2; m = %3; γc = %4; h/H_min = %5")
             .arg(fn(R.Ka)).arg(fn(R.Kp)).arg(fn(P.m, 3))
             .arg(fn(P.gamma_c, 3)).arg(fn(g_h_ratio_min, 2));
    s << QString("Профиль: I = %1 м⁴/м; W = %2 м³/м; Ry = %3 кПа")
             .arg(fg(P.I)).arg(fg(P.W)).arg(fn(P.Ry, 0));
    s << "------------------------------------------------------------";
    s << "         РЕЗУЛЬТАТ";
    s << "------------------------------------------------------------";
    s << QString("Минимальная глубина заделки: h = %1 м  (h/H = %2)")
             .arg(fn(R.h_found)).arg(fn(R.h_found/P.h, 3));
    s << QString("Полная длина: L = H + h = %1 м").arg(fn(P.h + R.h_found));
    s << QString("Ea = %1 кН/м  (центр za = %2 м)").arg(fn(R.Ea)).arg(fn(R.za));
    s << QString("Ep = %1 кН/м  (центр zp = %2 м от дна)").arg(fn(R.Ep)).arg(fn(R.zp));
    if (P.scheme != "console")
        s << QString("Na = %1 кН/м  (сжатие в распорке)").arg(fn(R.Na));
    s << QString("Mmax = %1 кН·м при z = %2 м").arg(fn(std::fabs(R.Mmax))).arg(fn(R.zM));
    s << QString("σ = %1 кПа при Ry·γc = %2 кПа").arg(fn(R.sigma, 2)).arg(fn(P.Ry * P.gamma_c, 2));
    s << QString("y_max ≈ %1 мм при [y] = %2 мм")
             .arg(fn(R.y_max * 1000, 2)).arg(fn(P.y_lim * 1000, 2));
    s << QString("Грунт: max(p/p_пред) = %1").arg(fn(R.ratio));
    if (P.scheme == "strut")
        s << QString("Распорка: N = %1 кН; Rc·A = %2 кН").arg(fn(R.N_strut, 2)).arg(fn(R.N_cap_strut, 2));
    else if (P.scheme == "frame")
        s << QString("Обвязка: M = %1 кН·м (kM=%2); Ry·W = %3 кН·м")
                 .arg(fn(R.M_frame, 2)).arg(fn(P.frame_kM, 2)).arg(fn(R.M_cap_frame, 2));
    else if (P.scheme == "ring")
        s << QString("Кольцо: N = %1 кН; Ry·A = %2 кН").arg(fn(R.N_ring, 2)).arg(fn(R.N_cap_ring, 2));
    s << "============================================================";
    R.summary = s.join("\n");
    return R;
}

Params CalcEngine::defaults() { return Params(); }

static QMap<QString,QString> readKV(const QString& fname) {
    QMap<QString,QString> kv;
    QFile f(fname);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return kv;
    QTextStream ts(&f);
    while (!ts.atEnd()) {
        QString line = ts.readLine();
        int c = line.indexOf('#'); if (c >= 0) line = line.left(c);
        int eq = line.indexOf('='); if (eq < 0) continue;
        QString k = line.left(eq).trimmed();
        QString v = line.mid(eq + 1).trimmed();
        if (!k.isEmpty()) kv[k] = v;
    }
    return kv;
}
static QString gs(const QMap<QString,QString>& kv, const QString& k, const QString& d) {
    return kv.contains(k) ? kv[k] : d;
}
static double gn(const QMap<QString,QString>& kv, const QString& k, double d) {
    if (!kv.contains(k)) return d;
    bool ok; double v = kv[k].toDouble(&ok); return ok ? v : d;
}

bool CalcEngine::loadFromFile(const QString& filename, Params& P, QString& err) {
    QFile f(filename);
    if (!f.exists()) { err = "Файл не найден: " + filename; return false; }
    auto kv = readKV(filename);
    if (kv.isEmpty()) { err = "Файл пуст: " + filename; return false; }
    P = Params();
    P.scheme    = gs(kv, "scheme", "console").trimmed().toLower();
    P.h         = gn(kv, "h", 3.0);
    P.gamma     = gn(kv, "gamma", 18.0);
    P.phi       = gn(kv, "phi", 30.0);
    P.q         = gn(kv, "q", 0.0);
    P.c         = gn(kv, "c", 0.0);
    P.m         = gn(kv, "m", 0.85);   // FIX #4: реалистичный default
    P.gamma_c   = gn(kv, "gamma_c", 1.0);
    P.E         = gn(kv, "E", 2.06e8);
    P.I         = gn(kv, "I", 3.0e-4);
    P.W         = gn(kv, "W", 1.67e-3);
    P.Ry        = gn(kv, "Ry", 240000.0);
    P.y_lim     = gn(kv, "y_lim", 0.03);
    P.h_start   = gn(kv, "h_start", 0.10);
    P.h_step    = gn(kv, "h_step", 0.10);
    P.h_max     = gn(kv, "h_max", 12.0);
    P.support_z = gn(kv, "support_z", 0.0);
    P.strut_s   = gn(kv, "strut_s", 4.0);
    P.strut_A   = gn(kv, "strut_A", 56e-4);
    P.strut_Rc  = gn(kv, "strut_Rc", 200000.0);
    P.ring_R    = gn(kv, "ring_R", 8.0);
    P.ring_A    = gn(kv, "ring_A", 61.2e-4);
    P.frame_B   = gn(kv, "frame_B", 6.0);
    P.frame_W   = gn(kv, "frame_W", 242e-6);
    P.frame_Ry  = gn(kv, "frame_Ry", 240000.0);
    P.frame_kM  = gn(kv, "frame_kM", 0.6);
    P.N_gamma   = gn(kv, "N_gamma", 15.0);
    P.N_c       = gn(kv, "N_c", 30.0);
    P.N_q       = gn(kv, "N_q", 18.0);
    g_h_ratio_min = gn(kv, "h_ratio_min", 0.45);  // NEW
    return true;
}

bool CalcEngine::saveToFile(const QString& filename, const Params& P, QString& err) {
    QFile f(filename);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        err = "Не удалось открыть файл: " + filename; return false;
    }
    QTextStream ts(&f);
    auto n = [](double v) { return QString::number(v, 'g', 12); };
    ts << "# ============ СХЕМА ============\n";
    ts << "scheme = " << P.scheme << "\n\n";
    ts << "# ============ ГЕОМЕТРИЯ И ГРУНТ ============\n";
    ts << "h        = " << n(P.h) << "\n";
    ts << "gamma    = " << n(P.gamma) << "\n";
    ts << "phi      = " << n(P.phi) << "\n";
    ts << "q        = " << n(P.q) << "\n";
    ts << "c        = " << n(P.c) << "\n";
    ts << "m        = " << n(P.m) << "\n";
    ts << "gamma_c  = " << n(P.gamma_c) << "\n\n";
    ts << "# ============ ШПУНТ ============\n";
    ts << "E        = " << n(P.E) << "\n";
    ts << "I        = " << n(P.I) << "\n";
    ts << "W        = " << n(P.W) << "\n";
    ts << "Ry       = " << n(P.Ry) << "\n\n";
    ts << "# ============ ПРОГИБ ============\n";
    ts << "y_lim    = " << n(P.y_lim) << "\n\n";
    ts << "# ============ ИТЕРАЦИЯ ============\n";
    ts << "h_start  = " << n(P.h_start) << "\n";
    ts << "h_step   = " << n(P.h_step) << "\n";
    ts << "h_max    = " << n(P.h_max) << "\n";
    ts << "h_ratio_min = " << n(g_h_ratio_min) << "\n\n";
    ts << "# ============ РАСПОРКА ============\n";
    ts << "support_z  = " << n(P.support_z) << "\n";
    ts << "strut_s    = " << n(P.strut_s) << "\n";
    ts << "strut_A    = " << n(P.strut_A) << "\n";
    ts << "strut_Rc   = " << n(P.strut_Rc) << "\n\n";
    ts << "# ============ КОЛЬЦО ============\n";
    ts << "ring_R   = " << n(P.ring_R) << "\n";
    ts << "ring_A   = " << n(P.ring_A) << "\n\n";
    ts << "# ============ РАМА ============\n";
    ts << "frame_B  = " << n(P.frame_B) << "\n";
    ts << "frame_W  = " << n(P.frame_W) << "\n";
    ts << "frame_Ry = " << n(P.frame_Ry) << "\n";
    ts << "frame_kM = " << n(P.frame_kM) << "\n\n";
    ts << "# ============ НЕСУЩАЯ СПОСОБНОСТЬ ============\n";
    ts << "N_gamma = " << n(P.N_gamma) << "\n";
    ts << "N_c     = " << n(P.N_c) << "\n";
    ts << "N_q     = " << n(P.N_q) << "\n";
    return true;
}