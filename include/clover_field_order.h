#ifndef _CLOVER_ORDER_H
#define _CLOVER_ORDER_H

/**
 * @file  clover_field_order.h
 * @brief Main header file for host and device accessors to CloverFields
 *
 */

#include <register_traits.h>
#include <clover_field.h>
#include <complex_quda.h>

namespace quda {

  namespace clover {

    /**
       The internal ordering for each clover matrix has chirality as the
       slowest running dimension, with the internal 36 degrees of
       freedom stored as follows (s=spin, c = color)

       i |  col  |  row  |
           s   c   s   c   z
       0   0   0   0   0   0
       1   0   1   0   1   0
       2   0   2   0   2   0
       3   1   0   1   0   0
       4   1   1   1   1   0
       5   1   2   1   2   0
                               ((c_row*2 + s_col)*3 + c_col)*2 + z
                               col  row
       6   0   1   0   0   0   1    0
       7   0   1   0   0   1   1    0
       8   0   2   0   0   0   2    0
       9   0   2   0   0   1   2    0
       10  1   0   0   0   0   3    0
       11  1   0   0   0   1   3    0
       12  1   1   0   0   0   4    0
       13  1   1   0   0   1   4    0
       14  1   2   0   0   0   5    0
       15  1   2   0   0   1   5    0
       16  0   2   0   1   0   2    1
       17  0   2   0   1   1   2    1
       18  1   0   0   1   0   3    1
       19  1   0   0   1   1   3    1
       20  1   1   0   1   0   4    1
       21  1   1   0   1   1   4    1
       22  1   2   0   1   0   5    1
       23  1   2   0   1   1   5    1
       24  1   0   0   2   0   3    2
       25  1   0   0   2   1   3    2
       26  1   1   0   2   0   4    2
       27  1   1   0   2   1   4    2
       28  1   2   0   2   0   5    2
       29  1   2   0   2   1   5    2
       30  1   1   1   0   0   4    3
       31  1   1   1   0   1   4    3
       32  1   2   1   0   0   5    3
       33  1   2   1   0   1   5    3
       34  1   2   1   1   0   5    4
       35  1   2   1   1   1   5    4

       first 6 sites are the pure real diagonal entries

       1/2 * N * (N-1) sites N = 6, so 15 complex sites on the strictly lower triangular

       storage order on the strictly lower triangular is column major

       // psuedo code in lieu of implementation
       int row = s_row*3 + c_row;
       int col = s_col*3 + c_col;
       if (row == col) {
         return complex(a[row])
       } else {
         int idx = row * (row+1) + col;
       }

    */

    template<typename Float, int nColor, int nSpin, QudaCloverFieldOrder order> struct Accessor {
      mutable complex<Float> dummy;
      __device__ __host__ inline complex<Float>& operator()(int parity, int x, int s_row, int s_col,
							    int c_row, int c_col) const {
#ifndef __CUDA_ARCH__
	errorQuda("Not implemented");
#endif
	return dummy;
      }
    };

    template<typename Float, int nColor, int nSpin> 
      struct Accessor<Float,nColor,nSpin,QUDA_PACKED_CLOVER_ORDER> { 
      complex <Float> *a[2];
      int volumeCB;
    Accessor(const CloverField &A, bool inverse=false) : volumeCB(A.VolumeCB()) { 
	// even
	a[0] = static_cast<complex<Float>*>(const_cast<void*>(A.V(inverse))); 
	// odd
	a[1] = static_cast<complex<Float>*>(static_cast<char*>(const_cast<void*>(A.V(inverse))) + A.Bytes()/2); 
      }
      __device__ __host__ inline complex<Float>& operator()(int d, int parity, int x, int s_row, int s_col, int c_row, int c_col) const {
	//assert(s_col / 2 == s_row / 2); // clover matrix is block Hermitian and only defined at (0,0) and (1,1) wrt chirality 
	const int chirality = s_col / 2;
#ifndef __CUDA_ARCH__
	errorQuda("Not implemented");
#endif
	return a[parity][0]; }
    };

    /**
       This is a template driven generic clover field accessor.  To
       deploy for a specifc field ordering, the two operator()
       accessors have to be specialized for that ordering.
     */
    template <typename Float, int nColor, int nSpin, QudaCloverFieldOrder order>
      struct FieldOrder {

      protected:
	/** An internal reference to the actual field we are accessing */
	CloverField &A;
	const int volumeCB;
	const Accessor<Float,nColor,nSpin,order> accessor;

      public:
	/** 
	 * Constructor for the FieldOrder class
	 * @param field The field that we are accessing
	 */
      FieldOrder(CloverField &A) : A(A), volumeCB(A.VolumeCB()), accessor(A)
	{ }
	
	CloverField& Field() { return A; }
	
	virtual ~FieldOrder() { ; } 
    
    	/**
	 * Read-only complex-member accessor function
	 * @param parity Parity index
	 * @param x 1-d site index
	 * @param s_row row spin index
	 * @param c_row row color index
	 * @param s_col col spin index
	 * @param c_col col color index
	 */
	__device__ __host__ inline const complex<Float>& operator()(int d, int parity, int x, int s_row, 
								    int s_col, int c_row, int c_col) const {
	  return accessor(parity, x, s_row, s_col, c_row, c_col);
	}
	
	/**
	 * Complex-member accessor function
	 * @param parity Parity index
	 * @param x 1-d site index
	 * @param s_row row spin index
	 * @param c_row row color index
	 * @param s_col col spin index
	 * @param c_col col color index
	 */
	__device__ __host__ inline complex<Float>& operator()(int d, int parity, int x, int s_row, 
							     int s_col, int c_row, int c_col) {
	  return accessor(d, parity, x, s_row, s_col, c_row, c_col);
	}
	
	/** Returns the number of field colors */
	__device__ __host__ inline int Ncolor() const { return nColor; }

	/** Returns the field volume */
	__device__ __host__ inline int Volume() const { return 2*volumeCB; }

	/** Returns the field volume */
	__device__ __host__ inline int VolumeCB() const { return volumeCB; }
      };

    /**
       FloatN ordering for clover fields
    */
    template <typename Float, int length, int N>
      struct FloatNOrder {
	typedef typename mapper<Float>::type RegType;
	Float *clover[2];
	float *norm[2];
	const int volumeCB;
	const int stride;

      FloatNOrder(const CloverField &clover, bool inverse, Float *clover_=0, float *norm_=0) : volumeCB(clover.VolumeCB()), stride(clover.Stride()) {
	this->clover[0] = clover_ ? clover_ : (Float*)(clover.V(inverse));
	this->clover[1] = (Float*)((char*)this->clover[0] + clover.Bytes()/2);
	this->norm[0] = norm_ ? norm_ : (float*)(clover.Norm(inverse));
	this->norm[1] = (float*)((char*)this->norm[0] + clover.NormBytes()/2);
      }
      
	__device__ __host__ inline void load(RegType v[length], int x, int parity) const {
	  const int M=length/(N*2);
	  for (int chirality=0; chirality<2; chirality++) {
	    for (int i=0; i<M; i++) {
	      for (int j=0; j<N; j++) {
		int intIdx = (chirality*M + i)*N + j; // internal dof index
		int padIdx = intIdx / N;
		copy(v[(chirality*M+i)*N+j], clover[parity][(padIdx*stride + x)*N + intIdx%N]);
		if (sizeof(Float)==sizeof(short)) v[(chirality*M+i)*N+j] *= norm[parity][chirality*volumeCB + x];
	      }
	    }
	  }
	}
  
	__device__ __host__ inline void save(const RegType v[length], int x, int parity) {
	  // find the norm of each chiral block
	  RegType scale[2];
	  if (sizeof(Float)==sizeof(short)) {
	    const int M = length/2;
	    for (int chi=0; chi<2; chi++) { // chirality
	      scale[chi] = 0.0;
	      for (int i=0; i<M; i++) 
		scale[chi] = fabs(v[chi*M+i]) > scale[chi] ? fabs(v[chi*M+i]) : scale[chi];
	      norm[parity][chi*volumeCB + x] = scale[chi];
	    }
	  }

	  const int M=length/(N*2);
	  for (int chirality=0; chirality<2; chirality++) {
	    for (int i=0; i<M; i++) {
	      for (int j=0; j<N; j++) {
		int intIdx = (chirality*M + i)*N + j;
		int padIdx = intIdx / N;
		if (sizeof(Float)==sizeof(short))
		  copy(clover[parity][(padIdx*stride + x)*N + intIdx%N], v[(chirality*M+i)*N+j] / scale[chirality]);
		else
		  copy(clover[parity][(padIdx*stride + x)*N + intIdx%N], v[(chirality*M+i)*N+j]);
	      }
	    }
	  }
	}

	size_t Bytes() const { 
	  size_t bytes = length*sizeof(Float);
	  if (sizeof(Float)==sizeof(short)) bytes += 2*sizeof(float);
	  return bytes;
	}
      };

    /**
       QDP ordering for clover fields
    */
    template <typename Float, int length>
      struct QDPOrder {
	typedef typename mapper<Float>::type RegType;
	Float *clover[2];
	const int volumeCB;
	const int stride;

      QDPOrder(const CloverField &clover, bool inverse, Float *clover_=0) 
      : volumeCB(clover.VolumeCB()), stride(volumeCB) {
	this->clover[0] = clover_ ? clover_ : (Float*)(clover.V(inverse));
	this->clover[1] = (Float*)((char*)this->clover[0] + clover.Bytes()/2);
      }

	__device__ __host__ inline void load(RegType v[length], int x, int parity) const {
	  for (int i=0; i<length; i++) v[i] = 0.5*clover[parity][x*length+i]; // factor of 0.5 comes from basis change
	}
  
	__device__ __host__ inline void save(const RegType v[length], int x, int parity) {
	  for (int i=0; i<length; i++) clover[parity][x*length+i] = 2.0*v[i];
	}

	size_t Bytes() const { return length*sizeof(Float); }
      };

    /**
       QDPJIT ordering for clover fields
    */
    template <typename Float, int length>
      struct QDPJITOrder {
	typedef typename mapper<Float>::type RegType;
	Float *diag; 	   /**< Pointers to the off-diagonal terms (two parities) */
	Float *offdiag;   /**< Pointers to the diagonal terms (two parities) */
	const int volumeCB;
	const int stride;

      QDPJITOrder(const CloverField &clover, bool inverse, Float *clover_=0) 
      : volumeCB(clover.VolumeCB()), stride(volumeCB) {
	offdiag = clover_ ? ((Float**)clover_)[0] : ((Float**)clover.V(inverse))[0];
	diag = clover_ ? ((Float**)clover_)[1] : ((Float**)clover.V(inverse))[1];
      }
	
	__device__ __host__ inline void load(RegType v[length], int x, int parity) const {
	  // the factor of 0.5 comes from a basis change
	  for (int chirality=0; chirality<2; chirality++) {
	    // set diagonal elements
	    for (int i=0; i<6; i++) {
	      v[chirality*36 + i] = 0.5*diag[((i*2 + chirality)*2 + parity)*volumeCB + x];
	    }

	  // the off diagonal elements
	    for (int i=0; i<30; i++) {
	      int z = i%2;
	      int off = i/2;
	      const int idtab[15]={0,1,3,6,10,2,4,7,11,5,8,12,9,13,14};
	      v[chirality*36 + 6 + i] = 0.5*offdiag[(((z*15 + idtab[off])*2 + chirality)*2 + parity)*volumeCB + x];
	    }

	  }
	}
  
	__device__ __host__ inline void save(const RegType v[length], int x, int parity) {
	  // the factor of 2.0 comes from undoing the basis change
	  for (int chirality=0; chirality<2; chirality++) {
	    // set diagonal elements
	    for (int i=0; i<6; i++) {
	      diag[((i*2 + chirality)*2 + parity)*volumeCB + x] = 2.0*v[chirality*36 + i];
	    }

	    // the off diagonal elements
	    for (int i=0; i<30; i++) {
	      int z = i%2;
	      int off = i/2;
	      const int idtab[15]={0,1,3,6,10,2,4,7,11,5,8,12,9,13,14};
	      offdiag[(((z*15 + idtab[off])*2 + chirality)*2 + parity)*volumeCB + x] = 2.0*v[chirality*36 + 6 + i];
	    }
	  }
	}
	
	size_t Bytes() const { return length*sizeof(Float); }
      };
      

    /**
       BQCD ordering for clover fields
       struct for reordering a BQCD clover matrix into the order that is
       expected by QUDA.  As well as reordering the clover matrix
       elements, we are also changing basis.
    */
    template <typename Float, int length>
      struct BQCDOrder {
	typedef typename mapper<Float>::type RegType;
	Float *clover[2];
	const int volumeCB;
	const int stride;

      BQCDOrder(const CloverField &clover, bool inverse, Float *clover_=0) 
      : volumeCB(clover.Stride()), stride(volumeCB) {
	this->clover[0] = clover_ ? clover_ : (Float*)(clover.V(inverse));
	this->clover[1] = (Float*)((char*)this->clover[0] + clover.Bytes()/2);
      }

	/**
	   @param v The output clover matrix in QUDA order
	   @param x The checkerboarded lattice site
	   @param parity The parity of the lattice site
	*/
	__device__ __host__ inline void load(RegType v[length], int x, int parity) const {
	  int bq[36] = { 21, 32, 33, 0,  1, 20,                   // diagonal
			 28, 29, 30, 31, 6, 7,  14, 15, 22, 23,   // column 1  6
			 34, 35, 8, 9, 16, 17, 24, 25,            // column 2  16
			 10, 11, 18, 19, 26, 27,                  // column 3  24
			 2,  3,  4,  5,                           // column 4  30
			 12, 13};
	
	  // flip the sign of the imaginary components
	  int sign[36];
	  for (int i=0; i<6; i++) sign[i] = 1;
	  for (int i=6; i<36; i+=2) {
	    if ( (i >= 10 && i<= 15) || (i >= 18 && i <= 29) )  { sign[i] = -1; sign[i+1] = -1; }
	    else { sign[i] = 1; sign[i+1] = -1; }
	  }
	
	  const int M=length/2;
	  for (int chirality=0; chirality<2; chirality++) 
	    for (int i=0; i<M; i++) 
	      v[chirality*M+i] = sign[i] * clover[parity][x*length+chirality*M+bq[i]];
	
	}
  
	// FIXME implement the save routine for BQCD ordered fields
	__device__ __host__ inline void save(RegType v[length], int x, int parity) {

	};

	size_t Bytes() const { return length*sizeof(Float); }
      };

  } // namespace clover
} // namespace quda

#endif //_CLOVER_ORDER_H

