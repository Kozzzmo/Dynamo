// Dynamo - moteur DSP (en-tete seul, sans dependance systeme).
// Utilise par le moteur audio (APO) et par l'interface (courbes).
#pragma once
#include "shared.h"
#include <math.h>
#include <string.h>

namespace dyn {

constexpr int    MAXCH = 16;
constexpr double PI = 3.14159265358979323846;

inline double dbToLin(double db) { return pow(10.0, db / 20.0); }
inline double linToDb(double l)  { return 20.0 * log10(l > 1e-12 ? l : 1e-12); }

// ---------------------------------------------------------------- biquads
struct Coef  { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
struct State { double z1 = 0, z2 = 0; };

inline double run(const Coef& c, State& s, double x) {
    double y = c.b0 * x + s.z1;
    s.z1 = c.b1 * x - c.a1 * y + s.z2;
    s.z2 = c.b2 * x - c.a2 * y;
    return y;
}

inline Coef norm(double b0, double b1, double b2, double a0, double a1, double a2) {
    Coef c; c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0; c.a1 = a1 / a0; c.a2 = a2 / a0; return c;
}

inline double safeFreq(double f, double fs) {
    double maxf = fs * 0.49;
    if (f > maxf) f = maxf;
    if (f < 5.0) f = 5.0;
    return f;
}

// Formules RBJ (Audio EQ Cookbook)
inline Coef peak(double f, double gainDb, double q, double fs) {
    f = safeFreq(f, fs);
    double A = pow(10.0, gainDb / 40.0), w = 2 * PI * f / fs, al = sin(w) / (2 * q), cw = cos(w);
    return norm(1 + al * A, -2 * cw, 1 - al * A, 1 + al / A, -2 * cw, 1 - al / A);
}
inline Coef lowShelf(double f, double gainDb, double q, double fs) {
    f = safeFreq(f, fs);
    double A = pow(10.0, gainDb / 40.0), w = 2 * PI * f / fs, cw = cos(w), al = sin(w) / (2 * q), sA = 2 * sqrt(A) * al;
    return norm(A * ((A + 1) - (A - 1) * cw + sA), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - sA),
                (A + 1) + (A - 1) * cw + sA, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - sA);
}
inline Coef highShelf(double f, double gainDb, double q, double fs) {
    f = safeFreq(f, fs);
    double A = pow(10.0, gainDb / 40.0), w = 2 * PI * f / fs, cw = cos(w), al = sin(w) / (2 * q), sA = 2 * sqrt(A) * al;
    return norm(A * ((A + 1) + (A - 1) * cw + sA), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - sA),
                (A + 1) - (A - 1) * cw + sA, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - sA);
}
inline Coef lowPass(double f, double q, double fs) {
    f = safeFreq(f, fs);
    double w = 2 * PI * f / fs, cw = cos(w), al = sin(w) / (2 * q);
    return norm((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + al, -2 * cw, 1 - al);
}
inline Coef highPass(double f, double q, double fs) {
    f = safeFreq(f, fs);
    double w = 2 * PI * f / fs, cw = cos(w), al = sin(w) / (2 * q);
    return norm((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + al, -2 * cw, 1 - al);
}
inline Coef notch(double f, double q, double fs) {
    f = safeFreq(f, fs);
    double w = 2 * PI * f / fs, cw = cos(w), al = sin(w) / (2 * q);
    return norm(1, -2 * cw, 1, 1 + al, -2 * cw, 1 - al);
}
inline Coef allPass(double f, double q, double fs) {
    f = safeFreq(f, fs);
    double w = 2 * PI * f / fs, cw = cos(w), al = sin(w) / (2 * q);
    return norm(1 - al, -2 * cw, 1 + al, 1 + al, -2 * cw, 1 - al);
}

// |H(e^jw)| en dB
inline double magDb(const Coef& c, double f, double fs) {
    double w = 2 * PI * f / fs;
    double c1 = cos(w), s1 = sin(w), c2 = cos(2 * w), s2 = sin(2 * w);
    double nr = c.b0 + c.b1 * c1 + c.b2 * c2, ni = -(c.b1 * s1 + c.b2 * s2);
    double dr = 1 + c.a1 * c1 + c.a2 * c2,    di = -(c.a1 * s1 + c.a2 * s2);
    double num = nr * nr + ni * ni, den = dr * dr + di * di;
    if (den < 1e-30) den = 1e-30;
    if (num < 1e-30) num = 1e-30;
    return 10.0 * log10(num / den);
}

// Q de Butterworth pour la section k d'un filtre d'ordre 2n
inline double butterQ(int n, int k) {
    int N = 2 * n;
    return 1.0 / (2.0 * cos(PI * (2 * k + 1) / (2.0 * N)));
}

// Conception d'une bande d'EQ : renvoie le nombre de biquads (0 = transparent)
inline int designBand(const DynEqBand& b, double fs, Coef out[4]) {
    if (!b.enabled) return 0;
    switch (b.type) {
    case EQ_PEAK:
        if (fabs(b.gain) < 0.01) return 0;
        out[0] = peak(b.freq, b.gain, b.q, fs); return 1;
    case EQ_LOWSHELF:
        if (fabs(b.gain) < 0.01) return 0;
        out[0] = lowShelf(b.freq, b.gain, b.q, fs); return 1;
    case EQ_HIGHSHELF:
        if (fabs(b.gain) < 0.01) return 0;
        out[0] = highShelf(b.freq, b.gain, b.q, fs); return 1;
    case EQ_NOTCH:
        out[0] = notch(b.freq, b.q, fs); return 1;
    case EQ_LOWCUT:
    case EQ_HIGHCUT: {
        int n = b.slope < 1 ? 1 : (b.slope > 4 ? 4 : b.slope);
        for (int k = 0; k < n; ++k) {
            double q = (n == 1) ? b.q : butterQ(n, k);
            out[k] = (b.type == EQ_LOWCUT) ? highPass(b.freq, q, fs) : lowPass(b.freq, q, fs);
        }
        return n;
    }
    }
    return 0;
}

inline double bandResponseDb(const DynEqBand& b, double f, double fs) {
    Coef c[4]; int n = designBand(b, fs, c);
    double r = 0; for (int i = 0; i < n; ++i) r += magDb(c[i], f, fs);
    return r;
}

inline double eqResponseDb(const DynParams& p, double f, double fs) {
    double r = 0;
    for (int i = 0; i < DYN_EQ_BANDS; ++i) r += bandResponseDb(p.eq[i], f, fs);
    return r;
}

// ---------------------------------------------------------------- compresseur
// Caracteristique statique a genou doux (Giannoulis, Massberg, Reiss 2012)
inline double gainComputer(double x, double T, double R, double W) {
    double d = x - T;
    if (2 * d < -W) return x;
    if (W > 0 && 2 * fabs(d) <= W) {
        double t = d + W / 2;
        return x + (1.0 / R - 1.0) * t * t / (2 * W);
    }
    return T + d / R;
}

inline double autoMakeupDb(const DynCompBand& c) {
    // compense la moitie de la reduction appliquee a un signal a 0 dBFS
    double gr0 = 0.0 - gainComputer(0.0, c.threshold, c.ratio, c.knee);
    return gr0 * 0.5;
}

inline double makeupDb(const DynCompBand& c) {
    return c.makeup + (c.autoMakeup ? autoMakeupDb(c) : 0.0);
}

inline double timeCoef(double ms, double fs) {
    if (ms <= 0.0) return 0.0;
    return exp(-1.0 / (ms * 0.001 * fs));
}

// ---------------------------------------------------------------- moteur
class Engine {
public:
    void prepare(double sampleRate, int channels) {
        fs_ = sampleRate > 1000 ? sampleRate : 48000;
        ch_ = channels < 1 ? 1 : channels;
        chp_ = ch_ > MAXCH ? MAXCH : ch_;
        applyParams(p_);
        reset();
    }

    void reset() {
        memset(eqS_, 0, sizeof(eqS_));
        memset(scS_, 0, sizeof(scS_));
        memset(x1lpS_, 0, sizeof(x1lpS_)); memset(x1hpS_, 0, sizeof(x1hpS_));
        memset(x2lpS_, 0, sizeof(x2lpS_)); memset(x2hpS_, 0, sizeof(x2hpS_));
        memset(apS_, 0, sizeof(apS_));
        for (int b = 0; b < DYN_COMP_BANDS; ++b) { env_[b] = 0; rms_[b] = 0; }
        limGain_ = 1.0;
        inGainSm_ = inGainT_; outGainSm_ = outGainT_;
        for (int b = 0; b < DYN_COMP_BANDS; ++b) makeupSm_[b] = makeupT_[b];
    }

    void applyParams(const DynParams& src) {
        p_ = src;
        DynSanitize(p_);
        for (int i = 0; i < DYN_EQ_BANDS; ++i) {
            int n = designBand(p_.eq[i], fs_, eqC_[i]);
            if (n != eqN_[i]) memset(eqS_[i], 0, sizeof(eqS_[i]));
            eqN_[i] = n;
        }
        scOn_ = p_.scHpf > 10.5f;
        if (scOn_) scC_ = highPass(p_.scHpf, 0.7071, fs_);
        // crossovers Linkwitz-Riley 24 dB/oct (2 Butterworth en serie)
        x1lp_ = lowPass(p_.xoverLow, 0.70710678, fs_);  x1hp_ = highPass(p_.xoverLow, 0.70710678, fs_);
        x2lp_ = lowPass(p_.xoverHigh, 0.70710678, fs_); x2hp_ = highPass(p_.xoverHigh, 0.70710678, fs_);
        ap_   = allPass(p_.xoverHigh, 0.70710678, fs_);
        for (int b = 0; b < DYN_COMP_BANDS; ++b) {
            att_[b] = timeCoef(p_.comp[b].attack, fs_);
            rel_[b] = timeCoef(p_.comp[b].release, fs_);
            makeupT_[b] = makeupDb(p_.comp[b]);
        }
        rmsC_ = timeCoef(10.0, fs_);
        limRel_ = timeCoef(p_.limRelease, fs_);
        ceil_ = dbToLin(p_.limCeiling);
        inGainT_ = p_.inputGain;
        outGainT_ = p_.outputGain;
        smC_ = timeCoef(8.0, fs_);
        bypassTarget_ = p_.bypass ? 1.0 : 0.0;
    }

    const DynParams& params() const { return p_; }

    // Mesures du dernier bloc
    struct BlockMeters {
        float inPeak[2], outPeak[2], gr[DYN_COMP_BANDS], grLim, level;
    };

    // in et out peuvent etre identiques (traitement en place)
    void process(const float* in, float* out, unsigned frames, BlockMeters& m, float* specRing, unsigned specMask, unsigned& specPos) {
        memset(&m, 0, sizeof(m));
        m.level = -120.f;
        const int ch = ch_, chp = chp_;
        const double bypassStep = 1.0 / (0.02 * fs_);

        // Chemin rapide : contourne completement
        if (bypass_ >= 1.0 && bypassTarget_ >= 1.0) {
            for (unsigned f = 0; f < frames; ++f) {
                const float* xi = in + (size_t)f * ch;
                float* yo = out + (size_t)f * ch;
                for (int c = 0; c < ch; ++c) yo[c] = xi[c];
                trackPeak(m.inPeak, xi, ch); trackPeak(m.outPeak, yo, ch);
                if (specRing) { specRing[specPos & specMask] = ch > 1 ? 0.5f * (yo[0] + yo[1]) : yo[0]; ++specPos; }
            }
            return;
        }
        if (bypass_ >= 1.0 && bypassTarget_ < 1.0) reset(); // sortie du contournement : etats propres

        double x[MAXCH], dry[MAXCH];
        const bool eqOn = p_.eqEnabled != 0;
        const bool compOn = p_.compEnabled != 0;
        const bool multi = p_.compMultiband != 0;
        const double mix = p_.mix;
        double maxLevel = -200.0;

        for (unsigned f = 0; f < frames; ++f) {
            const float* xi = in + (size_t)f * ch;
            float* yo = out + (size_t)f * ch;

            inGainSm_ = smC_ * inGainSm_ + (1 - smC_) * inGainT_;
            outGainSm_ = smC_ * outGainSm_ + (1 - smC_) * outGainT_;
            const double gi = dbToLinFast(inGainSm_), go = dbToLinFast(outGainSm_);

            for (int c = 0; c < chp; ++c) { dry[c] = xi[c]; x[c] = xi[c] * gi; }
            trackPeak(m.inPeak, xi, ch);

            if (eqOn && !p_.eqPost) runEq(x, chp);

            if (compOn) {
                if (!multi) compSingle(x, chp, m, maxLevel, mix);
                else        compMulti(x, chp, m, mix);
            }

            if (eqOn && p_.eqPost) runEq(x, chp);

            // gain de sortie + limiteur (attaque instantanee, sans latence)
            double pk = 0;
            for (int c = 0; c < chp; ++c) { x[c] *= go; double a = fabs(x[c]); if (a > pk) pk = a; }
            if (p_.limEnabled) {
                double req = pk > ceil_ ? ceil_ / pk : 1.0;
                if (req < limGain_) limGain_ = req;
                else limGain_ = limRel_ * limGain_ + (1 - limRel_) * req;
                double gr = -linToDb(limGain_);
                if (gr > m.grLim) m.grLim = (float)gr;
                for (int c = 0; c < chp; ++c) {
                    double v = x[c] * limGain_;
                    if (v > ceil_) v = ceil_; else if (v < -ceil_) v = -ceil_;
                    x[c] = v;
                }
            }

            // fondu enchaine du contournement
            if (bypass_ != bypassTarget_) {
                if (bypass_ < bypassTarget_) { bypass_ += bypassStep; if (bypass_ > bypassTarget_) bypass_ = bypassTarget_; }
                else { bypass_ -= bypassStep; if (bypass_ < bypassTarget_) bypass_ = bypassTarget_; }
            }
            const double wb = bypass_;
            for (int c = 0; c < chp; ++c) yo[c] = (float)(x[c] * (1 - wb) + dry[c] * wb);
            for (int c = chp; c < ch; ++c) yo[c] = xi[c];

            trackPeak(m.outPeak, yo, ch);
            if (specRing) { specRing[specPos & specMask] = ch > 1 ? 0.5f * (yo[0] + yo[1]) : yo[0]; ++specPos; }
        }
        m.level = (float)maxLevel;
    }

private:
    static inline double dbToLinFast(double db) { return exp(db * 0.11512925464970228); }

    static inline void trackPeak(float pk[2], const float* v, int ch) {
        float a = fabsf(v[0]); if (a > pk[0]) pk[0] = a;
        float b = ch > 1 ? fabsf(v[1]) : a; if (b > pk[1]) pk[1] = b;
    }

    inline void runEq(double* x, int chp) {
        for (int i = 0; i < DYN_EQ_BANDS; ++i) {
            const int n = eqN_[i];
            for (int k = 0; k < n; ++k) {
                const Coef& c = eqC_[i][k];
                for (int ch = 0; ch < chp; ++ch) x[ch] = run(c, eqS_[i][k][ch], x[ch]);
            }
        }
    }

    inline double detect(int b, double peakAbs, double sumSq, int chp) {
        double level;
        if (p_.detectorRms) {
            rms_[b] = rmsC_ * rms_[b] + (1 - rmsC_) * (sumSq / chp);
            level = sqrt(2.0 * rms_[b]);
        } else level = peakAbs;
        return linToDb(level);
    }

    // renvoie la reduction de gain lissee (dB)
    inline double gainReduction(int b, double levelDb) {
        const DynCompBand& c = p_.comp[b];
        double gr = levelDb - gainComputer(levelDb, c.threshold, c.ratio, c.knee);
        if (gr < 0) gr = 0;
        double& e = env_[b];
        if (gr > e) e = att_[b] * e + (1 - att_[b]) * gr;
        else        e = rel_[b] * e + (1 - rel_[b]) * gr;
        return e;
    }

    inline void compSingle(double* x, int chp, BlockMeters& m, double& maxLevel, double mix) {
        double pk = 0, ss = 0;
        for (int c = 0; c < chp; ++c) {
            double s = scOn_ ? run(scC_, scS_[c], x[c]) : x[c];
            double a = fabs(s); if (a > pk) pk = a; ss += s * s;
        }
        double lv = detect(0, pk, ss, chp);
        if (lv > maxLevel) maxLevel = lv;
        makeupSm_[0] = smC_ * makeupSm_[0] + (1 - smC_) * makeupT_[0];
        double gr = p_.comp[0].enabled ? gainReduction(0, lv) : 0.0;
        if (gr > m.gr[0]) m.gr[0] = (float)gr;
        double g = p_.comp[0].enabled ? dbToLinFast(makeupSm_[0] - gr) : 1.0;
        double k = (1 - mix) + mix * g;
        for (int c = 0; c < chp; ++c) x[c] *= k;
    }

    inline void compMulti(double* x, int chp, BlockMeters& m, double mix) {
        double bands[3][MAXCH];
        double pk[3] = {0, 0, 0}, ss[3] = {0, 0, 0};
        for (int c = 0; c < chp; ++c) {
            double lo = run(x1lp_, x1lpS_[c][1], run(x1lp_, x1lpS_[c][0], x[c]));
            double hiRest = run(x1hp_, x1hpS_[c][1], run(x1hp_, x1hpS_[c][0], x[c]));
            double mid = run(x2lp_, x2lpS_[c][1], run(x2lp_, x2lpS_[c][0], hiRest));
            double hi  = run(x2hp_, x2hpS_[c][1], run(x2hp_, x2hpS_[c][0], hiRest));
            lo = run(ap_, apS_[c], lo); // alignement de phase avec le 2e filtre
            bands[0][c] = lo; bands[1][c] = mid; bands[2][c] = hi;
            for (int b = 0; b < 3; ++b) { double a = fabs(bands[b][c]); if (a > pk[b]) pk[b] = a; ss[b] += bands[b][c] * bands[b][c]; }
        }
        double k[3];
        for (int b = 0; b < 3; ++b) {
            const int cb = b + 1;
            makeupSm_[cb] = smC_ * makeupSm_[cb] + (1 - smC_) * makeupT_[cb];
            double lv = detect(cb, pk[b], ss[b], chp);
            double gr = p_.comp[cb].enabled ? gainReduction(cb, lv) : 0.0;
            if (gr > m.gr[cb]) m.gr[cb] = (float)gr;
            double g = p_.comp[cb].enabled ? dbToLinFast(makeupSm_[cb] - gr) : 1.0;
            k[b] = (1 - mix) + mix * g;
        }
        for (int c = 0; c < chp; ++c) x[c] = bands[0][c] * k[0] + bands[1][c] * k[1] + bands[2][c] * k[2];
    }

    DynParams p_ = [] { DynParams d; DynDefaultParams(d); return d; }();
    double fs_ = 48000; int ch_ = 2, chp_ = 2;

    Coef  eqC_[DYN_EQ_BANDS][4];
    int   eqN_[DYN_EQ_BANDS] = {};
    State eqS_[DYN_EQ_BANDS][4][MAXCH];

    bool  scOn_ = true; Coef scC_; State scS_[MAXCH];
    Coef  x1lp_, x1hp_, x2lp_, x2hp_, ap_;
    State x1lpS_[MAXCH][2], x1hpS_[MAXCH][2], x2lpS_[MAXCH][2], x2hpS_[MAXCH][2], apS_[MAXCH];

    double env_[DYN_COMP_BANDS] = {}, rms_[DYN_COMP_BANDS] = {};
    double att_[DYN_COMP_BANDS] = {}, rel_[DYN_COMP_BANDS] = {};
    double makeupT_[DYN_COMP_BANDS] = {}, makeupSm_[DYN_COMP_BANDS] = {};
    double rmsC_ = 0;

    double limGain_ = 1, limRel_ = 0, ceil_ = 1;
    double inGainT_ = 0, outGainT_ = 0, inGainSm_ = 0, outGainSm_ = 0, smC_ = 0;
    double bypass_ = 0, bypassTarget_ = 0;
};

} // namespace dyn
