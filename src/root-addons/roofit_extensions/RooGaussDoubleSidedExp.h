/*****************************************************************************
 * Project: RooFit                                                           *
 *                                                                           *
 * This code was autogenerated by RooClassFactory                            *
 *****************************************************************************/
#ifndef ROOGAUSSDOUBLESIDEDEXP
#define ROOGAUSSDOUBLESIDEDEXP
#include "RooAbsPdf.h"
#include "RooRealProxy.h"
#include "RooCategoryProxy.h"
#include "RooAbsReal.h"
#include "RooAbsCategory.h"
class RooGaussDoubleSidedExp : public RooAbsPdf {
 public:
  RooGaussDoubleSidedExp() {} ; 
  RooGaussDoubleSidedExp(const char *name, const char *title,
			 RooAbsReal& _m,
			 RooAbsReal& _m0,
			 RooAbsReal& _sigma,
			 RooAbsReal& _alphaLo,
			 RooAbsReal& _alphaHi
			 );
  RooGaussDoubleSidedExp(const RooGaussDoubleSidedExp& other, const char* name=0) ;
  virtual TObject* clone(const char* newname) const { return new RooGaussDoubleSidedExp(*this,newname); }
  // inline virtual ~RooGaussDoubleSidedExp() { }

  virtual Int_t getAnalyticalIntegral( RooArgSet& allVars,  RooArgSet& analVars, const char* rangeName=0 ) const;
  virtual Double_t analyticalIntegral( Int_t code, const char* rangeName=0 ) const;

 protected:
  RooRealProxy m ;
  RooRealProxy m0 ;
  RooRealProxy sigma ;
  RooRealProxy alphaLo ;
  RooRealProxy alphaHi ;
  Double_t evaluate() const ;

  double gaussianIntegral(double tmin, double tmax) const;
  double tailIntegral(double tmin, double tmax, double alpha) const;

 private:
  ClassDef(RooGaussDoubleSidedExp,1) // Your description goes here...
};
#endif
