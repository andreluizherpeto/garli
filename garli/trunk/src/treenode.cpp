// GARLI version 0.952b2 source code
// Copyright  2005-2006 by Derrick J. Zwickl
// All rights reserved.
//
// This code may be used and modified for non-commercial purposes
// but redistribution in any form requires written permission.
// Please contact:
//
//  Derrick Zwickl
//	National Evolutionary Synthesis Center
//	2024 W. Main Street, Suite A200
//	Durham, NC 27705
//  email: zwickl@nescent.org
//
//	NOTE: Portions of this source adapted from GAML source, written by Paul O. Lewis

#include <cassert>
#include <iostream>
#include <fstream>

using namespace std;

#include "treenode.h"
#include "clamanager.h"
#include "bipartition.h"
#include "errorexception.h"
#include "outputman.h"
#include "mlhky.h"

#ifdef BOINC
	#include "boinc_api.h"
	#include "filesys.h"
	#ifdef _WIN32
		#include "boinc_win.h"
	#else
		#include "config.h"
	#endif
#endif

extern OutputManager outman;

#undef DEBUG_RECOMBINEWITH

TreeNode::TreeNode( const int no )
	: left(0), right(0), next(0), prev(0), anc(0), tipData(0L), bipart(0L)
{	attached =false;
	
	claIndexDown=-1;
	claIndexUL=-1;
	claIndexUR=-1;
		
	nodeNum  = no;
	dlen	= 0.0;
/* GANESH added this */
#ifdef PECR_SET_PARSIMONY_BRLEN
    /* every node is a leaf (no descendants) when created */
    leaf_mask = true;
#endif    
}

TreeNode::~TreeNode(){
	if(bipart!=NULL) delete bipart;
}

TreeNode* TreeNode::AddDes(TreeNode *d){
	//leaves blens as-is
	d->anc=this;
	d->next=NULL;
	if(left){
		if(right){
			right->next=d;
			d->prev=right;
			right=d;
			}
		else{
			right=d;
			left->next=d;
			d->prev=left;
			}
		}
	else{
		left=d;
		d->prev=NULL;
		}
	d->attached=true;

/* GANESH added this */
#ifdef PECR_SET_PARSIMONY_BRLEN
    /* not a leaf any more once we add descendants */
    leaf_mask = false;
#endif
	return d;
}

void TreeNode::RemoveDes(TreeNode *d){
	//leaves blens as-is
	assert(d->anc == this);
	//remove d from this
	if(d->prev != NULL) d->prev->next=d->next;
	if(d->next != NULL) d->next->prev=d->prev;
	if(d == left)
		left=d->next;
	else if(d == right)
		right=d->prev;

	d->next = d->prev = NULL;
	d->anc=NULL;
	}

void TreeNode::MoveDesToAnc(TreeNode *d){
	//this assumes that the anc is currently NULL,
	//and makes the specified des the new anc.
	//*this is added as a des of that anc
	//the blen of the des is trasfered to this
	assert(anc == NULL);
	dlen = d->dlen;
	d->dlen=-1;
	RemoveDes(d);

	//now add this to d
	d->AddDes(this);
	}

char *TreeNode::MakeNewick(char *s, bool internalNodes, bool branchLengths, bool highPrec /*=false*/) const{

	if(left){
		if(internalNodes==true && nodeNum!=0){
			sprintf(s, "%d", nodeNum);
			while(*s)s++;
			}
		*s++='(';
		s=left->MakeNewick(s, internalNodes, branchLengths);
		if(anc){
			if(branchLengths==true){
				*s++=':';
				if(highPrec == false)
					sprintf(s, "%.8lf", dlen);
				else sprintf(s, "%.10lf", dlen);
				while(*s)s++;
				}
			}
		else
			{*s='\0';
			return s;
			}
		}
	else {
		sprintf(s, "%d", nodeNum);
		while(*s)s++;
		if(branchLengths==true){
			*s++=':';
			if(highPrec == false)
				sprintf(s, "%.8lf", dlen);
			else sprintf(s, "%.10lf", dlen);
			while(*s)s++;
			}
		}
		
	if(next){
		*s++=',';
		s=next->MakeNewick(s, internalNodes, branchLengths);
		}
	else {
		if(anc){
			*s++=')';
			}
		}
	return s;
	}

void TreeNode::MakeNewickForSubtree(char *s) const{
	assert(left);
	*s++='(';
	s=left->MakeNewick(s, false, false);
	*s++=';';
	*s++='\0';
	}

//MTH
void TreeNode::Prune()
{	//DZ 7-6 removing adjustments to branch lengths when pruning, which just result in adding a whole bunch of length
	//to the whole tree, making the dlens get longer and longer and longer as the run progresses
	assert(anc);//never call with this=root
	attached=false;
	if(anc->anc)
		{//not connected to the root
		if(anc->left->next==anc->right)
			{TreeNode *sis;
			if(anc->left==this)
				sis=anc->right;
			else
				sis=anc->left;
//			sis->dlen+=anc->dlen;
			anc->SubstituteNodeWithRespectToAnc(sis);
			anc->attached=false;
			}
		else
			{
			anc=anc;
			assert(0);//internal polytomy
			}
		}
	else
		{
		//these assume a trifurcating root
		if(anc->left==this){
			anc->left=next;
			anc->left->prev=NULL;
			anc->left->next=anc->right;
			}
		else
			if(anc->right==this){
				anc->right=prev;
				anc->right->next=NULL;
				anc->left->next=anc->right;
				}
			else 
				{assert(anc->left==prev && anc->right==next);
		next->prev=prev;
		prev->next=next;
		assert(anc->left &&  anc->right);
		TreeNode *temp;
				if(anc->left->left){
					//anc->right->dlen+=anc->left->dlen;
			temp=anc->left;
			temp->SubstituteNodeWithRespectToAnc(temp->left);
			anc->AddDes(temp->right);
			temp->attached=false;
			}
		else
					if(anc->right->left){
						//anc->left->dlen+=anc->right->dlen;
				temp=anc->right;
				temp->SubstituteNodeWithRespectToAnc(temp->left);
				anc->AddDes(temp->right);
				temp->attached=false;
						}
				}
		
		}
}

//MTH	
void TreeNode::SubstituteNodeWithRespectToAnc(TreeNode *subs)//note THIS DOESN't do anything with numBranchesAdded or any other tree fields that describe the tree
{		//this function moves subs into the place in the tree that had been occupied by this
		// it is called in swapping and addRandomNode and can't be called with the root as this
		//  Nothing is done with branch lengths OR LEFT OR RIGHT (this and subs still keep their descendants)
		subs->anc=anc;
		subs->prev=prev;
		subs->next=next;
		assert(anc);
		if(anc->left==this)
			anc->left=subs;
		if(anc->right==this)
			anc->right=subs;
		if(next)
			next->prev=subs;
		if(prev)
			prev->next=subs;
		subs->attached=true;
		attached=false;
		next=prev=anc=NULL;
}

int TreeNode::CountBranches(int s){
	if(left)
		s=left->CountBranches(++s);
	if(nodeNum==0)
	  s=left->next->CountBranches(++s);
	if(right)
		s=right->CountBranches(++s);
	return s;
}

int TreeNode::CountTerminals(int s){
	if(left)
		s=left->CountTerminals(s);
	else s++;
	if(nodeNum==0)
		s=left->next->CountTerminals(s);
	if(right)
		s=right->CountTerminals(s);
	return s;
}

int TreeNode::CountTerminalsDown(int s, TreeNode *calledFrom){
	TreeNode *sib;
	
	if(nodeNum!=0){		
		if(left==calledFrom) sib=right;
		else sib=left;

		if(sib)
			s=sib->CountTerminals(s);
		else s++;

		s=anc->CountTerminalsDown(s, this);
		}
	else {
		if(left!=calledFrom) s=left->CountTerminals(s);
		if(left->next!=calledFrom) s=left->next->CountTerminals(s);
		if(right!=calledFrom) s=right->CountTerminals(s);
		}
	return s;
}

void TreeNode::CountSubtreeBranchesAndDepth(int &branches, int &sum, int depth, bool first) const{
//this is the version to use if you want to be
//sure not to jump to another subtree (ie, don't 
//go ->next from the calling node)

	if(left){
		sum+=depth;
		left->CountSubtreeBranchesAndDepth(++branches, sum, depth+1, false);
		}
	if(next&&!first){
		sum+=depth-1;
		next->CountSubtreeBranchesAndDepth(++branches, sum, depth, false);
		}
}
	
void TreeNode::CalcDepth(int &dep){
	dep++;
	int l=0, r=0;
	if(left){
		left->CalcDepth(l);
		}
	if(right){
		right->CalcDepth(r);
		}
	dep += (r > l ? r : l);
	}

void TreeNode::MarkTerminals(int *taxtags){
	if(left)
		left->MarkTerminals(taxtags);
	else
		taxtags[nodeNum]=1;
	if(next)
		next->MarkTerminals(taxtags);
}

void TreeNode::MarkUnattached(bool includenode){
	attached=false;
	if(left)
		left->MarkUnattached(false);
	if(next&&includenode==false)
		next->MarkUnattached(false);	
	}
	
TreeNode* TreeNode::FindNode( int &n, TreeNode *tempno){		
	//my version DZ.  It returns nodeNum n
	if(left&&tempno!=NULL){
		tempno=left->FindNode(n, tempno);
		}
	if(next&&tempno!=NULL){
		tempno=left->FindNode(n, tempno);
		}
	if(nodeNum==n){
		tempno=this;
		}			
	return tempno;
	}
	
//MTH
TreeNode* TreeNode::FindNode( int &n){
		//note that this function does NOT look for the node with nodeNum n, but rather
		//counts nodes and returns the nth one that it finds
		n--;
		if(n<0)
			return this;
		if(left)
			{TreeNode* tempno;
			tempno=left->FindNode(n);
			if(tempno)
				return tempno;
			}
		if(next)
			return next->FindNode(n);
		return NULL;
}

bool TreeNode::IsGood()
{	if(attached || !anc)
		{if(!left && right)
			return false;
		if(!anc)
			{if(nodeNum!=0 || next || prev)
				return false;
			}
		else
			{TreeNode *tempno;
			tempno=anc->left;
			bool found=false;
			int nsibs=0;
			while(tempno)
				{if(tempno->anc!=anc)
					return false;
				if(tempno==this)
					found=true;
				tempno=tempno->next;
				nsibs++;
				if(nsibs>3)
					return false;
				}
			if(!found)
				return false;
			}
		if(left){
			
			if(!left->IsGood())
				return false;
			}
		if(next)
			return next->IsGood();
		return true;
		}
	else
		return false;
}

void TreeNode::CountNumberofNodes(int &nnodes){
	if(left!=NULL){
		left->CountNumberofNodes(nnodes);
		}
	if(next!=NULL){
		next->CountNumberofNodes(nnodes);
		}
	nnodes++;
	}
	
void TreeNode::CheckforLeftandRight(){
	if(left!=NULL){
		left->CheckforLeftandRight();
		}
	
	if(next!=NULL){
		next->CheckforLeftandRight();
		}
	
	if((left&&!right)||(right&&!left)){
		assert(0);
		}
	}
	
void TreeNode::FindCrazyLongBranches(){
	if(left!=NULL){
		left->FindCrazyLongBranches();
		}
	
	if(next!=NULL){
		next->FindCrazyLongBranches();
		}
	if(dlen>1.0){
		outman.UserMessage("WTF?");
		}
	}
			
void TreeNode::FindCrazyShortBranches(){
	if(left!=NULL){
		left->FindCrazyShortBranches();
		}
	
	if(next!=NULL){
		next->FindCrazyShortBranches();
}
	if(anc&&dlen<.0001){
		outman.UserMessage("WTF?");
		}
	}
	
void TreeNode::CheckTreeFormation()	{
#ifndef NDEBUG
	//make sure that nodes that this node points to also point back (ie this->ldes->anc=this)
	if(left){
		assert(left->anc==this);
		left->CheckTreeFormation();
		}
	if(right){
		assert(right->anc==this);
		}
	if(next){
		assert(next->prev==this);
		next->CheckTreeFormation();
		}
	if(prev){
		assert(prev->next==this);
		}
	assert(!anc||dlen>0.0);
#endif
	}

void TreeNode::CheckforPolytomies(){
	
	if(IsInternal()){
		left->CheckforPolytomies();
		}

	if(next!=NULL){
		next->CheckforPolytomies();
		}
		
	if(anc!=NULL){
		if(left!=NULL){
			if(left->next!=right){
				//we don't ever allow polytomous trees to be used,
				//so crap out here is we detect one
				throw ErrorException("Error: Input tree has polytomies!!  See FAQ for suggestions on avoiding this.");
				assert(0);
				}
			}
		}
	}

Bipartition* TreeNode::CalcBipartition(){	
	if(IsInternal()){//not terminal
		TreeNode *nd=left;
		*bipart = nd->CalcBipartition();
		nd=nd->next;
		do{
			*bipart += nd->CalcBipartition();
			nd=nd->next;
			}while(nd != NULL);
		return bipart;
		}
	else if(IsNotRoot()){//terminal
		bipart=bipart->TerminalBipart(nodeNum);	
		return bipart;
		}
	return NULL;
	}

void TreeNode::StandardizeBipartition(){
	if(IsInternal()){//not terminal
		TreeNode *nd=left;
		do{
			nd->StandardizeBipartition();
			nd=nd->next;
			}while(nd != NULL);
		}
	bipart->Standardize();
	}

void TreeNode::GatherConstrainedBiparitions(vector<Bipartition> &biparts) {
	if(IsInternal()){
		TreeNode *nd=left;
		do{
			nd->GatherConstrainedBiparitions(biparts);
			nd=nd->next;
			}while(nd != NULL);
		if(IsNotRoot()){
			Bipartition b(*bipart);
			biparts.push_back(b);
			}
		}
	}

void TreeNode::OutputBipartition(ostream &out){	
	if(left&&anc){
		left->OutputBipartition(out);
		left->next->OutputBipartition(out);
		out << bipart->Output() << endl;
		}
	else if(!anc){
		left->OutputBipartition(out);
		left->next->OutputBipartition(out);
		left->next->next->OutputBipartition(out);
		}
	}

void TreeNode::RotateDescendents(){
	//don't call this with the root!
	assert(anc);
	TreeNode* tmp=right;
	right=left;
	left=tmp;
	left->prev=NULL;	
	left->next=right;
	right->next=NULL;
	}

void TreeNode::AddNodesToList(vector<int> &list){
	list.push_back(nodeNum);
	if(IsInternal()) left->AddNodesToList(list);
	if(next!=NULL) next->AddNodesToList(list);
	}
	
void TreeNode::FlipBlensToRoot(TreeNode *from){
	if(anc!=NULL) anc->FlipBlensToRoot(this);
	if(from==NULL) dlen=-1;
	else dlen=from->dlen;
	}

void TreeNode::FlipBlensToNode(TreeNode *from, TreeNode *stopNode){
	//for rerooting a subtree
	//each node gets the get blen of the previous node (one of
	//its des)
	assert(IsNotRoot());
	assert(from != NULL);
	assert(stopNode != NULL);
	if(anc != stopNode) 
		anc->FlipBlensToNode(this, stopNode);
	else dlen=from->dlen;
	}

void TreeNode::PrintSubtreeMembers(ofstream &out){
	if(IsTerminal()) out << nodeNum << "\t"; 
	else left->PrintSubtreeMembers(out);
	if(next!=NULL) next->PrintSubtreeMembers(out);
	}
	
void TreeNode::AdjustClasForReroot(int dir){
	int tmp=claIndexDown;
	if(dir==2){//the ancestor and left des have been swapped
		claIndexDown=claIndexUL;
		claIndexUL=tmp;
		}
	else if(dir==3){//the ancestor and right des have been swapped
		claIndexDown=claIndexUR;
		claIndexUR=tmp;		
		}
	else assert(0);
	}	

void TreeNode::RecursivelyAddOrRemoveSubtreeFromBipartitions(Bipartition *subtree){
	//this function just tricks nodes down to the root into thinking
	//that a taxon is in their subtree by flipping its bit in the bipartition
	//this obviously needs to be undone by calcing the biparts if the true
	//tree bipartitions are needed
	bipart->FlipBits(subtree);
	if(anc->IsNotRoot()) anc->RecursivelyAddOrRemoveSubtreeFromBipartitions(subtree);
	}

//unsigned MATCH_II=0, MATCH_TT=0, MATCH_IT=0, TOT_II=0, TOT_IT=0, TOT_TT=0;

void TreeNode::SetEquivalentConditionalVectors(const HKYData *data){
	if(nodeNum == 0){
		if(left->IsInternal()) left->SetEquivalentConditionalVectors(data);
		if(left->next->IsInternal()) left->next->SetEquivalentConditionalVectors(data);
		if(right->IsInternal()) right->SetEquivalentConditionalVectors(data);
		return;
		}

	if(left->IsTerminal() && right->IsTerminal()){
		unsigned char *leftSeq = data->GetRow(left->nodeNum-1);
		unsigned char *rightSeq = data->GetRow(right->nodeNum-1);
		char lastLeft, lastRight;
		lastLeft = leftSeq[0];
		lastRight = rightSeq[0];
		tipData[0] = 0;
		for(int i=1;i<data->NChar();i++){
			bool match=true;
			if(leftSeq[i] != lastLeft){
				lastLeft = leftSeq[i];
				match=false;
				}
			if(rightSeq[i] != lastRight){
				lastRight = rightSeq[i];
				match=false;
				}
//			MATCH_TT += match;
//			TOT_TT++;
			tipData[i] = match;
			}
		}
	else if(left->IsInternal() && right->IsInternal()){
		left->SetEquivalentConditionalVectors(data);
		right->SetEquivalentConditionalVectors(data);
		for(int i=0;i<data->NChar();i++){
			tipData[i] = left->tipData[i] && right->tipData[i];
//			MATCH_II += tipData[i];
//			TOT_II++;
			}
		}
	else if(left->IsTerminal()){
		right->SetEquivalentConditionalVectors(data);
		unsigned char *leftSeq = data->GetRow(left->nodeNum-1);
		char lastLeft;
		lastLeft = leftSeq[0];
		tipData[0] = 0;
		for(int i=1;i<data->NChar();i++){
			bool match=true;
			if(leftSeq[i] != lastLeft){
				lastLeft = leftSeq[i];
				match=false;
				}
//			MATCH_IT += right->tipData[i] && match;
//			TOT_IT++;
			tipData[i] = right->tipData[i] && match;
			}
		}
	else {
		left->SetEquivalentConditionalVectors(data);
		unsigned char *rightSeq = data->GetRow(right->nodeNum-1);
		char lastRight;
		lastRight = rightSeq[0];
		tipData[0] = 0;
		for(int i=1;i<data->NChar();i++){
			bool match=true;
			if(rightSeq[i] != lastRight){
				lastRight = rightSeq[i];
				match=false;
				}
//			MATCH_IT += right->tipData[i] && match;
//			TOT_IT++;
			tipData[i] = left->tipData[i] && match;
			}
		}
	}

void TreeNode::OutputBinaryNodeInfo(ofstream &out) const{
	int zero = 0;

	if(this->IsInternal()){
		out.write((char*) &(left->nodeNum), sizeof(int));
		out.write((char*) &(right->nodeNum), sizeof(int));
		}
	if(prev == NULL) out.write((char*) &zero, sizeof(int));
	else out.write((char*) &(prev->nodeNum), sizeof(int));

	if(next == NULL) out.write((char*) &zero, sizeof(int));
	else out.write((char*) &(next->nodeNum), sizeof(int));
	
	if(anc == NULL) out.write((char*) &zero, sizeof(int));
	else out.write((char*) &(anc->nodeNum), sizeof(int));
	
	out.write((char*) &dlen, sizeof(FLOAT_TYPE));
	}

#ifdef BOINC
void TreeNode::OutputBinaryNodeInfoBOINC(MFILE &out) const{
	int zero = 0;

	if(this->IsInternal()){
		out.write((char*) &(left->nodeNum), sizeof(int), 1);
		out.write((char*) &(right->nodeNum), sizeof(int), 1);
		}
	if(prev == NULL) out.write((char*) &zero, sizeof(int), 1);
	else out.write((char*) &(prev->nodeNum), sizeof(int), 1);

	if(next == NULL) out.write((char*) &zero, sizeof(int), 1);
	else out.write((char*) &(next->nodeNum), sizeof(int), 1);
	
	if(anc == NULL) out.write((char*) &zero, sizeof(int), 1);
	else out.write((char*) &(anc->nodeNum), sizeof(int), 1);
	
	out.write((char*) &dlen, sizeof(FLOAT_TYPE), 1);
	}
#endif