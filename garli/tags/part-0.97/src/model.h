// GARLI version 0.96b8 source code
// Copyright 2005-2008 Derrick J. Zwickl
// email: zwickl@nescent.org
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef _MODEL_
#define _MODEL_

#if !defined(_MSC_VER)
#define _stricmp strcasecmp
#endif

#include <iostream>
#include <cassert>
#include <math.h>
#include <vector>

using namespace std;

#include "rng.h"
#include "sequencedata.h"
#include "configoptions.h"
#include "errorexception.h"

class ModelSpecification;
class ModelSpecificationSet;
class MFILE;
class Individual;
class ClaSpecifier;

extern rng rnd;
//extern ModelSpecification modSpec;
extern ModelSpecificationSet modSpecSet;
extern bool FloatingPointEquals(const FLOAT_TYPE first, const FLOAT_TYPE sec, const FLOAT_TYPE epsilon);

extern vector<ClaSpecifier> claSpecs;

	enum{//the types
		STATEFREQS = 1,
		RELATIVERATES = 2,
		ALPHASHAPE = 3,
		RATEMULTS = 4,
		RATEPROPS = 5,
		PROPORTIONINVARIANT = 6,
		SUBSETRATE = 7
		};

class BaseParameter {
protected:
	NxsString name;
	int type;
	int numElements;
	bool fixed;
	FLOAT_TYPE maxv,minv;
	FLOAT_TYPE mutationWeight;
	FLOAT_TYPE mutationProb;
	vector<FLOAT_TYPE*> vals; //this will be aliased to the actual 
						//parameter value within the model class (sneaky!)
	vector<FLOAT_TYPE> default_vals;

public:
	vector<int> modelsThatInclude;//this isn't currently being used, but will eventually be used to only dirty models that changed

	BaseParameter()	{
		numElements=1;
		maxv=minv=0.0;
		}

	BaseParameter(const char *n, FLOAT_TYPE **dv, int t, int numE, FLOAT_TYPE mn, FLOAT_TYPE mx, int modNum) {
		vals.reserve(6);
		default_vals.reserve(6);
		name=n;
		type=t;
		numElements=numE;
		for(int i=0;i<numElements;i++){
			default_vals.push_back(*(dv[i]));
			vals.push_back(dv[i]);
			}
		minv=mn;
		maxv=mx;
		fixed=false;
		//PARTITION this is a hack for now that doesn't allow linking
		modelsThatInclude.push_back(modNum);
		}
	~BaseParameter(){};
/*	void Report(ostream &out){
		out << "Type:\t" << name << "\n";
		if(numElements > 1)
			out << "Current value:\t";
		else 
			out << "Current values:\t";
		for(int i=0;i<numElements;i++)
			out << vals[

		}
*/
	void SetFixed(bool tf) {fixed=tf;}
	bool IsFixed() const {return fixed;}
	int Type() const {return type;}
	void SetWeight(FLOAT_TYPE w){mutationWeight=w;}
	FLOAT_TYPE GetWeight(){return mutationWeight;}
	void SetProb(FLOAT_TYPE p){mutationProb=p;}
	FLOAT_TYPE GetProb(){return mutationProb;}
	virtual void Mutator(FLOAT_TYPE) = 0;
	void SetToDefaultValues(){
		for(int e=0;e<numElements;e++) *vals[e] = default_vals[e];
		}
	};

class StateFrequencies:public BaseParameter{

public:
	StateFrequencies(FLOAT_TYPE **dv, int numE, int modnum):BaseParameter("Base frequencies", dv, STATEFREQS, numE, 1e-4, 0.999, modnum){};

	void Mutator(FLOAT_TYPE mutationShape){
		int freqToChange=int(rnd.uniform()*numElements);
		FLOAT_TYPE newFreq=*vals[freqToChange] * rnd.gamma( mutationShape );
		for(int b=0;b<numElements;b++)
			if(b!=freqToChange) *vals[b] *= (FLOAT_TYPE)((1.0-newFreq)/(1.0-*vals[freqToChange]));
		*vals[freqToChange]=newFreq;
		}
	};

class RelativeRates:public BaseParameter{
public:
	// 5/9/06 now enforcing non-zero minimum relative rate to avoid problems in the linear algebra functions
	RelativeRates(FLOAT_TYPE **dv, int numE, int modnum):BaseParameter("Rate matrix", dv, RELATIVERATES, numE, (FLOAT_TYPE)1.0e-6, (FLOAT_TYPE)999.9, modnum){};

	void Mutator(FLOAT_TYPE mutationShape){
		if(numElements > 1){
			int rateToChange=int(rnd.uniform()*(numElements));

			//3/25/08 had to change this to allow arbitrary rate matrices mutation
			//of a rate other than GT might actually alter GT, so we need to actually check
			//whether it is 1.0 or not
			//if(rateToChange<numElements-1){
				*vals[rateToChange] *= rnd.gamma( mutationShape );
				if(*vals[rateToChange]>maxv) *vals[rateToChange]=maxv;
				if(*vals[rateToChange]<minv) *vals[rateToChange]=minv;
			//	}
			if(FloatingPointEquals(*vals[numElements-1], ONE_POINT_ZERO, 1.0e-12) == false){
				FLOAT_TYPE scaler = ONE_POINT_ZERO / *vals[numElements-1];
				for(int i=0;i<numElements-1;i++){
					if(vals[i] != vals[numElements-1]){
						*vals[i] *= scaler;
						if(*vals[i]>maxv) *vals[i]=maxv;
						if(*vals[i]<minv) *vals[i]=minv;
						}
					}
				*vals[numElements - 1] *= scaler;
#ifdef SINGLE_PRECISION_FLOATS
				assert(FloatingPointEquals(*vals[numElements-1], ONE_POINT_ZERO, 1.0e-6));
#else
				assert(FloatingPointEquals(*vals[numElements-1], ONE_POINT_ZERO, 1.0e-12));
#endif

				}
	/*		if(rateToChange<numElements-1){
				*vals[rateToChange] *= rnd.gamma( mutationShape );
				if(*vals[rateToChange]>maxv) *vals[rateToChange]=maxv;
				if(*vals[rateToChange]<minv) *vals[rateToChange]=minv;
				}
			else{//if we alter the reference rate, which we are assuming
				//is the last one (GT for DNA models, fixed to 1.0)
				//scale all of the other rates
				FLOAT_TYPE scaler= rnd.gamma( mutationShape );
				for(int i=0;i<numElements-1;i++){
					*vals[i] /= scaler;
					if(*vals[i]>maxv) *vals[i]=maxv;
					if(*vals[i]<minv) *vals[i]=minv;
					}
				}
	*/		}
		else {
			*vals[0] *= rnd.gamma( mutationShape );
			if(*vals[0]>maxv) *vals[0]=maxv;
			if(*vals[0]<minv) *vals[0]=minv;
			}
		}
	};

class RateProportions:public BaseParameter{
public:
	RateProportions(FLOAT_TYPE **dv, int numE, int modnum):BaseParameter("Rate props", dv, RATEPROPS, numE, 1e-5, 0.999, modnum){};
	void Mutator(FLOAT_TYPE mutationShape){
		int rateToChange=int(rnd.uniform()*(numElements));
		*vals[rateToChange] *= rnd.gamma( mutationShape );
		if(*vals[rateToChange]>maxv) *vals[rateToChange]=maxv;		

		FLOAT_TYPE newTot = 1.0 - *vals[rateToChange];
		FLOAT_TYPE oldTot = 0.0;
		for(int i=0;i<numElements;i++)
			if(i != rateToChange) oldTot += *vals[i];
		for(int i=0;i<numElements;i++)
			if(i != rateToChange) *vals[i] *= newTot / oldTot;
		newTot = 0.0;
		for(int i=0;i<numElements;i++) newTot += *vals[i];
		assert(fabs(newTot - 1.0) < 0.0001);
		}
	};

class RateMultipliers:public BaseParameter{
public:
	RateMultipliers(FLOAT_TYPE **dv, int numE, int modnum):BaseParameter("Rate mults", dv, RATEMULTS, numE, (FLOAT_TYPE)1e-5, (FLOAT_TYPE)999.9, modnum){};
	void Mutator(FLOAT_TYPE mutationShape){
		int rateToChange=int(rnd.uniform()*(numElements));
		*vals[rateToChange] *= rnd.gamma( mutationShape );
		if(*vals[rateToChange]>maxv) *vals[rateToChange]=maxv;
		}
	};

class AlphaShape:public BaseParameter{
public:
	AlphaShape(FLOAT_TYPE **dv, int modnum):BaseParameter("Alpha shape", dv, ALPHASHAPE, 1, (FLOAT_TYPE)1e-5, (FLOAT_TYPE)999.9, modnum){};
	void Mutator(FLOAT_TYPE mutationShape){
		*vals[0] *=rnd.gamma( mutationShape );
		}
	};

class ProportionInvariant:public BaseParameter{
public:
	ProportionInvariant(FLOAT_TYPE **dv, int modnum):BaseParameter("Prop. invar.", dv, PROPORTIONINVARIANT, 1, (FLOAT_TYPE)0.0, (FLOAT_TYPE)1.0, modnum){};
	void Mutator(FLOAT_TYPE mutationShape){
		*vals[0] *=rnd.gamma( mutationShape );
		}
	};

class SubsetRates:public BaseParameter{
public:
	SubsetRates(FLOAT_TYPE **dv, int numE, int modnum):BaseParameter("Subset rate", dv, SUBSETRATE, numE, (FLOAT_TYPE)0.0, (FLOAT_TYPE)1.0, modnum){};
	void Mutator(FLOAT_TYPE mutationShape){
		int rateToChange=int(rnd.uniform()*(numElements));
		*vals[rateToChange] *= rnd.gamma( mutationShape );
		if(*vals[rateToChange]>maxv) *vals[rateToChange]=maxv;
		}
	};

class ModelSpecification{
	//this will hold the model specification as a global variable
	//so that any models allocated will immediately know what they are
public:
	bool isSetup;
	int nstates;
	int numRateCats;

	bool fixStateFreqs;
	bool fixRelativeRates;
	string arbitraryRateMatrixString;

//	bool fixSubstitutionRates;
	//bool flexRates;

	bool fixInvariantSites;
	bool fixAlpha;
	bool includeInvariantSites;
	
	bool gotRmatFromFile;
	bool gotStateFreqsFromFile;
	bool gotAlphaFromFile;
	bool gotFlexFromFile;
	bool gotPinvFromFile;
	bool gotOmegasFromFile;

	enum{
		DNA = 0,
		RNA = 1,
		CODON = 2,
		AMINOACID = 3,
		CODONAMINOACID = 4,
		NSTATE = 5,
		NSTATEV = 6
		}datatype;
	
	enum{
		EQUAL = 0,
		EMPIRICAL = 1,
		ESTIMATE = 2,
		F1X4 = 3,
		F3X4 = 4,
		JONES = 5,
		DAYHOFF = 6,
		WAG = 8,
		MTMAM = 9,
		MTREV = 10,
		USERSPECIFIED = 20
		}stateFrequencies;

	enum{
		NST1 = 0,
		NST2 = 1,
		NST6 = 2,
		ARBITRARY = 3,
		JONESMAT = 5,
		DAYHOFFMAT = 6,
		POISSON = 7,
		WAGMAT = 8,
		MTMAMMAT = 9,
		MTREVMAT = 10,
		USERSPECIFIEDMAT = 20
		}rateMatrix;
	
	enum{
		NONE = 0,
		GAMMA = 1,
		FLEX  = 2,
		NONSYN = 3
		}rateHetType;

	enum{
		STANDARD = 0,
		VERTMITO = 1,
		INVERTMITO = 2
		}geneticCode;	

	ModelSpecification(){
		nstates=4;
		//this is the default model
		SetGTR();
		SetGammaRates();
		SetNumRateCats(4, false);
		SetInvariantSites();
		datatype=DNA;
		gotRmatFromFile = gotStateFreqsFromFile = gotAlphaFromFile = gotFlexFromFile = gotPinvFromFile = gotOmegasFromFile = false;
		geneticCode=STANDARD;
		isSetup = false;
		}

	bool IsCodon() const {return datatype == CODON;}
	bool IsNucleotide() const {return (datatype == DNA || datatype == RNA);}
	bool IsAminoAcid() const {return (datatype == AMINOACID || datatype == CODONAMINOACID);}//for most purposes codon-aminoacid should be considered AA
	bool IsCodonAminoAcid() const {return datatype == CODONAMINOACID;}
	bool IsNState() const {return datatype == NSTATE;}
	bool IsNStateV() const {return datatype == NSTATEV;}

	bool GotAnyParametersFromFile() const{
		return gotRmatFromFile || gotStateFreqsFromFile || gotAlphaFromFile || gotFlexFromFile || gotPinvFromFile || gotOmegasFromFile;
		}
	//A number of canned model setups
	
	void SetJC(){
		nstates=4;
		rateMatrix = NST1;
		SetEqualStateFreqs();
		fixRelativeRates=true;
		}

	void K2P(){
		nstates=4;
		rateMatrix = NST2;
		SetEqualStateFreqs();
		fixRelativeRates=false;
		}

	void SetF81(){
		nstates=4;
		rateMatrix = NST1;
		SetEstimateStateFreqs();
		fixRelativeRates=true;	
		}

	void SetHKY(){
		nstates=4;
		rateMatrix = NST2;
		SetEstimateStateFreqs();
		fixRelativeRates=false;
		}

	void SetGTR(){
		nstates=4;
		rateMatrix = NST6;
		SetEstimateStateFreqs();
		fixRelativeRates=false;
		}

	void SetCodon(){
		datatype = CODON;
		rateMatrix = NST2;
		stateFrequencies = EQUAL;
		nstates = 61;//this might be overridden if a nonstandard code is set
		numRateCats = 1;
		fixRelativeRates=false;
		}

	void SetAminoAcid(){
		datatype = AMINOACID;
		rateMatrix = WAGMAT;
		stateFrequencies = WAG;
		nstates = 20;
		fixRelativeRates=true;
		}

	void SetCodonAminoAcid(){
		datatype = CODONAMINOACID;
		rateMatrix = WAGMAT;
		stateFrequencies = WAG;
		nstates = 20;
		fixRelativeRates=true;
		}

	void SetNState(){
		datatype = NSTATE;
		rateMatrix = NST1;
		stateFrequencies = EQUAL;
		nstates = -1; //this will need to be reset later 
		fixRelativeRates=true;
		fixStateFreqs=true;
		}

	void SetNStateV(){
		datatype = NSTATEV;
		rateMatrix = NST1;
		stateFrequencies = EQUAL;
		nstates = -1; //this will need to be reset later 
		fixRelativeRates=true;
		fixStateFreqs=true;
		}

	void SetNStates(int ns){
		nstates = ns;
		}

	void SetGammaRates(){
		rateHetType = GAMMA;
		fixAlpha=false;
		}

	void SetFlexRates(){
		if(includeInvariantSites==true) throw(ErrorException("Invariant sites models should not be (and don't need to be) used\n     with the \"flex\" model of rate heterogeneity, since flex is able to\n     incorporate a class of sites with a rate of effectively zero"));
		rateHetType = FLEX;
		}

	void SetNumRateCats(int nrates, bool test){//correct behavior here depends on the fact that the default 
		//model includes gamma with 4 rate cats
		if(test ==true){
			if(rateHetType == NONE && nrates > 1)
				throw(ErrorException("ratehetmodel set to \"none\", but numratecats is equal to %d!", nrates));
			if(rateHetType != NONE && nrates == 1){
				if(rateHetType == GAMMA && fixAlpha == false)
					throw(ErrorException("ratehetmodel set to \"gamma\", but numratecats is equal to 1!"));
				else if(rateHetType == GAMMA && fixAlpha == true)
					throw(ErrorException("ratehetmodel set to \"gammafixed\", but numratecats is equal to 1!"));
				else if(rateHetType == FLEX)
					throw(ErrorException("ratehetmodel set to \"flex\", but numratecats is equal to 1!"));
				else if(rateHetType == NONSYN)
					throw(ErrorException("ratehetmodel set to \"nonsynonymous\", but numratecats is equal to 1!"));				
				}
			}
		
		if(nrates < 1) throw(ErrorException("1 is the minimum value for numratecats."));
		if(nrates > 20) throw(ErrorException("20 is the maximum value for numratecats."));
		numRateCats=nrates;
		}

	void SetInvariantSites(){
		if(rateHetType == NONSYN)
			throw(ErrorException("invariant sites cannot be used with nonsynonymous rate variation"));			
		includeInvariantSites=true;
		fixInvariantSites=false;
		}

	void RemoveInvariantSites(){
		includeInvariantSites=false;
		fixInvariantSites=false;
		}

	void SetEmpiricalStateFreqs(){
		stateFrequencies = EMPIRICAL;
		fixStateFreqs=true;
		}

	void SetEqualStateFreqs(){
		stateFrequencies = EQUAL;
		fixStateFreqs=true;
		}
	void SetUserSpecifiedStateFreqs(){
		stateFrequencies = USERSPECIFIED;
		fixStateFreqs=true;
		}
	void SetEstimateStateFreqs(){
		stateFrequencies = ESTIMATE;
		fixStateFreqs=false;
		}
	void SetF1X4StateFreqs(){
		stateFrequencies = F1X4;
		fixStateFreqs = true;
		}
	void SetF3X4StateFreqs(){
		stateFrequencies = F3X4;
		fixStateFreqs = true;
		}
	void SetFixedAlpha(){
		fixAlpha=true;
		}
	void SetFixedInvariantSites(){
		fixInvariantSites=true;
		includeInvariantSites=true;
		}
	void SetUserSpecifiedRateMatrix(){
		rateMatrix = USERSPECIFIEDMAT;
		fixRelativeRates=true;
		}
	void SetJonesAAMatrix(){
		rateMatrix = JONESMAT;
		fixRelativeRates=true;
		}
	void SetPoissonAAMatrix(){
		rateMatrix = POISSON;
		fixRelativeRates=true;
		}	
	void SetDayhoffAAMatrix(){
		rateMatrix = DAYHOFFMAT;
		fixRelativeRates=true;
		}
	void SetWAGAAMatrix(){
		rateMatrix = WAGMAT;
		fixRelativeRates=true;
		}	
	
	void SetMtMamAAMatrix(){
		rateMatrix = MTMAMMAT;
		fixRelativeRates=true;
		}	

	void SetMtRevAAMatrix(){
		rateMatrix = MTREVMAT;
		fixRelativeRates=true;
		}	

	void SetJonesAAFreqs(){
		stateFrequencies = JONES;
		fixStateFreqs=true;
		}
	void SetDayhoffAAFreqs(){
		stateFrequencies = DAYHOFF;
		fixStateFreqs=true;	
		}
	void SetWAGAAFreqs(){
		stateFrequencies = WAG;
		fixStateFreqs=true;
		}

	void SetMtMamAAFreqs(){
		stateFrequencies = MTMAM;
		fixStateFreqs=true;
		}

	void SetMtRevAAFreqs(){
		stateFrequencies = MTREV;
		fixStateFreqs=true;
		}

	void SetM3Model(){
		rateHetType = NONSYN;
		numRateCats = 3;
		}

	int Nst() const {
		if(rateMatrix == NST1) return 1;
		else if(rateMatrix == NST2) return 2;
		else if(rateMatrix == NST6 || rateMatrix == ARBITRARY) return 6;
		else if(rateMatrix == USERSPECIFIEDMAT && datatype != AMINOACID && datatype != CODONAMINOACID) return 6;
		else assert(0);
		return -1;
		}

	bool IsJonesAAFreqs() const {return (stateFrequencies == JONES);}
	bool IsJonesAAMatrix() const {return (rateMatrix == JONESMAT);}
	bool IsDayhoffAAFreqs() const {return (stateFrequencies == DAYHOFF);}
	bool IsDayhoffAAMatrix() const {return (rateMatrix == DAYHOFFMAT);}
	bool IsWAGAAFreqs() const {return (stateFrequencies == WAG);}
	bool IsWAGAAMatrix() const {return (rateMatrix == WAGMAT);}
	bool IsMtMamAAFreqs() const {return (stateFrequencies == MTMAM);}
	bool IsMtMamAAMatrix() const {return (rateMatrix == MTMAMMAT);}
	bool IsMtRevAAFreqs() const {return (stateFrequencies == MTREV);}
	bool IsMtRevAAMatrix() const {return (rateMatrix == MTREVMAT);}
	bool IsVertMitoCode() const {return (geneticCode == VERTMITO);}
	bool IsInvertMitoCode() const {return (geneticCode == INVERTMITO);}
	bool IsPoissonAAMatrix() const {return (rateMatrix == POISSON);}
	bool IsUserSpecifiedRateMatrix() const {return rateMatrix == USERSPECIFIEDMAT;}
	bool IsArbitraryRateMatrix() const {return rateMatrix == ARBITRARY;}
	const string GetArbitraryRateMatrixString() const {return arbitraryRateMatrixString;}

	bool IsEqualStateFrequencies() const {return stateFrequencies == EQUAL;}
	bool IsEmpiricalStateFrequencies() const {return stateFrequencies == EMPIRICAL;}
	bool IsUserSpecifiedStateFrequencies() const {return stateFrequencies == USERSPECIFIED;}
	bool IsF3x4StateFrequencies() const {return stateFrequencies == F3X4;}
	bool IsF1x4StateFrequencies() const {return stateFrequencies == F1X4;}
	bool IsPrecaledAAFreqs() const {return (IsAminoAcid() && (stateFrequencies == DAYHOFF || stateFrequencies == JONES || stateFrequencies == WAG || stateFrequencies == MTMAM || stateFrequencies == MTREV));}

	bool IsFlexRateHet() const {return rateHetType == FLEX;}
	bool IsGammaRateHet() const {return rateHetType == GAMMA;}
	bool IsNonsynonymousRateHet() const {return rateHetType == NONSYN;}

	void SetStateFrequencies(const char *str){
		if((datatype == NSTATE || datatype == NSTATEV) && _stricmp(str, "equal") != 0) 
			throw(ErrorException("Invalid setting for statefrequencies: %s\n\tOnly equal state frequencies are currently available for the standard data type", str));
		if(_stricmp(str, "equal") == 0) SetEqualStateFreqs();
		else if(_stricmp(str, "estimate") == 0){
			if(datatype == CODON) throw ErrorException("Sorry, ML estimation of equilibrium frequencies is not available under\ncodon models.  Try statefrequencies = empirical");
			else if(datatype == AMINOACID || datatype == CODONAMINOACID) outman.UserMessage("\nWARNING: to obtain good ML estimates of equilibrium aminoacid frequencies you\n\tmay need to run for a very long time or increase the modweight.\n\tConsider statefrequencies = empirical instead.\n");
			SetEstimateStateFreqs();
			}
		else if(_stricmp(str, "empirical") == 0) SetEmpiricalStateFreqs();
		else if(_stricmp(str, "fixed") == 0) SetUserSpecifiedStateFreqs();
		else if(datatype == CODON && _stricmp(str, "f1x4") == 0) SetF1X4StateFreqs();
		else if(datatype == CODON && _stricmp(str, "f3x4") == 0) SetF3X4StateFreqs();
		else if((datatype == AMINOACID || datatype == CODONAMINOACID) && _stricmp(str, "jones") == 0) SetJonesAAFreqs();
		else if((datatype == AMINOACID || datatype == CODONAMINOACID) && _stricmp(str, "dayhoff") == 0) SetDayhoffAAFreqs();
		else if((datatype == AMINOACID || datatype == CODONAMINOACID) && _stricmp(str, "wag") == 0) SetWAGAAFreqs();
		else if((datatype == AMINOACID || datatype == CODONAMINOACID) && _stricmp(str, "mtmam") == 0) SetMtMamAAFreqs();
		else if((datatype == AMINOACID || datatype == CODONAMINOACID) && _stricmp(str, "mtrev") == 0) SetMtRevAAFreqs();
		else throw(ErrorException("Invalid setting for statefrequencies: %s\n\tOptions for all datatypes: equal, empirical, fixed\n\tFor all datatypes besides codon: estimate\n\tFor aminoacid datatype only: poisson, dayhoff, jones, wag, mtmam, mtrev\n\tFor codon datatype only: F1X4, F3X4", str));
		}
	void SetRateMatrix(const char *str){
		if(datatype == AMINOACID || datatype == CODONAMINOACID){
			if(_stricmp(str, "jones") == 0) SetJonesAAMatrix();
			else if(_stricmp(str, "dayhoff") == 0) SetDayhoffAAMatrix();
			else if(_stricmp(str, "poisson") == 0) SetPoissonAAMatrix();
			else if(_stricmp(str, "wag") == 0) SetWAGAAMatrix();
			else if(_stricmp(str, "mtmam") == 0) SetMtMamAAMatrix();
			else if(_stricmp(str, "mtrev") == 0) SetMtRevAAMatrix();
			else throw(ErrorException("Sorry, %s is not a valid aminoacid rate matrix. \n\t(Options are: dayhoff, jones, poisson, wag, mtmam, mtrev)", str));
			}
		else if(datatype == NSTATE || datatype == NSTATEV){
			if(_stricmp(str, "1rate") != 0) throw(ErrorException("Sorry, %s is not a valid ratematrix setting for the standard data type.\n\tOnly 1rate matrices are currently allowed.", str));
			else rateMatrix = NST1;
			}
		else{
			if(_stricmp(str, "6rate") == 0) rateMatrix = NST6;
			else if(_stricmp(str, "2rate") == 0) rateMatrix = NST2;
			else if(_stricmp(str, "1rate") == 0) rateMatrix = NST1;
			else if(_stricmp(str, "fixed") == 0) SetUserSpecifiedRateMatrix();
			else if(str[0] == '('){
				rateMatrix = ARBITRARY;
				arbitraryRateMatrixString = str;
				}
			else{
				if(datatype == CODON) throw(ErrorException("Unknown setting for codon ratematrix: %s\n\t(options are: 6rate, 2rate, 1rate, fixed)", str));
				else throw(ErrorException("Unknown setting for dna/rna ratematrix: %s\n\t(options are: 6rate, 2rate, 1rate, fixed)", str));
				}
			}
		}
	void SetProportionInvariant(const char *str){
		if(_stricmp(str, "none") == 0) RemoveInvariantSites();
		//else if(datatype == CODON || datatype == AMINOACID) throw(ErrorException("Sorry, invariant sites not yet supported with Codon/Aminoacid data"));
		else if(datatype == CODON){
			if(_stricmp(str, "fixed") == 0 || _stricmp(str, "estimate") == 0) 
				throw ErrorException("Invariant sites cannot be used with codon models.\n     Try ratehetmodel = nonsynonymous to allow dN/dS variation across sites");
			else throw(ErrorException("Unknown setting for proportioninvariant: %s\n\t(only valid option for codon models is none)", str));
			}
		else if(_stricmp(str, "fixed") == 0) SetFixedInvariantSites();
		else if(_stricmp(str, "estimate") == 0) SetInvariantSites();
		else throw(ErrorException("Unknown setting for proportioninvariant: %s\n\t(options are: none, fixed, estimate)", str));
		}
	void SetRateHetModel(const char *str){
	//	if((datatype != DNA) && (datatype != AMINOACID) && _stricmp(str, "none")) throw(ErrorException("Sorry, rate heterogeneity not yet supported with Codon/Aminoacid data"));
		if(datatype == CODON){
			if(_stricmp(str, "nonsynonymous") == 0) SetM3Model();
			else if(_stricmp(str, "none") == 0){
				SetNumRateCats(1, false);
				rateHetType = NONE;
				}
			else if(_stricmp(str, "gamma") == 0) throw ErrorException("Gamma rate heterogeneity cannot be used with codon models.\n     Try ratehetmodel = nonsynonymous to allow dN/dS variation across sites");
			else throw(ErrorException("Unknown setting for ratehetmodel: %s\n\t(options for codon datatype are: nonsynonymous, none)", str));
			}
		else{			
			if(_stricmp(str, "gamma") == 0) SetGammaRates();
			else if(_stricmp(str, "gammafixed") == 0){
				SetGammaRates();
				SetFixedAlpha();
				}
			else if(_stricmp(str, "flex") == 0) SetFlexRates();
			else if(_stricmp(str, "none") == 0){
				SetNumRateCats(1, false);
				rateHetType = NONE;
				}
			else throw(ErrorException("Unknown setting for ratehetmodel: %s\n\t(options are for nucleotide or aminoacid data are: gamma, gammafixed, flex, none)", str));
			}
		}
	void SetDataType(const char *str){
		if(_stricmp(str, "codon") == 0) SetCodon();
		else if(_stricmp(str, "codon-aminoacid") == 0) SetCodonAminoAcid();
		else if(_stricmp(str, "aminoacid") == 0) SetAminoAcid();
		else if(_stricmp(str, "protein") == 0) SetAminoAcid();
		else if(_stricmp(str, "dna") == 0) str;
		else if(_stricmp(str, "rna") == 0) str;
		else if(_stricmp(str, "nucleotide") == 0) str;
		else if(_stricmp(str, "nstate") == 0) SetNState();
		else if(_stricmp(str, "standard") == 0) SetNState();
		else if(_stricmp(str, "mk") == 0) SetNState();
		else if(_stricmp(str, "standardvariable") == 0) SetNStateV();
		else if(_stricmp(str, "mkv") == 0) SetNStateV();
		else throw(ErrorException("Unknown setting for datatype: %s\n\t(options are: codon, codon-aminoacid, aminoacid, dna, rna, binary, nstate)", str));
		}
	void SetGeneticCode(const char *str){
		if(datatype != DNA && datatype != RNA){
			if(_stricmp(str, "standard") == 0) geneticCode = STANDARD;
			else if(_stricmp(str, "vertmito") == 0){
				geneticCode = VERTMITO;
				if(datatype == CODON) nstates = 60;
				}
			else if(_stricmp(str, "invertmito") == 0){
				geneticCode = INVERTMITO;
				if(datatype == CODON) nstates = 62;
				}
			else throw(ErrorException("Unknown genetic code: %s\n\t(options are: standard, vertmito, invertmito)", str));
			}
		}
	//PARTITION
	void SetupModSpec(const ConfigModelSettings &conf){
		SetDataType(conf.datatype.c_str());
		SetGeneticCode(conf.geneticCode.c_str());
		SetStateFrequencies(conf.stateFrequencies.c_str());
		SetRateMatrix(conf.rateMatrix.c_str());
		SetProportionInvariant(conf.proportionInvariant.c_str());
		SetRateHetModel(conf.rateHetModel.c_str());
		SetNumRateCats(conf.numRateCats, true);
		isSetup = true;
		}
/*
	void SetupModSpec(const GeneralGamlConfig &conf){
		SetDataType(conf.datatype.c_str());
		SetGeneticCode(conf.geneticCode.c_str());
		SetStateFrequencies(conf.stateFrequencies.c_str());
		SetRateMatrix(conf.rateMatrix.c_str());
		SetProportionInvariant(conf.proportionInvariant.c_str());
		SetRateHetModel(conf.rateHetModel.c_str());
		SetNumRateCats(conf.numRateCats, true);
		isSetup = true;
		}
*/
	};

class ModelSpecificationSet{
	//a set of ModelSpecifications, each corresponding to a data subset/modelset
	vector<ModelSpecification*> modSpecs;
	bool inferSubsetRates;
public:
	ModelSpecificationSet(){
		inferSubsetRates = true;
		}
	~ModelSpecificationSet(){
		for(int i = 0;i < modSpecs.size();i++)
			delete modSpecs[i];
		modSpecs.clear();
		}
	void Delete(){
		for(int i = 0;i < modSpecs.size();i++)
			delete modSpecs[i];
		modSpecs.clear();
		}
	void AddModSpec(const ConfigModelSettings &conf){
		ModelSpecification * mod = new ModelSpecification;
		mod->SetupModSpec(conf);
		modSpecs.push_back(mod);
		}
	ModelSpecification *GetModSpec(int num) const{
		if(num > -1 == false || num >= modSpecs.size()) 
			throw ErrorException("tried to access invalid ModSpec number");
		return modSpecs[num];
		}
	bool IsSetup(int num){return GetModSpec(num)->isSetup;}
	int NumSpecs() const {return modSpecs.size();}
	void SetInferSubsetRates(bool i){inferSubsetRates = i;}
	bool InferSubsetRates(){return inferSubsetRates;}
	bool GotAnyParametersFromFile() {
		for(vector<ModelSpecification *>::iterator msit = modSpecs.begin();msit != modSpecs.end();msit++)
			if((*msit)->GotAnyParametersFromFile()) return true;
		return false;
		}
	};

class Model{

	friend class ModelPartition;
	friend class ModelSet;

	int nst;
	int nstates;
	int nRateCats;
	ModelSpecification *modSpec;//pointer to the corresponding ModelSpecification
	int effectiveModels;//this is the number of models with different Q matrices
						//it does not include things like gamma or flex rates,
						//in which it is only the overall rate that varies
	bool includeInvariantSites;

	vector<FLOAT_TYPE*> stateFreqs;
	vector<FLOAT_TYPE*> relNucRates;
	int arbitraryMatrixIndeces[6];//this just keeps track of which rate parameters are aliased to single parameters
	vector<FLOAT_TYPE*> omegas;
	vector<FLOAT_TYPE*> omegaProbs;

	bool eigenDirty;
	FLOAT_TYPE *blen_multiplier; //this is the rescaling factor to make the mean rate in the qmat = 1

	FLOAT_TYPE rateMults[20];
	FLOAT_TYPE rateProbs[20];
	
	FLOAT_TYPE *alpha;
	FLOAT_TYPE *propInvar;

	//variables used for the eigen process if nst=6
	int *iwork, *indx;
	MODEL_FLOAT **eigvals, *eigvalsimag, ***eigvecs, ***inveigvecs, **teigvecs, *work, *temp, *col, **c_ijk, *EigValexp, *EigValderiv, *EigValderiv2;
	MODEL_FLOAT ***qmat, ***pmat1, ***pmat2;
	MODEL_FLOAT ***tempqmat;
	
	#ifdef SINGLE_PRECISION_FLOATS
	//these are used so that the transition matrices can be computed in double precision and
	//then copied to sinlge precision for use in the CLA/Deriv functions
	FLOAT_TYPE ***fpmat1, ***fpmat2;
	FLOAT_TYPE ***fderiv1, ***fderiv2;
	#endif
	
	//Newton Raphson crap
	MODEL_FLOAT ***deriv1, ***deriv2;

	//this will be a little bigger than necessary with some codes, but dynamically allocating a static is a bit of a pain
	static int qmatLookup[62*62];
	static GeneticCode *code;

	public:
//	static bool noPinvInModel;
//	static bool useFlexRates;
//	static int nRateCats;
	static FLOAT_TYPE mutationShape;
	FLOAT_TYPE maxPropInvar;

	vector<BaseParameter*> paramsToMutate;

	~Model();

	Model(int num){
		stateFreqs.reserve(4);
		relNucRates.reserve(6);
		paramsToMutate.reserve(5);
		//DEBUG - we should probably move this out of here.  It assumes that the
		//global modspec has been setup
		assert(modSpecSet.IsSetup(num));
		CreateModelFromSpecification(num);
		}

	void CalcMutationProbsFromWeights();
	FLOAT_TYPE GetTotalModelMutationWeight();
	BaseParameter *SelectModelMutation();
	const vector<BaseParameter *> *GetMutableParameters(){return &paramsToMutate;}
	int PerformModelMutation();
	void CreateModelFromSpecification(int);
	static void SetCode(GeneticCode *c){
		Model::code = c;
		FillQMatLookup();
		}
	const ModelSpecification *GetCorrespondingSpec() const {return modSpec;}
		
	private:
	void AllocateEigenVariables();
	void CalcEigenStuff();

	public:
	void CalcPmat(MODEL_FLOAT blen, MODEL_FLOAT *metaPmat, bool flip =false);
	void CalcPmats(FLOAT_TYPE blen1, FLOAT_TYPE blen2, FLOAT_TYPE *&mat1, FLOAT_TYPE *&mat2);
	void CalcPmatNState(FLOAT_TYPE blen, MODEL_FLOAT *metaPmat);
	void CalcDerivatives(FLOAT_TYPE, FLOAT_TYPE ***&, FLOAT_TYPE ***&, FLOAT_TYPE ***&);
	void OutputPmats(ofstream &deb);
	void AltCalcPmat(FLOAT_TYPE dlen, MODEL_FLOAT ***&pr);
	void UpdateQMat();
	void UpdateQMatCodon();
	void UpdateQMatAminoAcid();
	void UpdateQMatNState();
	void DiscreteGamma(FLOAT_TYPE *, FLOAT_TYPE *, FLOAT_TYPE);
	bool IsModelEqual(const Model *other) const ;	
	void CopyModel(const Model *from);
	void CopyEigenVariables(const Model *from);
	void SetModel(FLOAT_TYPE *model_string);
	void OutputPaupBlockForModel(ofstream &, const char *) const;
	void FillPaupBlockStringForModel(string &str, const char *treefname) const;
	void OutputGarliFormattedModel(ostream &) const;
	void FillGarliFormattedModelString(string &s) const;
	void OutputBinaryFormattedModel(OUTPUT_CLASS &) const;
	void ReadGarliFormattedModelString(string &);
	void OutputHumanReadableModelReportWithParams() const;
	void FillModelOrHeaderStringForTable(string &s, bool m) const;

	void ReadBinaryFormattedModel(FILE *);
	static void FillQMatLookup();
	void SetJonesAAFreqs();
	void SetMtMamAAFreqs();
	void SetMtRevAAFreqs();
	void SetDayhoffAAFreqs();
	void SetWAGAAFreqs();
	void MultiplyByJonesAAMatrix();
	void MultiplyByMtMamAAMatrix();
	void MultiplyByMtRevAAMatrix();
	void MultiplyByDayhoffAAMatrix();
	void MultiplyByWAGAAMatrix();

	//model mutations
	void MutateRates();
	void MutatePis();
	void MutateAlpha();
	void MutatePropInvar();
	void MutateRateProbs();
	void MutateRateMults();
	
	//Accessor functions
	FLOAT_TYPE StateFreq(int p) const{ return *stateFreqs[p];}
	FLOAT_TYPE TRatio() const;
	FLOAT_TYPE Rates(int r) const { return *relNucRates[r];}
	int NumRelRates() const {return relNucRates.size();}
	int NRateCats() const {return nRateCats;}
	FLOAT_TYPE *GetRateMults() {return rateMults;}
	FLOAT_TYPE Alpha() const {return *alpha;}
	FLOAT_TYPE PropInvar() const { return *propInvar;}
	bool NoPinvInModel() const { return ! (modSpec->includeInvariantSites);}
	FLOAT_TYPE MaxPinv() const{return maxPropInvar;}
	int NStates() const {return nstates;}
	int NumMutatableParams() const {return (int) paramsToMutate.size();}
	int Nst() const {return nst;}
	const int *GetArbitraryRateMatrixIndeces() const {return arbitraryMatrixIndeces;}
	bool IsNucleotide() {return modSpec->IsNucleotide();}
	bool IsNState() {return modSpec->IsNState();}
	bool IsNStateV() {return modSpec->IsNStateV();}

	//Setting things
	void SetDefaultModelParameters(const SequenceData *data);
	void SetRmat(FLOAT_TYPE *r, bool checkValidity){
		assert(nstates != 20);
		if(checkValidity == true){
			if(nst==1){
				if((FloatingPointEquals(r[0], r[1], 1.0e-5) &&
					FloatingPointEquals(r[1], r[2], 1.0e-5) &&
					FloatingPointEquals(r[2], r[3], 1.0e-5) &&
					FloatingPointEquals(r[3], r[4], 1.0e-5) &&
					FloatingPointEquals(r[4], r[5], 1.0e-5)) == false)
					throw(ErrorException("Config file specifies ratematrix = 1rate, but starting model has nonequal rates!\n"));
				}
			else if(nst==2){
				if((FloatingPointEquals(r[0], r[2], 1.0e-5) &&
					FloatingPointEquals(r[2], r[3], 1.0e-5) &&
					FloatingPointEquals(r[1], r[4], 1.0e-5) &&
					FloatingPointEquals(r[3], r[5], 1.0e-5)) == false)
					throw(ErrorException("Config file specifies ratematrix = 2rate, but starting model parameters do not match!\n"));
				}
			else if(nst==6 && modSpec->IsArbitraryRateMatrix()){
				for(int rate1=0;rate1<6-1;rate1++){
					for(int rate2=rate1+1;rate2<6;rate2++){
						if(arbitraryMatrixIndeces[rate1] == arbitraryMatrixIndeces[rate2]){
							if(!FloatingPointEquals(r[rate1], r[rate2], 1e-8))
								throw(ErrorException("Provided relative rate parameters don't obey the ratematix specification!\n\tGiven this spec: %s, rates %d and %d should be equal.\n", modSpec->arbitraryRateMatrixString.c_str(), rate1+1, rate2+1));
							}
						}
					}
				}
			}
		if(FloatingPointEquals(r[5], ONE_POINT_ZERO, 1.0e-5) == false){
			//if an alternate GTR paramterization is used in which GT != 1.0, rescale the rates
			for(int i=0;i<5;i++)
				r[i] /= r[5];
			}
		for(int i=0;i<5;i++) *relNucRates[i]=r[i];
		*relNucRates[5]=1.0;
		eigenDirty=true;
		}
	void SetPis(FLOAT_TYPE *b, bool checkValidity){
		//7/12/07 we'll now assume that all freqs have been passed in, rather than calcing the last
		//from the others
		if(checkValidity == true){
//			if(modSpec.IsNucleotide()){
				if(modSpec->IsEqualStateFrequencies() && (FloatingPointEquals(b[0], b[1], 1.0e-5) && FloatingPointEquals(b[1], b[2], 1.0e-5)) == false) 
					throw(ErrorException("Config file specifies equal statefrequencies,\nbut starting model has nonequal frequencies!\n"));
				if(modSpec->IsEmpiricalStateFrequencies()) 
					throw(ErrorException("Config file specifies empirical statefrequencies,\nbut starting model contains frequencies!\nTry statefrequencies = fixed or statefrequencies = estimate."));
				if(modSpec->IsPrecaledAAFreqs())
					throw(ErrorException("Config file specifies \"named\" amino acid statefrequencies,\nbut starting model contains frequencies!\nTry statefrequencies = fixed or statefrequencies = estimate."));
//				}
			}
		FLOAT_TYPE freqTot = 0.0;
		for(int i=0;i<nstates;i++){
			*stateFreqs[i]=b[i];
			freqTot += *stateFreqs[i];
			}
		if(FloatingPointEquals(freqTot, ONE_POINT_ZERO, 1.0e-5) == false)
			throw(ErrorException("State frequencies do not appear to add up to 1.0!\n"));
		eigenDirty=true;
		}

	void SetFlexRates(FLOAT_TYPE *rates, FLOAT_TYPE *probs){
		if(modSpec->IsFlexRateHet() == false) throw ErrorException("Flex rate values specified in start file,\nbut ratehetmodel is not flex in conf file.");
		for(int r=0;r<NRateCats();r++){
			rateMults[r]=rates[r];
			if(FloatingPointEquals(rateMults[r], ZERO_POINT_ZERO, 1e-8)){
				outman.UserMessage("WARNING: Flex rate multipliers cannot be zero. Rate %d changed from zero to 1.0e-5", r);
				rateMults[r] = 1.0e-5;
				}
			rateProbs[r]=probs[r];
			if(FloatingPointEquals(rateProbs[r], ZERO_POINT_ZERO, 1e-8)){
				throw ErrorException("Flex rate proportion %d cannot be zero.", r);
				}
			}
		FLOAT_TYPE tot = ZERO_POINT_ZERO;
		for(int r=0;r<NRateCats();r++)
			tot += rateProbs[r];
		if(!FloatingPointEquals(tot, ONE_POINT_ZERO, 1e-8))
			throw ErrorException("Specified Flex rate proportions don't add to 1.0!\n\tCorrect spec. is f rate1 prop1 rate2 prop2, etc.");
		}

	FLOAT_TYPE FlexRate(int which){
		assert(which < NRateCats());
		return rateMults[which];
		}

	FLOAT_TYPE FlexProb(int which){
		assert(which < NRateCats());
		return rateProbs[which];
		}

	//These are the set parameter functions used in the generic OptimizeBoundedParameter function
	//They need to have a standardized form, despite the fact that the "which" argument is unneccesary
	//for some of them
	void SetEquilibriumFreq(int which, FLOAT_TYPE val){

		assert(which < this->nstates);
#ifdef OLD_EQ_RESCALE
		FLOAT_TYPE rescale = (FLOAT_TYPE)((1.0 - val)/(1.0 - *stateFreqs[which]));
		for(int b=0;b<nstates;b++)
			if(b!=which) *stateFreqs[b] *= rescale;
		*stateFreqs[which] = val;

#else

		*stateFreqs[which] = val;
		NormalizeSumConstrainedValues(&stateFreqs[0], nstates, 1.0, 1e-4, which);
#endif
		eigenDirty = true;
		}

	void SetRelativeNucRate(int which, FLOAT_TYPE val){
		//this has the potential to do GT (fixed at 1.0) although that won't work with
		//OptBounded currently
		//DEBUG - Allow estimated AA matrices
		//assert(which < 6);
		//note that for arbitrary rate matrices mutation
		//of a rate other than GT might actually alter GT, so we need to actually check
		//whether it is 1.0 or not
		*relNucRates[which] = val;
		int refRate = NumRelRates() - 1;
		if(FloatingPointEquals(*relNucRates[refRate], ONE_POINT_ZERO, 1.0e-12) == false){
			FLOAT_TYPE scaler = ONE_POINT_ZERO / *relNucRates[refRate];
			for(int i=0;i<NumRelRates();i++){
				if(relNucRates[i] != relNucRates[refRate]){//this is checking whether the rate params are aliased to one another
					*relNucRates[i] *= scaler;
					}
				}
			*relNucRates[refRate] *= scaler;
			}
		eigenDirty = true;
		}

	void NormalizeSumConstrainedValues(FLOAT_TYPE **vals, int numVals, FLOAT_TYPE targetSum, FLOAT_TYPE minVal, int toNotChange){
		bool someMin = false;
		
		//CheckStatefreqBounds();
		FLOAT_TYPE minSum = ZERO_POINT_ZERO;
		FLOAT_TYPE sum = ZERO_POINT_ZERO;
		//note that sum here is the sum of everything besides toNotChange
		for(int i=0;i<numVals;i++){
			if(i != toNotChange){
				sum += *vals[i];
				if(*vals[i] < minVal)
					someMin = true;
				}
			}
		//so if there is any toNotChange this will be true
		if(someMin || !(FloatingPointEquals(sum, targetSum, minVal * 0.01))){
			do{
				FLOAT_TYPE unfixedTarget = targetSum - (toNotChange < 0 ? ZERO_POINT_ZERO : *vals[toNotChange]) - minSum;
				FLOAT_TYPE rescale = unfixedTarget / sum;
				someMin = false;
				sum = ZERO_POINT_ZERO;
				for(int i=0;i<numVals;i++){
					if(i != toNotChange){
						if(*vals[i] > ZERO_POINT_ZERO){
							*vals[i] *= rescale;
							if(*vals[i] < minVal){
								*vals[i] = -1.0;
								minSum += minVal;
								someMin = true;
								}
							else
								sum += *vals[i];
							}
						}
					}
				}while(someMin);
			}

		for(int i=0;i<numVals;i++)
			if(*vals[i] < ZERO_POINT_ZERO)
				*vals[i]= minVal;

#ifndef NDEBUG
		//CheckStatefreqBounds();
		sum = ZERO_POINT_ZERO;
		for(int i=0;i<numVals;i++){
			sum += *vals[i];
			}
		assert(FloatingPointEquals(sum, targetSum, minVal * 0.1));
#endif
		}

	void SetPinv(int which, FLOAT_TYPE val){
		assert(which == 0);
		*propInvar=val;
		//change the proportion of rates in each gamma cat
		for(int i=0;i<NRateCats();i++){
			rateProbs[i]=(FLOAT_TYPE)(1.0-*propInvar)/NRateCats();
			}
		}

	void SetAlpha(int which, FLOAT_TYPE val){
		assert(modSpec->numRateCats > 1);
		*alpha=val;
		DiscreteGamma(rateMults, rateProbs, *alpha);
		//This is odd, but we need to call normalize rates here if we are just using a gamma distrib to get starting rates for 
		//flex.  Flex expects that the rates will be normalized including pinv elsewhere
		if(modSpec->IsFlexRateHet()) NormalizeRates();
		}

	void SetFlexRate(int which, FLOAT_TYPE val){
		assert(which < NRateCats());
		rateMults[which] = val;
		NormalizeRates(which);
		eigenDirty = true;
		}

	void SetFlexProb(int which, FLOAT_TYPE val){
		assert(which < NRateCats());
		rateProbs[which] = val;
		NormalizeRates(which);
		eigenDirty = true;
		}

	void SetOmega(int which, FLOAT_TYPE val){
		assert(which < NRateCats());
		*omegas[which] = val;
		eigenDirty = true;
		}

	void SetOmegaProb(int which, FLOAT_TYPE val){
		assert(which < NRateCats());
		*omegaProbs[which] = val;

		FLOAT_TYPE newTot = 1.0 - *omegaProbs[which];
		FLOAT_TYPE oldTot = 0.0;
		for(int i=0;i<NRateCats();i++)
			if(i != which) oldTot += *omegaProbs[i];
		for(int i=0;i<NRateCats();i++)
			if(i != which) *omegaProbs[i] *= newTot / oldTot;
		newTot = 0.0;
		for(int i=0;i<NRateCats();i++) newTot += *omegaProbs[i];
		assert(FloatingPointEquals(newTot, ONE_POINT_ZERO, 1.0e-5));
		eigenDirty = true;
		}

	void SetOmegas(const FLOAT_TYPE *rates, const FLOAT_TYPE *probs){
		FLOAT_TYPE tot=0.0;
		for(int r=0;r<NRateCats();r++){
			if(FloatingPointEquals(rates[r], ZERO_POINT_ZERO, 1e-8)){
				outman.UserMessage("WARNING: Omega parameter %d cannot be zero.  Setting to 1e-5", r);
				*omegas[r] = 1.0e-5;
				}
			else *omegas[r]=rates[r];
			*omegaProbs[r]=probs[r];
			tot += *omegaProbs[r];
			}
		if(FloatingPointEquals(tot, ONE_POINT_ZERO, 1.0e-3) == false) throw ErrorException("omega category proportions add up to %f, not 1.0.", tot);
		eigenDirty = true;
		}

	FLOAT_TYPE Omega(int which) const{
		assert(which < NRateCats());
		return *omegas[which];
		}

	FLOAT_TYPE OmegaProb(int which) const{
		assert(which < NRateCats());
		return *omegaProbs[which];
		}

	void SetAlpha(FLOAT_TYPE a, bool checkValidity){
		if(checkValidity == true)
			if(modSpec->numRateCats==1) throw(ErrorException("Config file specifies ratehetmodel = none, but starting model contains alpha!\n"));
		*alpha=a;
		DiscreteGamma(rateMults, rateProbs, *alpha);
		//This is odd, but we need to call normalize rates here if we are just using a gamma distrib to get starting rates for 
		//flex.  Flex expects that the rates will be normalized including pinv elsewhere
		if(modSpec->IsFlexRateHet()) NormalizeRates();
		}

	void SetPinv(FLOAT_TYPE p, bool checkValidity){
		if(checkValidity == true)
			if(modSpec->includeInvariantSites==false && p!=0.0) throw(ErrorException("Config file specifies invariantsites = none, but starting model contains it!\n"));
		*propInvar=p;
		//change the proportion of rates in each gamma cat
		for(int i=0;i<NRateCats();i++){
			rateProbs[i]=(FLOAT_TYPE)(1.0-*propInvar)/NRateCats();
			}
		}
	void SetMaxPinv(FLOAT_TYPE p){
		maxPropInvar=p;
		}
	void SetDirty(bool tf){
		if(tf) eigenDirty=true;
		else eigenDirty=false;
		}

	void CheckAndCorrectRateOrdering(){
		assert(NRateCats() > 1);
		if(modSpec->IsNonsynonymousRateHet()){
			for(int f=0;f<NRateCats()-1;f++){
				if(*omegas[f] > *omegas[f+1]){
					//outman.UserMessage("prevented: %f %f", *omegas[f], *omegas[f+1]); 
					FLOAT_TYPE dum = *omegas[f+1];
					*omegas[f+1] = *omegas[f];
					*omegas[f] = dum;
					dum = *omegaProbs[f+1];
					*omegaProbs[f+1] = *omegaProbs[f];
					*omegaProbs[f] = dum;
					}
				}
			}
		else if(modSpec->IsFlexRateHet()){
			for(int f=0;f<NRateCats()-1;f++){
				if(rateMults[f] > rateMults[f+1]){
					FLOAT_TYPE dum = rateMults[f+1];
					rateMults[f+1] = rateMults[f];
					rateMults[f] = dum;
					dum = rateProbs[f+1];
					rateProbs[f+1] = rateProbs[f];
					rateProbs[f] = dum;
					}
				}
			}
		else assert(0);
		}

	void AdjustRateProportions(){
		//this will change the gamma class probs when pinv changes
		for(int i=0;i<NRateCats();i++) rateProbs[i]=(FLOAT_TYPE)(1.0-*propInvar)/NRateCats();
#ifndef NDEBUG
		FLOAT_TYPE sum=0.0;
		for(int i=0;i<NRateCats();i++){
			sum += rateProbs[i];
			}		
		sum += *propInvar;
		assert(FloatingPointEquals(sum, ONE_POINT_ZERO, 1.0e-5));
#endif
		}

	void NormalizeRates(int toRemainConstant = -1){
		//optionally, pass the number of one of the rate/prob pairs to hold constant

		FLOAT_TYPE sum=0.0;

		for(int i=0;i<NRateCats();i++){
			if(i != toRemainConstant) sum += rateProbs[i];
			}

		//pinv is figured into the normalization here, but it won't be changed itself.
		if(NoPinvInModel()==false){
			sum = sum / (FLOAT_TYPE)(1.0-*propInvar);
			}

		if(toRemainConstant > -1) sum /= (ONE_POINT_ZERO - rateProbs[toRemainConstant]);
		for(int i=0;i<NRateCats();i++)	{
			if(i != toRemainConstant) rateProbs[i] /= sum;
			}

		sum=0.0;
		
		double toRemainConstantContrib;
		if(toRemainConstant > -1){
			toRemainConstantContrib = rateMults[toRemainConstant]*rateProbs[toRemainConstant];
			//this means that it isn't possible to rescale and keep one of the rate/probs constant
			if(toRemainConstantContrib > ONE_POINT_ZERO)
				toRemainConstant = -1;
			}
			
		for(int i=0;i<NRateCats();i++){
			if(i != toRemainConstant) sum += rateMults[i]*rateProbs[i];
			}
		if(toRemainConstant > -1) sum /= (ONE_POINT_ZERO - (rateMults[toRemainConstant] * rateProbs[toRemainConstant]));
		for(int i=0;i<NRateCats();i++){
			if(i != toRemainConstant) rateMults[i] /= sum;
			}

#ifndef NDEBUG
		sum=0.0;
		for(int i=0;i<NRateCats();i++){
			sum += rateProbs[i];
			assert(rateProbs[i] > ZERO_POINT_ZERO);
			assert(rateProbs[i] < ONE_POINT_ZERO);
			assert(rateMults[i] > ZERO_POINT_ZERO);
			}		
		sum += *propInvar;
		assert(fabs(sum - 1.0) < 0.0001);
		sum=0.0;
		for(int i=0;i<NRateCats();i++){
			sum += rateMults[i]*rateProbs[i];
			}
		assert(FloatingPointEquals(sum, 1.0, 1.0e-5));

#endif
		}

	const FLOAT_TYPE *GetRateProbs() {
		//this is silly, but use the rateProbs as a holder to return the omegaProbs, which are in a vector of double pointers
		if(modSpec->IsNonsynonymousRateHet())
			for(int i=0;i<NRateCats();i++)
				rateProbs[i] = *omegaProbs[i];

#ifndef NDEBUG
		FLOAT_TYPE sum=0.0;
		for(int i=0;i<NRateCats();i++){
			sum += rateProbs[i];
			}
		sum+=*propInvar;
		assert(fabs(1.0-sum) < .001);
#endif		
		return rateProbs;
		}
	};

class ModelSet{//this is a set of models that are applied to a _single_ set of sites
				//i.e., model mixtures, although only ones in which the models have separate
				//Q matrices, eigen variables, etc (not just rates, so gamma and flex rates don't count)
	friend class Model;
	
	vector<Model*> mods;
	vector<FLOAT_TYPE> modelProbs;
	vector<FLOAT_TYPE> modelMutationProbs;
public:
	ModelSet(int m){
		//currently one model per set
		int numModels = 1;
		for(int i=0;i<numModels;i++){
			Model *mod = new Model(m);
			mods.push_back(mod);
			}
		}
	~ModelSet(){
		for(int i=0;i<mods.size();i++)
			delete mods[i];
		mods.clear();
		}
	ModelSet(const ModelSet &m){
		CopyModelSet(&m);
		}
	Model *GetModel(int m) const{
		if(m < 0 || (m < mods.size()) == false) throw ErrorException("Attemped to access invalid Model number");
		return mods[m];
		}
	unsigned NumModels(){return mods.size();}
	void CopyModelSet(const ModelSet *m){
		assert(GetModel(0)->stateFreqs[0] != NULL);
		unsigned num = 0;
		for(vector<Model*>::const_iterator modit = m->mods.begin();modit != m->mods.end();modit++){
			Model *mod;
			if(num >= mods.size()){
				mod = new Model(num);
				mods.push_back(mod);
				}
			else mod = mods[num];
			mod->CopyModel(*modit);
			num++;
			}
		}
	bool IsModelSetEqual(const ModelSet *other) const{
		bool equal = true;
		for(unsigned i=0;i<mods.size();i++){
			equal = mods[i]->IsModelEqual(other->GetModel(i));
			if(!equal) return equal;
			}
		return equal;
		}

	void CollectMutableParameters(vector<BaseParameter *> &paramVec){
		for(vector<Model*>::const_iterator modit = mods.begin();modit != mods.end();modit++){
			const vector<BaseParameter *> *tempVec = (*modit)->GetMutableParameters();
			for(unsigned i=0;i<tempVec->size();i++)
				paramVec.push_back((*tempVec)[i]);
			}
		}
	};

class ModelPartition{
	//a collection of model sets, each of which corresponds to a set of characters

	vector<ModelSet *> modSets;
	//the models here are the same as included in the modSets - it will sometimes
	//be handy to access them directly, and it will often be not matter what the 
	//hierarchy is to calculate things as the model level (ie, pmats, CLAs etc)
	vector<Model *> models;
	vector<BaseParameter *> allParamsToMutate;
//	FLOAT_TYPE globalRateScaler;

	vector<FLOAT_TYPE> subsetRates;
	vector<FLOAT_TYPE> subsetProportions;

public:
	ModelPartition();
	~ModelPartition(){
		for(int i = 0;i < modSets.size();i++)
			delete modSets[i];
		modSets.clear();
		//these are just pulled from the modsets, so don't need to be deleted
		models.clear();
		allParamsToMutate.clear();
		}

	void CopyModelPartition(const ModelPartition *mp){
		unsigned num = 0;
		for(vector<ModelSet*>::const_iterator setit = mp->modSets.begin();setit != mp->modSets.end();setit++){
			ModelSet *modSet;
			if(num >= modSets.size()){
				modSet = new ModelSet(num);
				modSets.push_back(modSet);
				}
			else modSet = modSets[num];
			modSet->CopyModelSet(*setit);
			num++;
			}
		//subsetProportions are just proportional to the number of total chars in each data subset, so they won't vary
		assert(NumSubsetRates() == mp->NumSubsetRates());
		for(int d = 0;d < subsetRates.size();d++){
			subsetRates[d] = mp->subsetRates[d];
			}
		}
	unsigned NumModelSets()const {return modSets.size();}
	unsigned NumModels()const {return models.size();}
	unsigned NumMutableParams() const {return allParamsToMutate.size();}
	unsigned NumSubsetRates() const {return subsetRates.size();}
	FLOAT_TYPE SubsetRate(int i){return subsetRates[i];}
	//can't think of anything else that really needs to get reset here
	void Reset(){
		for(int d = 0;d < subsetRates.size();d++){
			subsetRates[d] = 1.0;
			}
		}
	void SetSubsetRate(int which, FLOAT_TYPE val){
		assert(which < subsetRates.size());
		subsetRates[which] = val;
		NormalizeSubsetRates(which);
		}
	void SetSubsetRates(const vector<FLOAT_TYPE> vals){
		assert(NumSubsetRates() == vals.size());
		subsetRates.clear();
		for(int i = 0;i < vals.size();i++)
			subsetRates.push_back(vals[i]);
		}

	int PerformModelMutation();
	BaseParameter *SelectModelMutation();
	void CalcMutationProbsFromWeights();
	unsigned CalcRequiredCLAsize(const DataPartition *dat); //this is the size in BYTES not elements

	const ModelSet *GetModelSet(int ms) const{
		if(ms < 0 || ms < modSets.size() == false) throw ErrorException("Attemped to access invalid ModelSet number");
		return modSets[ms];
		}
	Model *GetModel(int m) const{
		if(m < 0 || (m < models.size()) == false) 
			throw ErrorException("Attemped to access invalid Model number");
		return models[m];
		}
	bool IsModelPartitionEqual(const ModelPartition *other) const{
		bool equal = true;
		for(unsigned i=0;i<modSets.size();i++){
			equal = modSets[i]->IsModelSetEqual(other->GetModelSet(i));
			if(!equal) return equal;
			}
		return equal;
		}
	void CollectMutableParameters(){
		for(vector<ModelSet*>::const_iterator setit = modSets.begin();setit != modSets.end();setit++){
			vector<BaseParameter *> setParams;
			(*setit)->CollectMutableParameters(setParams);
			for(vector<BaseParameter *>::iterator pit = setParams.begin();pit != setParams.end();pit++)
				allParamsToMutate.push_back(*pit);
			setParams.clear();
			}
		}
	void NormalizeSubsetRates(int toRemainConstant = -1){
		//optionally, pass the number of one of the rates to hold constant

		//the proportions don't change, so this is simpler than flex rates
		assert(subsetRates.size() > 1);

		double toRemainConstantContrib;
		if(toRemainConstant > -1){
			toRemainConstantContrib = subsetRates[toRemainConstant]*subsetProportions[toRemainConstant];
			//this means that it isn't possible to rescale and keep one of the rate/probs constant
			if(toRemainConstantContrib > ONE_POINT_ZERO)
				toRemainConstant = -1;
			}
		FLOAT_TYPE sum = 0.0;
		for(int i=0;i<subsetRates.size();i++){
			if(i != toRemainConstant) sum += subsetRates[i]*subsetProportions[i];
			}
		if(toRemainConstant > -1) sum /= (ONE_POINT_ZERO - (subsetRates[toRemainConstant] * subsetProportions[toRemainConstant]));
		for(int i=0;i<subsetRates.size();i++){
			if(i != toRemainConstant) subsetRates[i] /= sum;
			}

#ifndef NDEBUG
		sum = 0.0;
		for(int i=0;i<subsetRates.size();i++){
			sum += subsetProportions[i] * subsetRates[i];
			}
		assert(FloatingPointEquals(sum, 1.0, 1e-6));
#endif
		}
	void OutputHumanReadableModelReportWithParams(){
		for(int m = 0;m < NumModels();m++){
			outman.UserMessage("Model %d", m);
			models[m]->OutputHumanReadableModelReportWithParams();
			}
		if(modSpecSet.InferSubsetRates()){
			outman.UserMessageNoCR("Subset rate multipliers:\n  ");
			for(int d = 0;d < subsetRates.size();d++)
				outman.UserMessageNoCR("%6.2f", SubsetRate(d));
			outman.UserMessage("");
			}
		}

	};

typedef void (Model::*SetParamFunc) (int, FLOAT_TYPE);
#define CALL_SET_PARAM_FUNCTION(object, ptrToMember) ((object).*(ptrToMember))

#endif

