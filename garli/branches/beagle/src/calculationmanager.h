// GARLI version 1.00 source code
// Copyright 2005-2010 Derrick J. Zwickl
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

#ifndef CALCULATION_MANAGER
#define CALCULATION_MANAGER

#ifdef USE_BEAGLE
#include "beagle.h"
#endif

//extern ClaManager claMan;
//owned by each TreeNode
#include "utility.h"
#include "model.h"
class TreeNode;

#include "managedresource.h"

struct ScoreSet{
	MODEL_FLOAT lnL;
	MODEL_FLOAT d1;
	MODEL_FLOAT d2;
	};

#define GARLI_FINAL_SCALER_INDEX -9999999


class ClaOperation{
	friend class CalculationManager;
public:
	int destClaIndex;
	int childClaIndex1;
	int childClaIndex2;
	int transMatIndex1;
	int transMatIndex2;
	int opDepLevel;
public:
	ClaOperation(): destClaIndex(-1), childClaIndex1(-1), childClaIndex2(-1), transMatIndex1(-1), transMatIndex2(-1), opDepLevel(-1){};	
	ClaOperation(int d, int cla1, int cla2, int pmat1, int pmat2, int dLevel): destClaIndex(d), childClaIndex1(cla1), childClaIndex2(cla2), transMatIndex1(pmat1), transMatIndex2(pmat2), opDepLevel(dLevel){
		int poo = 2;	
		}
	ClaOperation(int d, const CondLikeArrayHolder *h): destClaIndex(d), childClaIndex1(h->HolderDep1()), childClaIndex2(h->HolderDep2()), transMatIndex1(h->TransMatDep1()), transMatIndex2(h->TransMatDep2()), opDepLevel(h->DepLevel()){};	
	};

class TransMatOperation{
	friend class CalculationManager;
	//friend class NodeOperation;
	friend class BranchOperation;

public:
	int destTransMatIndex;
	FLOAT_TYPE edgeLength;
	int modelIndex; // ~eigen solution
	bool calcDerivs;

	TransMatOperation() : destTransMatIndex(-1), modelIndex(-1), edgeLength(-1.0), calcDerivs(false){};
	TransMatOperation(int ind, int modInd, FLOAT_TYPE len, bool d) : destTransMatIndex(ind), modelIndex(modInd), edgeLength(len), calcDerivs(d){};
	};

class NodeOperation{
	friend class CalculationManager;
	ClaOperation claOp;
	TransMatOperation transOp1;
	TransMatOperation transOp2;
public:
	NodeOperation();
	NodeOperation(const NodeOperation &from){
		claOp = from.claOp;
		transOp1 = from.transOp1;
		transOp2 = from.transOp2;
		}
	NodeOperation(const ClaOperation &c, const TransMatOperation &t1, const TransMatOperation &t2){
		claOp = c;
		transOp1 = t1;
		transOp2 = t2;
		}
	bool operator <(const NodeOperation &rhs){
		return claOp.opDepLevel < rhs.claOp.opDepLevel;
		}
	};

class ScoringOperation{
	friend class CalculationManager;
	friend class BranchOperation;

	//my functions don't write the final cla, but Beagle allows for specifying one
	int destClaIndex;
	int childClaIndex1;
	int childClaIndex2;
	int transMatIndex;
	bool derivatives;
public:
	ScoringOperation() : destClaIndex(-1), childClaIndex1(-1), childClaIndex2(-1), transMatIndex(-1), derivatives(false){};
	ScoringOperation(int dest, int child1, int child2, int pmat, bool d) : destClaIndex(dest), childClaIndex1(child1), childClaIndex2(child2), transMatIndex(pmat), derivatives(d){};
	};

class BranchOperation{
	friend class CalculationManager;
	ScoringOperation scrOp;
	TransMatOperation transOp;
public:
	BranchOperation(const BranchOperation &from){
		scrOp = from.scrOp;
		transOp = from.transOp;
		}
	};

class BlockingOperationsSet{
public:
	int opSetDepLevel;
	list<ClaOperation> claOps;
	list<TransMatOperation> pmatOps;
	};

/*this is a generic matrix for use as a pmat or derivative matrix
multiple matrices for rates or whatever appear one after another*/
class TransMat{
	friend class TransMatSet;
	int nRates;
	int nStates;
	MODEL_FLOAT ***theMat;
public:

	TransMat(): theMat(NULL), nRates(-1), nStates(-1){}
	void Allocate(int numStates, int numRates){
		nStates = numStates;
		nRates = numRates;
		theMat = New3DArray<MODEL_FLOAT>(nRates, nStates, nStates);
		}
	~TransMat(){
		if(theMat)
			Delete3DArray<MODEL_FLOAT>(theMat);
		theMat = NULL;
		}
	MODEL_FLOAT ***GetMatrix(){
		return theMat;
		}
	};

/*package of pmat/d1mat/d2mat that will be assigned to a TransMatHolder (representing a branch)*/
class TransMatSet{
	friend class TransMatHolder;

	TransMat pmat;
	TransMat d1mat;
	TransMat d2mat;
	int index;
public:
	void Allocate(int i, int numStates, int numRates){
		index = i;
		pmat.Allocate(numStates, numRates);
		d1mat.Allocate(numStates, numRates);
		d2mat.Allocate(numStates, numRates);
		}
	MODEL_FLOAT ***GetPmatArray(){
		assert(pmat.theMat);
		return pmat.theMat;
		}
	MODEL_FLOAT ***GetD1Array(){
		assert(d1mat.theMat);
		return d1mat.theMat;
		}
	MODEL_FLOAT ***GetD2Array(){
		assert(d2mat.theMat);
		return d2mat.theMat;
		}
	int Index(){return index;}
	};

/*analogous to a ClaHolder.  When a valid transmat set exists theMatSet will
keep a pointer to it.  When dirty it will be NULL*/
class TransMatHolder{
	friend class PmatManager;
	//friend class NodeClaManager;
	//friend class CalculationManager;

	//reference count
	int numAssigned;

	//these are the things that the holder must know to calculate valid transmats for itself
	Model *myMod;
	MODEL_FLOAT edgeLen;

	//will be NULL if dirty
	TransMatSet *theMatSet;

	TransMatHolder() : theMatSet(NULL), myMod(NULL), edgeLen(-1.0), numAssigned(0){};
	~TransMatHolder() {
		myMod = NULL;
		theMatSet = NULL;
		}
	void Reset(){numAssigned = 0; theMatSet = NULL; myMod = NULL;}
	
	MODEL_FLOAT ***GetPmatArray(){
		assert(theMatSet);
		assert(theMatSet->GetPmatArray()[0] && theMatSet->GetPmatArray()[0][0]);
		return theMatSet->GetPmatArray();
		}

	MODEL_FLOAT ***GetD1Array(){
		assert(theMatSet);
		return theMatSet->GetD1Array();
		}

	MODEL_FLOAT ***GetD2Array(){
		assert(theMatSet);
		return theMatSet->GetD2Array();;
		}

	const Model *GetConstModel() const{
		return myMod;
		}
	};

class PmatManager{
	int nRates;
	int nStates;
	int nMats;
	int nHolders;
	int maxUsed;
	
	//these are the actual matrices to be used in calculations, but will assigned to 
	//nodes (branches) via a PmatHolder.  There may be a limited number
	TransMatSet **allMatSets;

	//there will be enough of these such that every branch of every tree could
	//have a unique one, although many will generally be shared
	TransMatHolder *holders; 
	
	//pointers to the matrix sets
	vector<TransMatSet*> transMatSetStack;
	
	//indeces of free holders
	vector<int> holderStack;
public:
	PmatManager(int numMats, int numHolders, int numRates, int numStates) : nMats(numMats), nHolders(numHolders), nRates(numRates), nStates(numStates){
		allMatSets = new TransMatSet* [nMats];
		transMatSetStack.reserve(nMats);
		
		for(int i = nMats - 1;i >= 0;i--){
			allMatSets[i] = new TransMatSet;
			allMatSets[i]->Allocate(i, nStates, nRates);
			transMatSetStack.push_back(allMatSets[i]);
			}

		holders = new TransMatHolder[nHolders];
		holderStack.reserve(nHolders);
		for(int i=nHolders-1;i>=0;i--)
			holderStack.push_back(i);
		}

	~PmatManager(){
		//transMatSetStack.clear();
		//holderStack.clear();
		if(allMatSets != NULL){
			for(int i = 0;i < nMats;i++){
				delete allMatSets[i];
				}
			delete []allMatSets;
			}
		delete []holders;
		}

	const int GetNumStates() const { return nStates; }
	const int GetNumRates() const { return nRates; }
	bool TransMatIsAssigned(int index) const { return holders[index].theMatSet != NULL; }
	bool IsHolderDirty(int index) const { return holders[index].theMatSet == NULL; }

	TransMatSet *GetTransMatSet(int index){
		if(holders[index].theMatSet == NULL)
			FillTransMatHolder(index);
		assert(holders[index].theMatSet != NULL);
		return holders[index].theMatSet;
		}

	void SetModel(int index, Model *mod){
		holders[index].myMod = mod;
		}

	void SetEdgelen(int index, FLOAT_TYPE e){
		holders[index].edgeLen = e;
		}

	const FLOAT_TYPE GetEdgelen(int index) const{
		return holders[index].edgeLen;
		}

	void GetEigenSolution(int index, ModelEigenSolution &sol) const{
		assert(holders[index].myMod);
		holders[index].myMod->GetEigenSolution(sol);
		}

//this includes pinv, which my scheme doesn't treat as a rate class per se
	void GetCategoryRatesForBeagle(int index, vector<FLOAT_TYPE> &r)const{
		holders[index].myMod->GetRateMultsForBeagle(r);
		}

//this includes pinv
	void GetCategoryWeightsForBeagle(int index, vector<FLOAT_TYPE> &p) const {
		holders[index].myMod->GetRateProbsForBeagle(p);
		}

	//need to include pinv, which is a separate rate as far as beagle is concerned, but not for Gar
	int NumRateCatsForBeagle(int index) const { 
		return holders[index].myMod->NumRateCatsForBeagle();
		}

	MODEL_FLOAT ***GetPmatArray(int index){
		assert(holders[index].numAssigned > 0);
		if(holders[index].theMatSet == NULL){
			FillTransMatHolder(index);
			//outman.DebugMessage("get, fill %d", index);
			}
		else{
			//outman.DebugMessage("get, %d already filled", index);
			}
		return holders[index].GetPmatArray();
		}
 
	MODEL_FLOAT ***GetD1MatArray(int index){
		assert(holders[index].numAssigned > 0);
		if(holders[index].theMatSet == NULL)
			FillTransMatHolder(index);
		return holders[index].GetD1Array();
		}

	MODEL_FLOAT ***GetD2MatArray(int index){
		assert(holders[index].numAssigned > 0);
		if(holders[index].theMatSet == NULL)
			FillTransMatHolder(index);
		return holders[index].GetD2Array();
		}

	void FillTransMatHolder(int index){
		//outman.DebugMessage("assign %d", index);
		assert(holders[index].theMatSet == NULL);
		holders[index].theMatSet = AssignFreeTransMatSet();
		}

	//This is essentially the same as GetTransmat, but doesn't return anything and also sets the tempReserved flag
	//FillHolder is only used if the holder is currently empty
	void ClaimTransmatsetFillIfNecessary(int index){
		//this is all that needs to happen here until transmat recycling is worked out
		if(IsHolderDirty(index)){
			FillTransMatHolder(index);
			}
/*		else{
			//FillHolder will also set the dep level, but we need to set it even if FillHolder doesn't need to be called
			SetDepLevel(index, depLevel);
			}
*/
		//mark this holder as one not to be reclaimed, since it has been "claimed"
		//TempReserveCla(index);
		}

	const TransMatHolder *GetTransMatHolder(int index) const{
		//outman.DebugMessage("get %d", index);
		return &holders[index];
		}

	TransMatHolder *GetMutableTransMatHolder(int index){
		//outman.DebugMessage("get mut %d", index);
		return &holders[index];
		}

	const Model *GetModelForTransMatHolder(int index)const{
		return holders[index].GetConstModel();
		}

	Model *GetMutableModelForTransMatHolder(int index)const{
		return holders[index].myMod;
		}

	void FillDerivativeMatrices(int index, double ***thePMat, double ***theD1Mat, double ***theD2Mat){
		TransMatHolder *hold = &holders[index];
		thePMat = hold->GetPmatArray();
		theD1Mat = hold->GetD1Array();
		theD2Mat = hold->GetD2Array();
		hold->myMod->FillDerivativeMatrices(hold->edgeLen, thePMat, theD1Mat, theD2Mat);
		}

	void ObtainAppropriateTransmats(const TransMatOperation &pit, double ***&thePMat, double ***&theD1Mat, double ***&theD2Mat){
		TransMatHolder *hold = &holders[pit.destTransMatIndex];
		thePMat = GetPmatArray(pit.destTransMatIndex);
		if(pit.calcDerivs){
			theD1Mat = GetD1MatArray(pit.destTransMatIndex);
			theD2Mat = GetD2MatArray(pit.destTransMatIndex);
			hold->myMod->FillDerivativeMatrices(hold->edgeLen, thePMat, theD1Mat, theD2Mat);
			}
		else{
			hold->myMod->AltCalcPmat(hold->edgeLen, thePMat);
			}
		}

	//this will be called by a node (branch) and will return a previously unused holder index
	int AssignTransMatHolder(){
		assert(holderStack.size() > 0);
		int index=holderStack[holderStack.size()-1];
		assert(holders[index].numAssigned == 0);
		IncrementTransMatHolder(index);
		holderStack.pop_back();
		return index;
		}

	TransMatSet* AssignFreeTransMatSet(){
		//TODO
		//DEBUG need to figure out how this will work with beagle, or if recycling is even necessary with transmats
//		if(transMatSetStack.empty() == true) 
//			RecycleTransMatSets();

		assert(! transMatSetStack.empty());
		TransMatSet *mat=transMatSetStack[transMatSetStack.size()-1];
		transMatSetStack.pop_back();
		if(nMats - (int)transMatSetStack.size() > maxUsed) 
			maxUsed = nMats - (int)transMatSetStack.size();
		return mat;
		}

	void IncrementTransMatHolder(int index){
		//outman.DebugMessage("inc %d to %d", index, holders[index].numAssigned + 1);
		holders[index].numAssigned++;
		}

	void DecrementTransMatHolder(int index){
		TransMatHolder *thisHold = &holders[index];

		assert(index != -1);
		assert(thisHold->numAssigned != 0);
		//outman.DebugMessage("dec %d to %d", index, holders[index].numAssigned - 1);
		if(thisHold->numAssigned == 1){
			//if the count has fallen to zero, reclaim the holder
			//outman.DebugMessage("reclaim1 %d", index);
			holderStack.push_back(index);
			if(holders[index].theMatSet != NULL){
				//if there is a valid matrix set in the holder, reclaim it too
				//outman.DebugMessage("reclaim1 set from %d", index);
	//			assert(find(transMatSetStack.begin(), transMatSetStack.end(), holders[index].theMatSet) == transMatSetStack.end());
				transMatSetStack.push_back(thisHold->theMatSet);
				}
			thisHold->Reset();
			}
		else{
			thisHold->numAssigned--; 
			//TODO
			//DEBUG - what happens with this and beagle
			//this is important! (this was the old message that was here)
			//holders[index].tempReserved=false;
			}
		}

	int SetTransMatDirty(int index){
		//there are only two options here:
		//1. transmatSet is being made dirty, and only node node points to it 
		//	->null the holder's transmatSet pointer and return the same index
		//2. transmatSet is being made dirty, and multiple nodes point to it
		//	->remove this transmatSet from the holder (decrement) and assign a new one	
		assert(index != -1);

		TransMatHolder *thisHold = &holders[index];
		//outman.DebugMessage("dirty %d", index);
		if(thisHold->numAssigned==1){
			if(thisHold->theMatSet != NULL){
				//TODO
				//holders[index].SetReclaimLevel(0);
				//outman.DebugMessage("reclaim2 %d", index);
				
				transMatSetStack.push_back(thisHold->theMatSet);
				thisHold->theMatSet=NULL;
				}
			}
		else{
			DecrementTransMatHolder(index);
			assert(holderStack.size() > 0);
			index=holderStack[holderStack.size()-1];
			holderStack.pop_back();
			IncrementTransMatHolder(index);
			}
		return index;
		}

	//This returns the pmat number of the cla that this holder points to 
	//FOR BEAGLE PURPOSES.  It sees three matrices for each mat set here, so
	//multiply accordingly
	int GetPmatIndexForBeagle(int index) const{
		assert(index > -1 && index < nHolders);
		assert(holders[index].theMatSet != NULL);
		return holders[index].theMatSet->Index() * 3;
		}
	
	int GetD1MatIndexForBeagle(int index) const{
		assert(index > -1 && index < nHolders);
		assert(holders[index].theMatSet != NULL);
		return holders[index].theMatSet->Index() * 3 + 1;
		}

	int GetD2MatIndexForBeagle(int index) const{
		assert(index > -1 && index < nHolders);
		assert(holders[index].theMatSet != NULL);
		return holders[index].theMatSet->Index() * 3 + 2;
		}
	};

class NodeClaManager{
	static ClaManager *claMan;
	static ClaManager2 *claMan2;
	static PmatManager *pmatMan;

public:
	//Indeces should always be valid after initial assignment
	//Dirtying is reassignment to a new ClaHolder index (initially containing no assigned CondlikeArray index)
	//or NULLing of the CondlikeArray pointer from the current ClaHolder if only one node refers to it
	int downHolderIndex;
	int ULHolderIndex;	
	int URHolderIndex;
	int transMatIndex;

	NodeClaManager() : downHolderIndex(-1), ULHolderIndex(-1), URHolderIndex(-1), transMatIndex(-1){};

	static void SetClaManager(ClaManager *cMan) {
		NodeClaManager::claMan = cMan;
		}

	static void SetClaManager2(ClaManager2 *cMan) {
		NodeClaManager::claMan2 = cMan;
		}

	static void SetPmatManager(PmatManager *pMan) {
		NodeClaManager::pmatMan = pMan;
		}

	//tips will have the cla holders all -1, but a valid transmat index
	bool IsAllocated() const {
		return !(downHolderIndex < 0 && ULHolderIndex < 0 && URHolderIndex < 0 && transMatIndex < 0);
		}

	void SetHolders(int d, int UL, int UR, int p){
		//this might be called to copy changes that were made to cla assignments NOT through the
		//NodeClaManager. So these can already be set to some other valid value
/*
		assert(downHolderIndex < 0);
		assert(ULHolderIndex < 0);
		assert(URHolderIndex < 0);
		assert(transMatIndex < 0);
*/
		downHolderIndex = d;
		ULHolderIndex = UL;	
		URHolderIndex = UR;
		transMatIndex = p;
		}

	void ObtainTransMatHolders(){
		assert(transMatIndex < 0);
		transMatIndex = pmatMan->AssignTransMatHolder();
		}

	void ObtainClaAndTransMatHolders(){
		assert(downHolderIndex < 0);
		assert(ULHolderIndex < 0);
		assert(URHolderIndex < 0);
		assert(transMatIndex < 0);
		downHolderIndex = claMan->AssignFreeClaHolder();
		ULHolderIndex = claMan->AssignFreeClaHolder();
		URHolderIndex = claMan->AssignFreeClaHolder();
		transMatIndex = pmatMan->AssignTransMatHolder();
		}

	void SetDirtyUpRight(){
		URHolderIndex = claMan->SetHolderDirty(URHolderIndex);
		}

	void SetDirtyUpLeft(){
		ULHolderIndex = claMan->SetHolderDirty(ULHolderIndex);
		}

	void SetDirtyDown(){
		downHolderIndex = claMan->SetHolderDirty(downHolderIndex);
		}

	void SetDirtyAll(){
		URHolderIndex = claMan->SetHolderDirty(URHolderIndex);
		ULHolderIndex = claMan->SetHolderDirty(ULHolderIndex);
		downHolderIndex = claMan->SetHolderDirty(downHolderIndex);
		SetTransMatDirty();
		}

	void SetTransMatDirty(){
		if(transMatIndex >= 0)
			transMatIndex = pmatMan->SetTransMatDirty(transMatIndex);
		}

	void SetDependenciesUL(int depIndex1, int pDepIndex1, int depIndex2, int pDepIndex2){
		claMan->SetHolderDependencies(ULHolderIndex, depIndex1, pDepIndex1, depIndex2, pDepIndex2);
		}

	void SetDependenciesUR(int depIndex1, int pDepIndex1, int depIndex2, int pDepIndex2){
		claMan->SetHolderDependencies(URHolderIndex, depIndex1, pDepIndex1, depIndex2, pDepIndex2);
		}

	void SetDependenciesDown(int depIndex1, int pDepIndex1, int depIndex2, int pDepIndex2){
		claMan->SetHolderDependencies(downHolderIndex, depIndex1, pDepIndex1, depIndex2, pDepIndex2);
		}

	void SetTransMat(Model *m, FLOAT_TYPE e){
		//outman.DebugMessage("up pdeps %d", transMatIndex);

		pmatMan->SetModel(transMatIndex, m);
		pmatMan->SetEdgelen(transMatIndex, e);
		}

	inline CondLikeArray *GetClaDown(){
		bool calc = true;
		if(claMan->IsHolderDirty(downHolderIndex)){
			if(calc==true){
				assert(0);
				//DEBUG - may have to nix this auto calc behavior when dirty with the new management system
				//Need to somehow calculate this here
				//ConditionalLikelihoodRateHet(DOWN, nd);
				}
			else claMan->FillHolder(downHolderIndex, 1);
			}

	//	if(memLevel > 1) claMan->ReserveCla(downClaIndex);
		return claMan->GetClaFillIfNecessary(downHolderIndex);
		}

	inline CondLikeArray *GetClaUpLeft(){
		bool calc = true;
		if(claMan->IsHolderDirty(ULHolderIndex)){
			if(calc==true){
				//DEBUG
				assert(0);
				//Need to somehow calculate this here
				//ConditionalLikelihoodRateHet(UPLEFT, nd);
				}
			else claMan->FillHolder(ULHolderIndex, 2);
			}

	//	if(memLevel > 0) claMan->ReserveCla(nd->claIndexUL);

		return claMan->GetClaFillIfNecessary(ULHolderIndex);
		}

	inline CondLikeArray *GetClaUpRight(){
		bool calc = true;
		if(claMan->IsHolderDirty(URHolderIndex)){
			if(calc==true){
				//DEBUG
				assert(0);
				//Need to somehow calculate this here
				//ConditionalLikelihoodRateHet(UPRIGHT, nd);
				}
			else claMan->FillHolder(ULHolderIndex, 2);
			}
	//	if(memLevel > 0) claMan->ReserveCla(nd->claIndexUR);
		return claMan->GetClaFillIfNecessary(ULHolderIndex);
		}

	inline TransMatSet *GetTransMatSet() const {
		return pmatMan->GetTransMatSet(transMatIndex);
		}

	void CopyHolderIndecesInternal(const NodeClaManager *from, bool remove){
		if(remove){
			StripHolders();
			}

		downHolderIndex = from->downHolderIndex;
		ULHolderIndex = from->ULHolderIndex;
		URHolderIndex = from->URHolderIndex;
		transMatIndex = from->transMatIndex;

		if(downHolderIndex >= 0){
			assert(claMan->NumAssigned(downHolderIndex) > 0);
			assert(claMan->NumAssigned(ULHolderIndex) > 0);
			assert(claMan->NumAssigned(URHolderIndex) > 0);
			claMan->IncrementHolder(downHolderIndex);
			claMan->IncrementHolder(ULHolderIndex);
			claMan->IncrementHolder(URHolderIndex);
			}
		if(transMatIndex >= 0)
			pmatMan->IncrementTransMatHolder(transMatIndex);
		}

	void CopyHolderIndecesTerminal(const NodeClaManager *from, bool remove){
		if(remove){
			StripHolders();
			}
		transMatIndex = from->transMatIndex;
		if(transMatIndex >= 0)
			pmatMan->IncrementTransMatHolder(transMatIndex);
		}

	void StripHolders(){
		if(downHolderIndex >= 0){
			claMan->DecrementHolder(downHolderIndex);
			claMan->DecrementHolder(ULHolderIndex);
			claMan->DecrementHolder(URHolderIndex);

			downHolderIndex = -1;
			ULHolderIndex = -1;
			URHolderIndex = -1;
			}
		else{
			assert(ULHolderIndex < 0 && ULHolderIndex < 0);
			}
		if(transMatIndex >= 0){
			pmatMan->DecrementTransMatHolder(transMatIndex);
			transMatIndex = -1;
			}
		}
	};

/*
class NewBlockingOperationsSet{
public:
	list<NodeOperation> nodeOps;
	list<BranchOperation> brOps;
	~NewBlockingOperationsSet(){
		nodeOps.clear();
		brOps.clear();
		}
	};
*/
/*
bool MyOpLessThan(const BlockingOperationsSet &lhs, const BlockingOperationsSet &rhs){
//	int d1 = lhs.claOps.size() == 0 ? -1 : lhs.claOps.begin()->depLevel;
//	int d2 = rhs.claOps.size() == 0 ? -1 : rhs.claOps.begin()->depLevel;
	if(lhs.claOps.size() == 0)
		return false;
	else if(rhs.claOps.size() == 0)
		return true;
	return 
		(lhs.claOps.begin()->depLevel < rhs.claOps.begin()->depLevel);
	}
*/
/*
This does all of the actual calculations and operations once they have been figured out at the Tree level.  It only understands about indeces
of CLA holders and pmat holders, and constructs the sets of operations to be calculated.

1. Tree funcs sweeps over the tree structure when necessary, using NodeClaManagers to dirty clas and set up the dependencies between different clas.
   Dirtying happens when blens change, topology changes or model changes (many clas get dirtied).  Topology changes don't change many dependencies
   directly, but dirtying may change the Holder indecies if multiple trees are pointing to it, and thereby change many dependencies.

2. Tree asks the CalcManager for either a lnL or lnL + derivs

3. CalcManager uses the holder dependencies to deduce what operations need to happen.

4. Operations are accumulated by the CalculationManager and carried out either by passing raw matrices and clas locally, or by passing 
   operations by indecies to Beagle.
*/

class CalculationManager{
	//the claManager may eventually be owned by calcManager, but for now it can interact indirectly
	//with the global manager, as everything did previously
	static ClaManager *claMan;
	static PmatManager *pmatMan;
	static const SequenceData *data;
	list<BlockingOperationsSet> operationSetQueue;
	list<BlockingOperationsSet> freeableOpSets;
	list<ScoringOperation> scoreOps;
	
//	list<NewBlockingOperationsSet> newOperationSetQueue;

	map<long, string> flagToName;
	map<string, long> nameToFlag;
	vector<long> invalidFlags;

	string requiredBeagleFlags;
	string preferredBeagleFlags;
	string actualBeagleFlags;

	long req_flags;
	long pref_flags;
	long actual_flags;

public:
	bool useBeagle;
	bool termOnBeagleError;
	int beagleDeviceNum;
	//bool rescaleBeagle;
//	bool singlePrecBeagle;
//	bool gpuBeagle;
	int beagleInst;
	string ofprefix;

	CalculationManager(){
		useBeagle = false; 
//		gpuBeagle = false;
		beagleInst = -1; 
		termOnBeagleError = true;
		//singlePrecBeagle = false;
		//rescaleBeagle = false;
		FillBeagleOptionsMaps();
		req_flags = pref_flags = actual_flags = 0;
		}

	void FillBeagleOptionsMaps();
/*
	void SetBeagleDetails(bool gpu, bool singlePrec, bool rescale, string &prefix){
		useBeagle = true;
		if(gpu){//assuming that all GPU is SP for now
			gpuBeagle = true;
			singlePrecBeagle = true;
			//DEBUG - should eventually force rescaling with gpu, but don't for debugging purposes
			//rescaleBeagle = true;
			}
		if(singlePrec){
			singlePrecBeagle = true;
			}
		if(rescale)
			rescaleBeagle = true;
		this->ofprefix = prefix;
		}
*/
	void SetBeagleDetails(string prefFlags, string reqFlags, int dnum, string &prefix){
		useBeagle = true;
		preferredBeagleFlags = prefFlags;
		requiredBeagleFlags = reqFlags;
		ofprefix = prefix;
		beagleDeviceNum = dnum;
		}

	long ParseBeagleFlagString(string flagsString, long flagMask = 0) const;
	void InterpretBeagleResourceFlags(long flags, string &list, long flagMask = 0) const;
	void OutputBeagleResources() const;

	//for now assuming that rescaling will always be available - this will need to be
	//called BEFORE initializing an instance because how the initialization is called depends on it
	bool IsRescaling() {return (bool) ((pref_flags | req_flags) & BEAGLE_FLAG_SCALERS_LOG);}
	//this will only know if it ended up being SP AFTER initialization
	bool IsSinglePrecision() {return (bool) (actual_flags & BEAGLE_FLAG_PRECISION_SINGLE);}
	//this is also only known AFTER initialization
	bool IsGPU() {return (bool) (actual_flags & BEAGLE_FLAG_PROCESSOR_GPU);}

	void InitializeBeagle(int nnod, int nClas, int nHolders, int nstates, int nchar, int nrates);

	void Finalize(){
#ifdef USE_BEAGLE
		if(beagleInst > 0)
			beagleFinalizeInstance(beagleInst);
#endif
		}

	static void SetClaManager(ClaManager *cMan) {
		CalculationManager::claMan = cMan;
		}

	static void SetPmatManager(PmatManager *pMan) {
		CalculationManager::pmatMan = pMan;
		}

	static void SetData(SequenceData *dat){
		CalculationManager::data = dat;
		}
	
	void SetOfprefix(string &prefix){
		ofprefix = prefix;
		}

	void GetBeagleSiteLikelihoods(double *likes);

	//This reports back whether Beagle ended up using single precision.  It may be because it was specifically specified,
	//or just implied by other settings such as GPU.  Should be called AFTER SetBeagleDetails and InitializeBeagle, at which
	//point the details of the instance are definitely known
	//bool IsSinglePrecision() {return singlePrecBeagle;}

	//Called by Tree
	//For likelihood : pass the effective root node (any non-tip), it will figure out which clas to combine
	//For derivs     : pass the node on the upper end of the branch.  may be terminal
	ScoreSet CalculateLikelihoodAndDerivatives(const TreeNode *topOfBranch, bool calcDerivs);
	
private:
	/*called by CalculateLikelihoodAndDerivatives. interprets the focal node 
	depending on whether derivatives are requested*/
	void DetermineRequiredOperations(const TreeNode *focalNode, bool derivatives);

	/*called by DetermineRequiredOperations. using dependecies pre-set in the ClaHolders, tracks backwards recursively to 
	determine all of the pmats that must be calculated (TransMatOps) and the clas that must be combined (ClaOps) to get 
	the initially passed holder number.  Note that at this point any notion of a tree or directionality is out of the 
	picture, only dependencies remain*/
	int AccumulateOpsOnPath(int holderInd, list<NodeOperation> &nodeOps);

	/*called by CalculateLikelihoodAndDerivatives after needed cla ops are determined.  Gets calcs
	up to the point where CalcScoringOpt is next*/
	void UpdateAllConditionals();

	/*Called from UpdateAllConditionals, claims the destination clas for the current dep level.  Those
	clas then remain reserved as they are the deps of the next level, then they are unreserved either
	just before the call to CalcLikeAndDerives returns or as the dep pass continue, if mem is limited*/
	void ReserveDestinationClas(BlockingOperationsSet &set);

	/*Mark clas as recycleable after they are no longer directly needed.  The dep levels remain the same, 
	so lower dep level clas will still be reclaimed before higher ones*/
	void UnreserveDependencyClas(const BlockingOperationsSet &set);

	void QueueDependencyClasForFreeing(const BlockingOperationsSet &set);
	void QueueDestinationClasForFreeing(const BlockingOperationsSet &set);

	//resets single opSet. not used I think
	void ResetDepLevels(const BlockingOperationsSet &set);

	/*resets depLevels and any temp reservations for all ops in operationsSetQueue.  Done before returning from
	CalcLikeAndDerivs*/
	void ResetDepLevelsAndReservations();

	//Calculate a whole set of cla operations at once - each combines two clas and two transmats into one dest cla
	void PerformClaOperationBatch(const list<ClaOperation> &theOps);

	//Calculate a whole set of transmat operations at once. May include derivative matrix calcs too.
	//Acutal calculation of the matrices may happen in Garli and then be sent to beagle
	void PerformTransMatOperationBatch(const list<TransMatOperation> &theOps);
	//call back to the underlying models to calculate the pmats, then send them to beagle
	void SendTransMatsToBeagle(const list<TransMatOperation> &theOps);

	//calculate and return either the lnL alone, or the lnl, D1 and D2
	ScoreSet PerformScoringOperation(const ScoringOperation *theOp);

	//THESE SINGLE OP VERSIONS ARE DEPRECATED IN FAVOR OF BATCH OPS ABOVE
	//always combine two clas and one pmat
	void PerformClaOperation(const ClaOperation *theOp);
	//calculate a pmat, or a pmat, d1mat and d2mat
	void PerformTransMatOperation(const TransMatOperation *theOp);
	
	//these are just duplicates of the functions originally in Tree.  They are not used with beagle
	void CalcFullClaInternalInternal(CondLikeArray *destCLA, const CondLikeArray *LCLA, const CondLikeArray *RCLA,  MODEL_FLOAT ***Lpr,  MODEL_FLOAT ***Rpr, const Model *mod);
	void CalcFullCLATerminalTerminal(CondLikeArray *destCLA, const char *Ldata, const char *Rdata,  MODEL_FLOAT ***Lpr,  MODEL_FLOAT ***Rpr, const Model *mod);
	void CalcFullCLAInternalTerminal(CondLikeArray *destCLA, const CondLikeArray *LCLA, char *data2,  MODEL_FLOAT ***pr1,  MODEL_FLOAT ***pr2, const Model *mod, const unsigned *ambigMap);
	FLOAT_TYPE GetScorePartialTerminalRateHet(const CondLikeArray *partialCLA,  MODEL_FLOAT ***prmat, const char *Ldata, const Model *mod);
	FLOAT_TYPE GetScorePartialInternalRateHet(const CondLikeArray *partialCLA, const CondLikeArray *childCLA,  MODEL_FLOAT ***prmat, const Model *mod);

#ifdef USE_BEAGLE
	//BEAGLE SPECIFIC FUNCS

	void SendTipDataToBeagle();

	//interpret beagle error codes, output a message and bail if termOnBeagleError == true
	void CheckBeagleReturnValue(int err, const char *funcName) const;

	//simple report
	void ParseInstanceDetails(const BeagleInstanceDetails *det);

	//These functions can be altered to get indeces from different schemes without actually altering the
	//functions that talk to beagle

	/*beagle treats cla indeces 0 -> NTax - 1 as the tips, then above that the other internals.  My internal cla indexing
	starts at zero, and the negatives are just (-taxnum) NOT starting at zero.  So, convert like this: 
			T4 T3 T2 T1 int1 int2 int3
		me	-4 -3 -2 -1  0    1     2
		Be	 3  2  1  0  4    5     6	*/
	
	/*This gets extremely confusing when using holders as pointers to cla objects, which is necessary for recycling
		H: HolderIndex - These represent particular holders in the allHolders array.   They are the index held by the nodes, 
			and used for dependencies, ranging from 0 - #holders. This is the index that the claMan will always be passed.
			If clean (or reserved and about to be filled) the theMat member will point to a cla object, otherwise NULL

		HD: Holder index for dependencies.  The tips are negative -(tip#), non-negative are same as H
		
		C: ClaIndex - Each cla object has its own fixed number, from 0 to #clas, with each corresponding to one partial 
			buffer allocated in beagle.  When passed to beagle the numbers must be altered (incremented by , since it uses the first 
			0 -> Ntax-1 partial indeces to represent the tips, which may actually be represented by a partial or not.

		CD: ClaIndex for dependencies - For internal nodes, these are the ClaIndecies, with the same mapping
			for tips, these are the -(tip#)

		S: ScalerIndex - These correspond exactly to the C. No object actually exists, they are just allocated in equal
			number as the C, and are dirtied/cleaned at the same time.  Indexing scheme is NOT SAME AS C, is identical in G and B,
			both beginning indexing from 0, up to #clas.  No corresponding object is really indexed in my normal G scheme, 
			but are again tied one to one with the clas

						range				cla_mapping				beagle_mapping			beagle_range				
		H		0 -> (as many as needed)	arbitrary					NA						NA
		HD		-(#tips) -> #C				neg maps to tip				NA						NA

		C		0 -> #C							NA					  C+#tips			#tips -> #tips + #partial buffers
		CD		-(#tips) -> #C			  1to1 excp tips		D<0 ? -C + 1 : C+#tips		0 -> #partials + #tips
		
		S		0 -> #C						1to1 with C				1to1 with C			0 -> #partials (?)
		SD		-(#tips) -> #C				1to1 excp tips			D<0 ? NULL : C
		*/

	//passed in a holder dependency index
	//return beagle partial index (might actually represent tip)
	int PartialIndexForBeagle(int ind) const{
#ifdef PASS_HOLDER_INDECES
		return (ind < 0 ? ((-ind) - 1) : ind + data->NTax());
#else
		return (ind < 0 ? ((-ind) - 1) : claMan->GetClaIndexForBeagle(ind) + data->NTax());
#endif
		}
	/*beagle rescaling indeces are in the same order as mine, and they match my partial indeces*/
	int ScalerIndexForBeagle(int ind) const{
#ifdef PASS_HOLDER_INDECES		
		return ind;
#else
		//this is the code used for the cumulative rescaling index which is reused again and again
		//there aren't actually that many clas, so don't pass this in G functions
		if(ind == GARLI_FINAL_SCALER_INDEX)
			return claMan->NumClas();
		else if(ind < 0)
			return ind;
		else
			return claMan->GetClaIndexForBeagle(ind);
#endif
		}
	/*beagle doesn't know anything about pmat vs deriv mats in terms of storage.  So, I keep track of TransMatSets, which contain one of each
	these will be transformed into beagle indeces such that the pmat, d1 and d2 mats use three consecutive beagle mat indeces
	thus, for my TransMatSet 0, the beagle pmat index is 0, d1 index is 1, d2 is 2.  TransMatSet 1 is 3, 4, 5, etc. */
	int PmatIndexForBeagle(int ind) const{
#ifdef PASS_PMAT_HOLDER_INDECES
		return (ind * 3);
#else
		return pmatMan->GetPmatIndexForBeagle(ind);
#endif
		}
	int D1MatIndexForBeagle(int ind) const{
#ifdef PASS_PMAT_HOLDER_INDECES
		return (ind * 3) + 1;
#else
		return pmatMan->GetD1MatIndexForBeagle(ind);
#endif
		}
	int D2MatIndexForBeagle(int ind) const{
#ifdef PASS_PMAT_HOLDER_INDECES
		return (ind * 3) + 2;
#else
		return pmatMan->GetD2MatIndexForBeagle(ind);
#endif
		}

	void AccumulateRescalers(int destIndex, int childIndex1, int childIndex2);
	void OutputBeagleSiteValues(ofstream &out, bool derivs) const;

	//for testing/debugging
	void SendClaToBeagle(int num){
		CheckBeagleReturnValue(
			beagleSetPartials(
				beagleInst,
				PartialIndexForBeagle(num),
				claMan->GetClaFillIfNecessary(num)->arr), 
			"beagleSetPartials");
		}

	void OutputBeagleTransMat(int index);
	//this is just for debugging
	void OutputOperationsSummary() const;
	//Sum site likes/derivs. Beagle now sums on its own, so no longer necessary except for debugging
	ScoreSet SumSiteValues(const FLOAT_TYPE *sitelnL, const FLOAT_TYPE *siteD1, const FLOAT_TYPE *siteD2) const;
#endif 
	};
#endif
