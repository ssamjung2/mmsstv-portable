/*
 * Portable DSP Filters and Utilities
 * Based on MMSSTV by Makoto Mori (JE3HHT) and Nobuyuki Oba
 */
#ifndef SSTV_DSP_FILTERS_H
#define SSTV_DSP_FILTERS_H

#include <vector>

namespace sstv_dsp {

constexpr int kTapMax = 512;
constexpr int kIirMax = 16;

enum FilterType {
    kFfLPF = 0,
    kFfHPF,
    kFfBPF,
    kFfBEF,
    kFfUSER,
    kFfLMS
};

struct FirSpec {
    int n;
    int typ;
    double fs;
    double fcl;
    double fch;
    double att;
    double gain;
    double fc;
};

void MakeFilter(double *hp, int tap, int type, double fs, double fcl, double fch, double att, double gain);
void MakeFilter(double *hp, FirSpec *fp);
void MakeHilbert(double *h, int n, double fs, double fc1, double fc2);

void MakeIIR(double *a, double *b, double fc, double fs, int order, int bc, double rp);

double DoFIR(double *hp, double *zp, double d, int tap);

class CIIRTANK {
public:
    CIIRTANK();
    void SetFreq(double f, double smp, double bw);
    double Do(double d);

private:
    double z1;
    double z2;
    double a0;
    double b1;
    double b2;
};

class CIIR {
public:
    CIIR();
    void MakeIIR(double fc, double fs, int order, int bc, double rp);
    double Do(double d);
    void Clear(void);

private:
    std::vector<double> a_;
    std::vector<double> b_;
    std::vector<double> z_;
    int order_;
    int bc_;
    double rp_;
};

class CFIR2 {
public:
    CFIR2();
    void Create(int tap);
    void Create(int tap, int type, double fs, double fcl, double fch, double att, double gain);
    void Clear(void);
    double Do(double d);
    double Do(double d, double *hp);
    double Do(double *hp);
    void Do(double &d, double &j, double *hp);

    inline double GetHD(int n) const { return h_.empty() ? 0.0 : h_[n]; }
    inline double *GetHP(void) { return h_.empty() ? nullptr : h_.data(); }
    inline int GetTap(void) const { return tap_; }

private:
    std::vector<double> z_;
    std::vector<double> h_;
    double *zp_;
    int w_;
    int tap_;
    int tap_half_;
};

} // namespace sstv_dsp

#endif
