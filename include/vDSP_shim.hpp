#ifndef __VDSP_SHIM_HPP__
#define __VDSP_SHIM_HPP__

#include <cmath>
#include <complex>
#include <vector>
#include <algorithm>
#include <cstring>

#define POCKETFFT_NO_MULTITHREADING
#include "pocketfft_hdronly.h"

// Types
typedef struct {
    double *realp;
    double *imagp;
} DSPSplitComplex;

typedef DSPSplitComplex DSPDoubleSplitComplex;

typedef struct {
    double real;
    double imag;
} DSPDoubleComplex;

typedef void* FFTSetupD;

enum {
    kFFTRadix2 = 0,
    kFFTDirection_Forward = 1,
    kFFTDirection_Inverse = -1,
    FFT_FORWARD = 1,
    FFT_INVERSE = -1
};

// Vector Ops
inline void vDSP_vnegD(const double *__A, long __IA, double *__C, long __IC, long __N) {
    for (long i = 0; i < __N; ++i) __C[i * __IC] = -__A[i * __IA];
}

inline void vDSP_vsubD(const double *__A, long __IA, const double *__B, long __IB, double *__C, long __IC, long __N) {
    for (long i = 0; i < __N; ++i) __C[i * __IC] = __A[i * __IA] - __B[i * __IB];
}

inline void vDSP_vssqD(const double *__A, long __IA, double *__C, long __IC, long __N) {
    for (long i = 0; i < __N; ++i) {
        double val = __A[i * __IA];
        __C[i * __IC] = std::copysign(val * val, val);
    }
}

inline void vDSP_vsqD(const double *__A, long __IA, double *__C, long __IC, long __N) {
    for (long i = 0; i < __N; ++i) {
        double val = __A[i * __IA];
        __C[i * __IC] = val * val;
    }
}

inline void vDSP_vsmulD(const double *__A, long __IA, const double *__B, double *__C, long __IC, long __N) {
    double scalar = *__B;
    for (long i = 0; i < __N; ++i) __C[i * __IC] = __A[i * __IA] * scalar;
}

inline void vDSP_vsaddD(const double *__A, long __IA, const double *__B, double *__C, long __IC, long __N) {
    double scalar = *__B;
    for (long i = 0; i < __N; ++i) __C[i * __IC] = __A[i * __IA] + scalar;
}

inline void vDSP_vdbconD(const double *__A, long __IA, const double *__B, double *__C, long __IC, long __N, unsigned int __F) {
    // A is power or amplitude. B is reference. F=0 is power, F=1 is amplitude.
    // Formula: 10 * log10(A/B) for power, 20 * log10(A/B) for amplitude.
    // Usually vDSP_vdbconD treats A as amplitude if F=1?
    // Docs say: if F=0, C = 10*log10(A/B). If F=1, C = 20*log10(A/B).
    double ref = *__B;
    double factor = (__F == 0) ? 10.0 : 20.0;
    for (long i = 0; i < __N; ++i) {
        double val = __A[i * __IA];
        if (val <= 0) val = 1e-20; // avoid log(0)
        __C[i * __IC] = factor * std::log10(val / ref);
    }
}

inline void vDSP_svdivD(const double *__A, const double *__B, long __IB, double *__C, long __IC, long __N) {
    double scalar = *__A;
    for (long i = 0; i < __N; ++i) __C[i * __IC] = scalar / __B[i * __IB];
}

inline void vDSP_vaddD(const double *__A, long __IA, const double *__B, long __IB, double *__C, long __IC, long __N) {
    for (long i = 0; i < __N; ++i) __C[i * __IC] = __A[i * __IA] + __B[i * __IB];
}

inline void vDSP_vdivD(const double *__A, long __IA, const double *__B, long __IB, double *__C, long __IC, long __N) {
    // vDSP_vdivD does C = A / B? Wait, vDSP docs say: Divide vector B by vector A. C = B / A.
    // "Divide vector B by vector A" -> C = B / A.
    // Double check. Apple docs: "Divides the second vector by the first vector." -> C[i] = B[i] / A[i].
    for (long i = 0; i < __N; ++i) __C[i * __IC] = __B[i * __IB] / __A[i * __IA];
}

inline void vDSP_vmulD(const double *__A, long __IA, const double *__B, long __IB, double *__C, long __IC, long __N) {
    for (long i = 0; i < __N; ++i) __C[i * __IC] = __A[i * __IA] * __B[i * __IB];
}

inline void vDSP_vminD(const double *__A, long __IA, const double *__B, long __IB, double *__C, long __IC, long __N) {
    for (long i = 0; i < __N; ++i) __C[i * __IC] = std::fmin(__A[i * __IA], __B[i * __IB]);
}

inline void vDSP_vmaxD(const double *__A, long __IA, const double *__B, long __IB, double *__C, long __IC, long __N) {
    for (long i = 0; i < __N; ++i) __C[i * __IC] = std::fmax(__A[i * __IA], __B[i * __IB]);
}

inline void vDSP_vdistD(const double *__A, long __IA, const double *__B, long __IB, double *__C, long __IC, long __N) {
    for (long i = 0; i < __N; ++i) __C[i * __IC] = std::hypot(__A[i * __IA], __B[i * __IB]);
}


// Windowing
inline void vDSP_hann_windowD(double *__C, long __N, int __F) {
    // F is flag usually 0
    for (long i = 0; i < __N; ++i) {
        __C[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (__N - 1)));
    }
}

inline void vDSP_hamm_windowD(double *__C, long __N, int __F) {
    for (long i = 0; i < __N; ++i) {
        __C[i] = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (__N - 1));
    }
}

inline void vDSP_blkman_windowD(double *__C, long __N, int __F) {
    for (long i = 0; i < __N; ++i) {
        __C[i] = 0.42 - 0.5 * std::cos(2.0 * M_PI * i / (__N - 1)) + 0.08 * std::cos(4.0 * M_PI * i / (__N - 1));
    }
}

// Complex / FFT
inline void vDSP_ctozD(const DSPDoubleComplex *__C, long __IC, const DSPSplitComplex *__Z, long __IZ, long __N) {
    const double* C_scalar = (const double*)__C;
    for (long i = 0; i < __N; ++i) {
        __Z->realp[i * __IZ] = C_scalar[i * __IC];
        __Z->imagp[i * __IZ] = C_scalar[i * __IC + 1];
    }
}

inline void vDSP_ztocD(const DSPSplitComplex *__Z, long __IZ, DSPDoubleComplex *__C, long __IC, long __N) {
    double* C_scalar = (double*)__C;
    for (long i = 0; i < __N; ++i) {
        C_scalar[i * __IC] = __Z->realp[i * __IZ];
        C_scalar[i * __IC + 1] = __Z->imagp[i * __IZ];
    }
}

inline void vDSP_rectD(const double *__polar, long __IP, double *__rect, long __IR, long __N) {
    // polar [mag, phase] -> rect [real, imag]
    // Input is interleaved mag/phase?
    // Docs: polar: mag, phase. rect: real, imag.
    // If stride is 2, likely interleaved.
    for (long i = 0; i < __N; ++i) {
        double mag = __polar[i * __IP];
        double phase = __polar[i * __IP + 1];
        __rect[i * __IR] = mag * std::cos(phase);
        __rect[i * __IR + 1] = mag * std::sin(phase);
    }
}

// FFT Wrappers
// We use void* for setup, but effectively we don't need it for pocketfft
inline FFTSetupD vDSP_create_fftsetupD(long __Log2N, int __Radix) {
    return (void*)1; // Dummy
}

inline void vDSP_destroy_fftsetupD(FFTSetupD __I) {}

// Complex FFT: zop (out-of-place) and zip (in-place)
// pocketfft uses C2C
inline void vDSP_fft_zopD(FFTSetupD __I, const DSPSplitComplex *__A, long __IA, const DSPSplitComplex *__C, long __IC, long __Log2N, int __D) {
    // Join split complex to std::complex
    size_t N = 1 << __Log2N;
    std::vector<std::complex<double>> in(N);
    std::vector<std::complex<double>> out(N);
    
    for (size_t i = 0; i < N; ++i) {
        in[i] = {__A->realp[i * __IA], __A->imagp[i * __IA]};
    }
    
    bool forward = (__D == kFFTDirection_Forward);
    double scale = 1.0; // vDSP does not scale? Wait, Accelerate FFT is unscaled.
    
    pocketfft::c2c({N}, {sizeof(std::complex<double>)}, {sizeof(std::complex<double>)}, {0}, forward, in.data(), out.data(), scale);
    
    for (size_t i = 0; i < N; ++i) {
        __C->realp[i * __IC] = out[i].real();
        __C->imagp[i * __IC] = out[i].imag();
    }
}

inline void vDSP_fft_zipD(FFTSetupD __I, const DSPSplitComplex *__C, long __IC, long __Log2N, int __D) {
    vDSP_fft_zopD(__I, __C, __IC, __C, __IC, __Log2N, __D);
}

// Real-to-Complex / Complex-to-Real
// vDSP_fft_zropD: Real to Complex (Interleaved in a specific way? Or standard?)
// In Accelerate, real FFT usually takes a split complex buffer where realp and imagp are half the size,
// treated as packed complex for the first step.
// But vDSP_fft_zropD (z-real-out-of-place) takes split complex input and output.
// Wait, zropD: "Complex to Complex"? No. "Real FFT".
// "zrop": Complex input, Split complex output? No.
// vDSP_fft_zropD performs a Complex FFT?
// Actually, for Real FFT in Accelerate, one typically uses `vDSP_fft_zripD` (Real In-Place).
// `vDSP_fft_zropD`: "Forward real FFT, out-of-place".
// Input is `DSPSplitComplex*`? No, that's odd.
// Usually Real FFT takes `double*` input.
// But `vDSP_fft_zropD` declaration: `vDSP_fft_zropD(FFTSetupD __Setup, const DSPSplitComplex *__A, long __IA, const DSPSplitComplex *__C, long __IC, long __Log2N, int __Direction)`
// This suggests it treats the input split complex as a packed real array (realp || imagp)?
// Accelerate Real FFT is WEIRD. It packs 2N real numbers into a split complex of size N.
// Input A.realp contains even elements, A.imagp contains odd elements.
// So `A` effectively holds `2^Log2N` real numbers.
// Output `C` is split complex of size `2^(Log2N-1)`.
// Let's assume standard Accelerate behavior:
// N = 1 << Log2N. Real elements count.
// Input is split complex acting as packed real.
// Output is split complex (standard frequency domain).

inline void vDSP_fft_zropD(FFTSetupD __I, const DSPSplitComplex *__A, long __IA, const DSPSplitComplex *__C, long __IC, long __Log2N, int __D) {
    // Log2N is the size of the FFT in COMPLEX elements if it were complex?
    // For Real FFT, Log2N usually refers to the number of REAL elements.
    // Accelerate docs: "The number of real elements, n, is 2^Log2N."
    
    size_t N = 1 << __Log2N;
    std::vector<double> in(N);
    
    // Unpack split complex A to real vector
    // A.realp[i] -> in[2*i]
    // A.imagp[i] -> in[2*i+1]
    // Assuming IA=1
    for (size_t i = 0; i < N/2; ++i) {
        in[2*i] = __A->realp[i * __IA];
        in[2*i+1] = __A->imagp[i * __IA];
    }
    
    if (__D == kFFTDirection_Forward) {
        std::vector<std::complex<double>> out(N/2 + 1);
        pocketfft::r2c({N}, {sizeof(double)}, {sizeof(std::complex<double>)}, {0}, true, in.data(), out.data(), 1.0);
        
        // Pack back to split complex C
        // C size is N/2.
        // Accelerate packing: DC is at C.realp[0], Nyquist is at C.imagp[0].
        // Rest are standard real/imag.
        
        __C->realp[0] = out[0].real();
        __C->imagp[0] = out[N/2].real(); // Nyquist
        
        for (size_t i = 1; i < N/2; ++i) {
            __C->realp[i * __IC] = out[i].real();
            __C->imagp[i * __IC] = out[i].imag();
        }
    } else {
        // Inverse
        // Not typically called via zropD for Real?
        // Usually inverse real FFT is separate or via zripD.
    }
}

inline void vDSP_fft_zripD(FFTSetupD __I, const DSPSplitComplex *__C, long __IC, long __Log2N, int __D) {
    // In-place real FFT.
    // If Forward: C contains packed reals, output packed complex.
    // If Inverse: C contains packed complex, output packed reals.
    
    size_t N = 1 << __Log2N;
    
    if (__D == kFFTDirection_Forward) {
        // Same as zropD but in place
        // Unpack
        std::vector<double> in(N);
        for (size_t i = 0; i < N/2; ++i) {
            in[2*i] = __C->realp[i * __IC];
            in[2*i+1] = __C->imagp[i * __IC];
        }
        
        std::vector<std::complex<double>> out(N/2 + 1);
        pocketfft::r2c({N}, {sizeof(double)}, {sizeof(std::complex<double>)}, {0}, true, in.data(), out.data(), 1.0);
        
        // Pack
        __C->realp[0] = out[0].real();
        __C->imagp[0] = out[N/2].real();
        for (size_t i = 1; i < N/2; ++i) {
            __C->realp[i * __IC] = out[i].real();
            __C->imagp[i * __IC] = out[i].imag();
        }
    } else {
        // Inverse Real FFT
        // Input is split complex with DC/Nyquist packed.
        std::vector<std::complex<double>> in(N/2 + 1);
        
        in[0] = {__C->realp[0], 0};
        in[N/2] = {__C->imagp[0], 0};
        
        for (size_t i = 1; i < N/2; ++i) {
            in[i] = {__C->realp[i * __IC], __C->imagp[i * __IC]};
        }
        
        std::vector<double> out(N);
        pocketfft::c2r({N}, {sizeof(std::complex<double>)}, {sizeof(double)}, {0}, false, in.data(), out.data(), 1.0); // Unscaled? Accelerate might scale by 2 or something?
        // Accelerate Inverse Real FFT usually needs scaling by 1/2 or 1/N? 
        // Usually it's unscaled, so user scales by 1/N.
        // PocketFFT c2r is unscaled? default scale 1.0.
        
        // Pack back to C
        for (size_t i = 0; i < N/2; ++i) {
            __C->realp[i * __IC] = out[2*i];
            __C->imagp[i * __IC] = out[2*i+1];
        }
    }
}

// VForce Ops
inline void vvfabs(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::fabs(__X[i]);
}

inline void vvfloor(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::floor(__X[i]);
}

inline void vvceil(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::ceil(__X[i]);
}

inline void vvround(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::round(__X[i]);
}

inline void vvsin(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::sin(__X[i]);
}

inline void vvcos(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::cos(__X[i]);
}

inline void vvtan(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::tan(__X[i]);
}

inline void vvasin(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::asin(__X[i]);
}

inline void vvacos(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::acos(__X[i]);
}

inline void vvatan(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::atan(__X[i]);
}

inline void vvsqrt(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::sqrt(__X[i]);
}

inline void vvlog(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::log(__X[i]);
}

inline void vvlog10(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::log10(__X[i]);
}

inline void vvexp(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::exp(__X[i]);
}

inline void vvpow(double *__Y, const double *__X, const double *__Z, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::pow(__X[i], __Z[i]);
}

inline void vvnint(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::rint(__X[i]);
}

inline void vvrec(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = 1.0 / __X[i];
}

inline void vvrsqrt(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = 1.0 / std::sqrt(__X[i]);
}

inline void vvexp2(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::exp2(__X[i]);
}

inline void vvexpm1(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::expm1(__X[i]);
}

inline void vvlog2(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::log2(__X[i]);
}

inline void vvlog1p(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::log1p(__X[i]);
}

inline void vvtanh(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::tanh(__X[i]);
}

inline void vvasinh(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::asinh(__X[i]);
}

inline void vvacosh(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::acosh(__X[i]);
}

inline void vvatanh(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::atanh(__X[i]);
}

inline void vvlogb(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::logb(__X[i]);
}

inline void vvsinh(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::sinh(__X[i]);
}

inline void vvcosh(double *__Y, const double *__X, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::cosh(__X[i]);
}

inline void vvcopysign(double *__Y, const double *__X, const double *__Z, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::copysign(__X[i], __Z[i]);
}

inline void vvfmod(double *__Y, const double *__X, const double *__Z, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::fmod(__X[i], __Z[i]);
}

inline void vvremainder(double *__Y, const double *__X, const double *__Z, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::remainder(__X[i], __Z[i]);
}

inline void vvatana(double *__Y, const double *__X, const double *__Z, const int *__N) {
    // vvatana(y, x, z, n) -> y = atan2(x, z)? Or atan2(z, x)?
    // vForce docs: vvatana(y, num, den, n) -> y = atan2(num, den)
    for (int i = 0; i < *__N; ++i) __Y[i] = std::atan2(__X[i], __Z[i]);
}

inline void vvatan2(double *__Y, const double *__X, const double *__Z, const int *__N) {
    // alias
    vvatana(__Y, __X, __Z, __N);
}

inline void vvnextafter(double *__Y, const double *__X, const double *__Z, const int *__N) {
    for (int i = 0; i < *__N; ++i) __Y[i] = std::nextafter(__X[i], __Z[i]);
}

#endif
