
#ifndef SimTK_LAPACK_INTERFACE_H_
#define SimTK_LAPACK_INTERFACE_H_

/* Portions copyright (c) 2007 Stanford University and Jack Middleton.
 * Contributors:
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including 
 * without limitation the rights to use, copy, modify, merge, publish, 
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included 
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**@file
 * These is a templatized, C++ callable interface to LAPACK and BLAS.
 * Each method must be explicitly specialized for the supported precisions.
 */


#include "SimTKcommon.h"
#include "SimTKlapack.h"

namespace SimTK {

class LapackInterface { 
   
public:

static int getLWork( float* work);
static int getLWork( double* work);
static int getLWork( std::complex<float>* work);
static int getLWork( std::complex<double>* work);

template <class T> static
void gesdd( char jobz, int m, int n, T* a, int lda, 
           typename CNT<T>::TReal* s, T* u, int ldu,
           T* vt, int ldvt, int& info);

template <class T> static
void geev (char jobvl, char jobvr, int n, T* a, int lda, 
    std::complex<typename CNT<T>::TReal>* values, 
    T* vl, int ldvl, std::complex<typename CNT<T>::TReal>* vr, 
    int ldvr, T* work, int lwork, int& info );

template <class T> static
void syevx( char jobz, char range, char uplo, int n, T* a, int lda, 
    typename CNT<T>::TReal vl, typename CNT<T>::TReal vu, int il, int iu, 
    typename CNT<T>::TReal abstol, int& nFound, typename CNT<T>::TReal* values, 
    T* vectors, int LDVectors, int* ifail, int& info );
                  

template <class T> static
void syev( char jobz,  char uplo, int n, T* a_eigenVectors, int lda,  
    typename CNT<T>::TReal* eigenValues, int& info );


/* solve system of linear equations using the LU factorization  computed by getrf */
template <class T> static 
void getrs( const bool transpose, const int ncol, const int nrhs, const T *lu, const int* pivots, T *b ); 

template <class T> static 
void getrf( const int m, const int n, T *a, const int lda, int* pivots, int& info );

template <class T> static 
void gttrf( const int m, const int n, T* dl, T* d, T* du, T* du2, int* pivots, int& info );

template <class T> static 
void gbtrf( const int m, const int n, const int kl, const int ku, T* lu, const int lda, int* pivots, int& info );

template <class T> static 
void potrf( const int m, const int n, const int kl, const int ku, T* lu, const int lda, int* pivots, int& info );

template <class T> static 
void sytrf( const char m, const int n, T* a,  const int lda, int* pivots, T* work, const int lwork, int& info );

template <class T> static
int ilaenv( const int& ispec,  const char* name,  const char* opts, const int& n1, const int& n2, const int& n3, const int& n4  );

template <class T> static
void getMachinePrecision( T& smallNumber, T& bigNumber );

template <class T> static
void getMachineUnderflow( T& underFlow );

template <class T> static
void tzrzf( const int& m, const int& n,  T* a, const int& lda, T* tau, T* work, const int& lwork, int& info );

template <class T> static
void geqp3( const int& m, const int& n,  T* a, const int& lda, int *pivots, T* tau, T* work, const int& lwork, int& info );

template <class T> static
void lascl( const char& type, const int& kl, const int& ku, const typename CNT<T>::TReal& cfrom, const typename CNT<T>::TReal& cto,  const int& m, const int& n, T* a, const int& lda, int& info );

template <class T> static
double lange( const char& norm, const int& m, const int& n, const T* a, const int& lda );

template <class T> static
void ormqr(const char& side, const char& trans, const int& m, const int& n, const int& k, T* a, const int& lda, T *tau, T *c__, const int& ldc, T* work, const int& lwork, int& info);

template <class T> static
void trsm(const char& side, const char& uplo, const char& transA, const char& diag, const int& m, const int& n, const T& alpha, const T* A, const int& lda, T* B, const int& ldb );
 
template <class T> static
// TODO void ormrz(const char& side, const char& trans, const int& m, const int& n, const int& k, const int& l, T* a, const int& lda, T* tau, T* c__, const int& ldc, T* work, const int& lwork, int& info);
void ormrz(const char& side, const char& trans, const int& m, const int& n, const int& k, int* l, T* a, const int& lda, T* tau, T* c__, const int& ldc, T* work, const int& lwork, int& info);
 
template <class T> static
void copy( const int& n, const T* x, const int& incx, T* y, const int& incy);

template <class T> static
void laic1(const int& job, const int& j, const T* x, const typename CNT<T>::TReal& sest, const T* w, const T& gamma, typename CNT<T>::TReal& sestpr, T& s, T& c__ );

}; // class LapackInterface

}   // namespace SimTK

#endif // SimTK_LAPACK_INTERFACE_H_
