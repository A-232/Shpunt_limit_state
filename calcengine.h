#ifndef CALCENGINE_H
#define CALCENGINE_H

#include <QString>
#include <QStringList>

struct Params {
    QString scheme = "console";
    double h = 3.0;
    double support_z = 0.0;
    double gamma = 18.0, phi = 30.0, q = 0.0, c = 0.0;
    double m = 0.95, gamma_c = 1.0;
    double E = 2.06e8, I = 3.0e-4, W = 1.67e-3, Ry = 240000.0, y_lim = 0.03;
    double h_start = 0.10, h_step = 0.10, h_max = 12.0;
    double strut_s = 4.0, strut_A = 56e-4, strut_Rc = 200000.0;
    double ring_R = 8.0, ring_A = 61.2e-4;
    double frame_B = 6.0, frame_W = 242e-6, frame_Ry = 240000.0;
    double frame_kM = 0.6; // FIX: Коэффициент распределения момента в обвязке (0.5 - 0.7 для рамных узлов)
    double N_gamma = 15.0, N_c = 30.0, N_q = 18.0;
};

struct CalcResult {
    bool found = false;
    double h_found = 0;
    double Ka = 0, Kp = 0;
    double Ea = 0, za = 0, Ep = 0, zp = 0, Na = 0;
    double Mmax = 0, zM = 0, sigma = 0, y_max = 0, ratio = 0;
    double N_strut = 0, N_cap_strut = 0;
    double M_frame = 0, M_cap_frame = 0;
    double N_ring = 0,  N_cap_ring = 0;
    QStringList log;
    QString summary;
    QString error;
};

class CalcEngine {
public:
    static CalcResult calculate(const Params& P);
    static Params defaults();
    static bool loadFromFile(const QString& filename, Params& P, QString& err);
    static bool saveToFile(const QString& filename, const Params& P, QString& err);
};

#endif
