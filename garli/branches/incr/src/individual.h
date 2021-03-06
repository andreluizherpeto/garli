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

#ifndef INDIVIDUAL_H
#define INDIVIDUAL_H

#include "tree.h"
#include "model.h"
#include "hashdefines.h"

class CondLikeArray;
class Tree;
class ParallelManager;
class Adaptation;


extern bool gOptimizePlausibleDuringStepwise;
class Individual
{
	FLOAT_TYPE fitness;
	bool dirty;      // individual becomes dirty if mutated in any way

	public:

		int mutation_type;
		//here we define the binary equivalents of the mutation types, so that they can all be rolled
		//into a single int with bit flags
		enum {	//normal mutation types
				randNNI 	= 0x0001,  //1
			 	randSPRCon	= 0x0002,  //2
			 	randSPR		= 0x0004,  //4			
			 	limSPR		= 0x0008,  //8
			 	limSPRCon	= 0x0010,  //16
			 	randRecom	= 0x0020,  //32
			 	bipartRecom	= 0x0040,  //64
				taxonSwap   = 0x1000,  //4096
				subtreeRecom= 0x2000,  //8192
				
			 	brlen		= 0x0080,  //128

			 	rates		= 0x0100,  //256
			 	pi			= 0x0200,  //512
			 	alpha		= 0x0400,  //1024
			 	pinv		= 0x0800,  //2048
			 	muScale		= 0x10000, //65536
	#ifdef GANESH
                randPECR    = 0x4000,  //16384                
	#endif		 	
			 	rerooted	= 0x8000,  //32768 - this is needed because in many senses a tree that has been rerooted
			 						   //is a new topology (for example the left, right and anc pointers from a particular nodenum
			 						   //won't be the same before and after rerooting) although the likelihood is the same

			 	//compostite types

#ifdef GANESH
			 	anyTopo		= (randNNI | exNNI | randSPR | limSPR 
			 		 | exlimSPR | randRecom | bipartRecom | taxonSwap
                     | subtreeRecom | randPECR ) ,
#else
			 	anyTopo		= (randNNI | randSPRCon | randSPR | limSPR 
			 		 | limSPRCon | randRecom | bipartRecom | taxonSwap | subtreeRecom ) ,
#endif
			 	anyModel	= rates | pi | alpha | pinv | muScale
			 	};
		int mutated_brlen;//the number of brlen muts
		bool accurateSubtrees;

		Model *mod;
		
		Tree *treeStruct;

		bool reproduced;
		bool willreproduce;
		bool willrecombine;
		int recombinewith;
		int parent;
	private:
		int topologyInt;
	public:
		Individual();
		Individual(const Individual *other);
		~Individual();
		
		Individual & operator=(const Individual &);
		
		// getters and setters
		const int GetTopo() const { 
			return this->topologyInt;
			}
		const int SetTopo(int t) { 
			this->topologyInt = t;
			return t;
			}


		FLOAT_TYPE Fitness() const { return fitness; }
		FLOAT_TYPE GetFitness() { 
			if (this->dirty)
				CalcFitness(0);
			return this->fitness; 
		}
		void SetDirty() { dirty = true; }
		bool IsDirty() const { return dirty; }

		void SetFitness( FLOAT_TYPE f ) {
			fitness = f;
			dirty=false;
			}
		void GetStartingConditionsFromFile(const char *fname, int rank, int nTax, bool restart=false);
		void GetStartingTreeFromNCL(const NxsTreesBlock *treesblock, int rank, int nTax, bool restart=false, bool demandAllTaxa=true);
		void ReadNxsFullTreeDescription(const NxsFullTreeDescription &t, bool demandAllTaxa=true);
		void RefineStartingConditions(bool optModel, FLOAT_TYPE branchPrec);
		void CalcFitness(int subtreeNode);
		void ReadTreeFromFile(istream & inf);

		
		
		void CrossOverWith( Individual& so, FLOAT_TYPE optPrecision);
		
		void CopyNonTreeFields(const Individual* ind );
		void CopyByStealingTree(Individual* ind );
		void CopySecByStealingFirstTree(Individual * sourceOfTreePtr, const Individual *sourceOfInformation);
		void CopySecByRearrangingNodesOfFirst(Tree * sourceOfTreePtr, const Individual *sourceOfInformation, bool CLAassigned=false);
		void ResetIndiv();
		void MakeRandomTree(unsigned nTax);
		void MakeStepwiseTree(unsigned nTax, unsigned attemptsPerTaxon, FLOAT_TYPE optPrecision );
		void ContinueBuildingStepwiseTree(unsigned nTax, unsigned attachesPerTaxon, FLOAT_TYPE optPrecision, Individual & scratchI, std::set<unsigned> & taxset, int &placeInAllNodes, Bipartition & mask);
		void FinishIncompleteTreeByStepwiseAddition(unsigned nTax, unsigned attachesPerTaxon, FLOAT_TYPE optPrecision , Individual & scratchI);


		void Mutate(FLOAT_TYPE optPrecision, const Adaptation & adap);

		void DoBrLenMutation();
		bool DoTopoMutation(FLOAT_TYPE optPrecision, const Adaptation & adap);
		void DoModelMutation();


	};


inline void Individual::CopyByStealingTree(Individual* ind ){
	CopyNonTreeFields(ind);
	treeStruct=ind->treeStruct;
	}

inline void Individual::ResetIndiv(){
	reproduced=willreproduce=willrecombine=false;
	recombinewith=-1;
	mutation_type=mutated_brlen=0;
	}

#define BIPART_BASED_RECOM

inline void Individual::CrossOverWith( Individual& so , FLOAT_TYPE optPrecision){
	//check if the models are the same, which will allow the replicated parts of the trees
	//to use the same clas
	#ifdef BIPART_BASED_RECOM
	//this will return -1 if no recombination actually occured
	int x=-1;
	x=treeStruct->BipartitionBasedRecombination(so.treeStruct, mod->IsModelEqual(so.mod), optPrecision);
	//if we don't find a bipart based recom that does much good, do a normal one
	if(x==-1){
		/*
		treeStruct->RecombineWith( so.treeStruct, mod->IsModelEqual(so.mod), optPrecision);
		mutation_type |= randRecom;
		*/
		mutation_type=0;
		recombinewith=-1;
		}
	else{
//		recombinewith+=100;
		mutation_type |= bipartRecom;
		fitness=treeStruct->lnL;
		dirty=false;
		CalcFitness(0);
		}
	#else
	treeStruct->RecombineWith( so.treeStruct, params->rnd , kappa, mod->IsModelEqual(so.mod));
	dirty=1;
	#endif
	}


#endif


