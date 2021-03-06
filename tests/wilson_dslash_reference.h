#ifndef _WILSON_DSLASH_REFERENCE_H
#define _WILSON_DSLASH_REFERENCE_H

#include <enum_quda.h>
#include <quda.h>

#ifdef __cplusplus
extern "C" {
#endif

  void wil_dslash(void *res, void **gauge, void *spinorField, int oddBit,
		  int daggerBit, QudaPrecision precision, QudaGaugeParam &param);
  
  void wil_mat(void *out, void **gauge, void *in, double kappa, int daggerBit,
	       QudaPrecision precision, QudaGaugeParam &param);

  void wil_matpc(void *out, void **gauge, void *in, double kappa,
		 QudaMatPCType matpc_type,  int daggerBit, QudaPrecision precision, QudaGaugeParam &param);

  void tm_dslash(void *res, void **gauge, void *spinorField, double kappa,
		 double mu, QudaTwistFlavorType flavor, int oddBit, QudaMatPCType matpc_type,
		 int daggerBit, QudaPrecision sprecision, QudaGaugeParam &param);
  
  void tm_mat(void *out, void **gauge, void *in, double kappa, double mu,
	      QudaTwistFlavorType flavor, int daggerBit, QudaPrecision precision, QudaGaugeParam &param);

  void tm_matpc(void *out, void **gauge, void *in, double kappa, double mu,
		QudaTwistFlavorType flavor, QudaMatPCType matpc_type,  
		int daggerBit, QudaPrecision precision, QudaGaugeParam &param);

  void tm_ndeg_dslash(void *res1, void *res2, void **gaugeFull, void *spinorField1, void *spinorField2, 
		      double kappa, double mu,  double epsilon, int oddBit, int daggerBit, QudaMatPCType matpc_type, 
		      QudaPrecision precision, QudaGaugeParam &gauge_param) ;
  void tm_ndeg_matpc(void *outEven1, void *outEven2, void **gauge, void *inEven1, void *inEven2, double kappa, double mu, double epsilon,
	   QudaMatPCType matpc_type, int dagger_bit, QudaPrecision precision, QudaGaugeParam &gauge_param);
		      
  void tm_ndeg_mat(void *evenOut, void* oddOut, void **gauge, void *evenIn, void *oddIn,  
		   double kappa, double mu, double epsilon, int dagger_bit, QudaPrecision precision, QudaGaugeParam &gauge_param);		      

#ifdef __cplusplus
}
#endif

#endif // _WILSON_DSLASH_REFERENCE_H
