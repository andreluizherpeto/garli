// GARLI version 0.951 source code
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

#include <iostream>

using namespace std;

#include "defs.h"
#include "utility.h"
#include "linalg.h"
#include "model.h"
#include "individual.h"
#include "mlhky.h"
#include "rng.h"

#undef ALIGN_MODEL

Profiler ProfCalcPmat("CalcPmat      ");
					 
extern rng rnd;
FLOAT_TYPE Model::mutationShape;

FLOAT_TYPE Model::maxPropInvar;

FLOAT_TYPE PointNormal (FLOAT_TYPE prob);
FLOAT_TYPE IncompleteGamma (FLOAT_TYPE x, FLOAT_TYPE alpha, FLOAT_TYPE LnGamma_alpha);
FLOAT_TYPE PointChi2 (FLOAT_TYPE prob, FLOAT_TYPE v);


Model::~Model(){
	if(stateFreqs.empty() == false){
		for(int i=0;i<(int)stateFreqs.size();i++)
			delete stateFreqs[i];
		}

	if(relNucRates.empty() == false){
		if(modSpec.IsCodon())
			delete relNucRates[0];
		else if(nst==6){
			for(int i=0;i<(int)relNucRates.size();i++)
				delete relNucRates[i];
			}
		else if(nst==2){
			delete relNucRates[0];
			delete relNucRates[1];
			}
		else if(nst==1) delete relNucRates[0];
		}

	if(propInvar != NULL) delete propInvar;

	if(alpha != NULL) delete alpha;

	for(vector<BaseParameter*>::iterator delit=paramsToMutate.begin();delit!=paramsToMutate.end();delit++)
		delete *(delit);

	delete []eigvals;
	delete []eigvalsimag;
	delete []iwork;
	delete []work;
	delete []col;
	delete []indx;
	delete []c_ijk;
	delete []EigValexp;
	delete []EigValderiv;
	delete []EigValderiv2;

#ifndef ALIGN_MODEL
	Delete2DArray(eigvecs);
	Delete2DArray(teigvecs);
	Delete2DArray(inveigvecs);
	Delete3DArray(pmat);
	Delete2DArray(qmat);
	Delete2DArray(tempqmat);
	Delete3DArray(deriv1);
	Delete3DArray(deriv2);
#else
	Delete2DAlignedArray(eigvecs);
	Delete2DAlignedArray(teigvecs);
	Delete2DAlignedArray(inveigvecs);
	Delete3DAlignedArray(pmat);
	Delete2DAlignedArray(qmat);
	Delete2DAlignedArray(tempqmat);
	Delete3DAlignedArray(deriv1);
	Delete3DAlignedArray(deriv2);
#endif
	}

void Model::AllocateEigenVariables(){
#ifndef ALIGN_MODEL
	//a bunch of allocation here for all of the qmatrix->eigenvector->pmatrix related variables
	eigvals=new FLOAT_TYPE[nstates];//eigenvalues
	eigvalsimag=new FLOAT_TYPE[nstates];
	iwork=new int[nstates];
	work=new FLOAT_TYPE[nstates];
	col=new FLOAT_TYPE[nstates];
	indx=new int[nstates];
	c_ijk=new FLOAT_TYPE[nstates*nstates*nstates];	
	EigValexp=new FLOAT_TYPE[nstates*NRateCats()];
	EigValderiv=new FLOAT_TYPE[nstates*NRateCats()];
	EigValderiv2=new FLOAT_TYPE[nstates*NRateCats()];

	//create the matrix for the eigenvectors
	eigvecs=New2DArray<FLOAT_TYPE>(nstates,nstates);

	//create a temporary matrix to hold the eigenvectors that will be destroyed during the invertization
	teigvecs=New2DArray<FLOAT_TYPE>(nstates,nstates);

	//create the matrix for the inverse eigenvectors
	inveigvecs=New2DArray<FLOAT_TYPE>(nstates,nstates);	

	//allocate the pmat
	pmat=New3DArray<FLOAT_TYPE>(NRateCats(), nstates, nstates);

	//allocate qmat and tempqmat
	qmat=New2DArray<FLOAT_TYPE>(nstates,nstates);
	tempqmat=New2DArray<FLOAT_TYPE>(nstates,nstates);

	deriv1=New3DArray<FLOAT_TYPE>(NRateCats(), nstates, nstates);
	deriv2=New3DArray<FLOAT_TYPE>(NRateCats(), nstates, nstates);
#else

	//a bunch of allocation here for all of the qmatrix->eigenvector->pmatrix related variables
	eigvals=new FLOAT_TYPE[nstates];//eigenvalues
	eigvalsimag=new FLOAT_TYPE[nstates];
	iwork=new int[nstates];
	work=new FLOAT_TYPE[nstates];
	col=new FLOAT_TYPE[nstates];
	indx=new int[nstates];
	c_ijk=new FLOAT_TYPE[nstates*nstates*nstates];	
	EigValexp=new FLOAT_TYPE[nstates*NRateCats()];	

	//create the matrix for the eigenvectors
	eigvecs=New2DAlignedArray<FLOAT_TYPE>(nstates,nstates);

	//create a temporary matrix to hold the eigenvectors that will be destroyed during the invertization
	teigvecs=New2DAlignedArray<FLOAT_TYPE>(nstates,nstates);

	//create the matrix for the inverse eigenvectors
	inveigvecs=New2DAlignedArray<FLOAT_TYPE>(nstates,nstates);	

	//allocate the pmat
	pmat=New3DAlignedArray<FLOAT_TYPE>(NRateCats(), nstates, nstates);

	//allocate qmat and tempqmat
	qmat=New2DAlignedArray<FLOAT_TYPE>(nstates,nstates);
	tempqmat=New2DAlignedArray<FLOAT_TYPE>(nstates,nstates);

	deriv1=New3DAlignedArray<FLOAT_TYPE>(NRateCats(), nstates, nstates);
	deriv2=New3DAlignedArray<FLOAT_TYPE>(NRateCats(), nstates, nstates);
#endif
	}

void Model::UpdateQMat(){
	//recalculate the qmat from the basefreqs and rates

	if(modSpec.IsCodon()){
		UpdateQMatCodon();
		return;
		}
	else if(modSpec.IsAminoAcid() || modSpec.IsCodonAminoAcid()){
		UpdateQMatAminoAcid();
		return;
		}
	
	if(nstates==4){
		qmat[0][1]=*relNucRates[0] * *stateFreqs[1];  //a * piC
		qmat[0][2]=*relNucRates[1] * *stateFreqs[2];  //b * piG
		qmat[0][3]=*relNucRates[2] * *stateFreqs[3];  //c * piT
		qmat[1][2]=*relNucRates[3] * *stateFreqs[2];  //d * piG
		qmat[1][3]=*relNucRates[4] * *stateFreqs[3];  //e * piT
		qmat[2][3]=*stateFreqs[3];  			//f(=1) * piT 
		qmat[1][0]=*relNucRates[0] * *stateFreqs[0];  //a * piA
		qmat[2][0]=*relNucRates[1] * *stateFreqs[0];  //b * piA
		qmat[2][1]=*relNucRates[3] * *stateFreqs[1];  //d * piC
		qmat[3][0]=*relNucRates[2] * *stateFreqs[0];  //c * piA
		qmat[3][1]=*relNucRates[4] * *stateFreqs[1];  //e * piC
		qmat[3][2]=*stateFreqs[2]; 			//f(=1) * piG
		}
	else {
		//general nstate x nstate method	
		int rnum=0;
		for(int i=0;i<nstates;i++){
			for(int j=i+1;j<nstates;j++){
				qmat[i][j]=*relNucRates[rnum] * *stateFreqs[j];
				qmat[j][i]=*relNucRates[rnum] * *stateFreqs[i];
				rnum++;
				}
			}
		}
		
	//set diags to sum rows to 0
	FLOAT_TYPE sum;
	for(int x=0;x<nstates;x++){
		sum=ZERO_POINT_ZERO;
		for(int y=0;y<nstates;y++){
			if(x!=y) sum+=qmat[x][y];
			}
		qmat[x][x]=-sum;
		}
	}

void Model::FillQMatLookup(){
	//the code here is:
	//bit 1 = viable 1 nuc change path
	//bit 2 = transition
	//bit 4 = Nonsynonymous
	//bit 8 = A->C or C->A nuc change
	//bit 16 = A->G or G->A nuc change
	//bit 32 = A->T or T->A nuc change
	//bit 64 = C->G or G->C nuc change
	//bit 128 = C->T or T->C nuc change
	//bit 256 = G->T or T->G nuc change
	
	//although it seems a little fucked up, I'm going to fill the 64x64 matrix, and then eliminate the
	//rows and columns that are stop codons, since they differ for different codes and the following 
	//stuff would be hell without regularity
	
	int *tempqmatLookup=new int[64*64];
	for(int q=0;q<64*64;q++) tempqmatLookup[q]=0;
	//its easier to do this in 4 x 4 blocks
	for(int i=0;i<16;i++){
		for(int j=0;j<16;j++){
			for(int ii=0;ii<4;ii++){
				for(int jj=0;jj<4;jj++){
					if(i==j){//on diagonal 4x4
						if(ii!=jj){
							//all the cells in this subsection are 1 nuc change away
							tempqmatLookup[64*(i*4+ii) + (j*4+jj)] = 1;
							if((ii+jj)%2 == 0)
								tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 2;
							if((ii==0 && jj==1) || (ii==1 && jj==0)) tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 8;
							else if((ii==0 && jj==2) || (ii==2 && jj==0)) tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 16;
							else if((ii==0 && jj==3) || (ii==3 && jj==0)) tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 32;
							else if((ii==1 && jj==2) || (ii==2 && jj==1)) tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 64;
							else if((ii==1 && jj==3) || (ii==3 && jj==1)) tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 128;
							else if((ii==2 && jj==3) || (ii==3 && jj==2)) tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 256;
							}
						}
					else if(floor(i/4.0)==floor(j/4.0)){//near diagonal 4x4, some cells differ at the 2nd pos
						if(ii==jj){
							//the diagonal cells in this subsection are 1 nuc change away
							tempqmatLookup[64*(i*4+ii) + (j*4+jj)] = 1;
							if(abs(i-j) == 2)
								tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 2;
							if((i%4==0 && j%4==1) || (i%4==1 && j%4==0)) tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 8;
							else if((i%4==0 && j%4==2) || (i%4==2 && j%4==0)) tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 16;
							else if((i%4==0 && j%4==3) || (i%4==3 && j%4==0)) tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 32;
							else if((i%4==1 && j%4==2) || (i%4==2 && j%4==1)) tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 64;
							else if((i%4==1 && j%4==3) || (i%4==3 && j%4==1)) tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 128;
							else if((i%4==2 && j%4==3) || (i%4==3 && j%4==2)) tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 256;							
							}
						}
					else{//far from diagonal 4x4, some cells differ at the 1nd pos
						if(i%4 == j%4){
							if(ii==jj){
								//the diagonal cells in this subsection are 1 nuc change away
								tempqmatLookup[64*(i*4+ii) + (j*4+jj)] = 1;
								if(abs(i-j) ==8)
									tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 2;
								if((i/4==0 && j/4==1) || (i/4==1 && j/4==0)) tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 8;
								else if((i/4==0 && j/4==2) || (i/4==2 && j/4==0)) tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 16;
								else if((i/4==0 && j/4==3) || (i/4==3 && j/4==0)) tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 32;
								else if((i/4==1 && j/4==2) || (i/4==2 && j/4==1)) tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 64;
								else if((i/4==1 && j/4==3) || (i/4==3 && j/4==1)) tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 128;
								else if((i/4==2 && j/4==3) || (i/4==3 && j/4==2)) tempqmatLookup[64*(i*4+ii) + (j*4+jj)] |= 256;	
								}
							}
						}
					}
				}
			}
		}
	//here are all of the NS changes for the universal code
tempqmatLookup[	1	]|=4;
tempqmatLookup[	3	]|=4;
tempqmatLookup[	4	]|=4;
tempqmatLookup[	8	]|=4;
tempqmatLookup[	12	]|=4;
tempqmatLookup[	16	]|=4;
tempqmatLookup[	32	]|=4;
tempqmatLookup[	64	]|=4;
tempqmatLookup[	66	]|=4;
tempqmatLookup[	69	]|=4;
tempqmatLookup[	73	]|=4;
tempqmatLookup[	77	]|=4;
tempqmatLookup[	81	]|=4;
tempqmatLookup[	97	]|=4;
tempqmatLookup[	113	]|=4;
tempqmatLookup[	129	]|=4;
tempqmatLookup[	131	]|=4;
tempqmatLookup[	134	]|=4;
tempqmatLookup[	138	]|=4;
tempqmatLookup[	142	]|=4;
tempqmatLookup[	146	]|=4;
tempqmatLookup[	162	]|=4;
tempqmatLookup[	192	]|=4;
tempqmatLookup[	194	]|=4;
tempqmatLookup[	199	]|=4;
tempqmatLookup[	203	]|=4;
tempqmatLookup[	207	]|=4;
tempqmatLookup[	211	]|=4;
tempqmatLookup[	227	]|=4;
tempqmatLookup[	243	]|=4;
tempqmatLookup[	256	]|=4;
tempqmatLookup[	264	]|=4;
tempqmatLookup[	268	]|=4;
tempqmatLookup[	276	]|=4;
tempqmatLookup[	292	]|=4;
tempqmatLookup[	308	]|=4;
tempqmatLookup[	321	]|=4;
tempqmatLookup[	329	]|=4;
tempqmatLookup[	333	]|=4;
tempqmatLookup[	341	]|=4;
tempqmatLookup[	357	]|=4;
tempqmatLookup[	373	]|=4;
tempqmatLookup[	386	]|=4;
tempqmatLookup[	394	]|=4;
tempqmatLookup[	398	]|=4;
tempqmatLookup[	406	]|=4;
tempqmatLookup[	422	]|=4;
tempqmatLookup[	438	]|=4;
tempqmatLookup[	451	]|=4;
tempqmatLookup[	459	]|=4;
tempqmatLookup[	463	]|=4;
tempqmatLookup[	471	]|=4;
tempqmatLookup[	487	]|=4;
tempqmatLookup[	503	]|=4;
tempqmatLookup[	512	]|=4;
tempqmatLookup[	516	]|=4;
tempqmatLookup[	521	]|=4;
tempqmatLookup[	523	]|=4;
tempqmatLookup[	524	]|=4;
tempqmatLookup[	552	]|=4;
tempqmatLookup[	577	]|=4;
tempqmatLookup[	581	]|=4;
tempqmatLookup[	584	]|=4;
tempqmatLookup[	586	]|=4;
tempqmatLookup[	589	]|=4;
tempqmatLookup[	601	]|=4;
tempqmatLookup[	617	]|=4;
tempqmatLookup[	633	]|=4;
tempqmatLookup[	642	]|=4;
tempqmatLookup[	646	]|=4;
tempqmatLookup[	649	]|=4;
tempqmatLookup[	651	]|=4;
tempqmatLookup[	654	]|=4;
tempqmatLookup[	682	]|=4;
tempqmatLookup[	698	]|=4;
tempqmatLookup[	707	]|=4;
tempqmatLookup[	711	]|=4;
tempqmatLookup[	712	]|=4;
tempqmatLookup[	714	]|=4;
tempqmatLookup[	719	]|=4;
tempqmatLookup[	731	]|=4;
tempqmatLookup[	747	]|=4;
tempqmatLookup[	763	]|=4;
tempqmatLookup[	768	]|=4;
tempqmatLookup[	772	]|=4;
tempqmatLookup[	776	]|=4;
tempqmatLookup[	782	]|=4;
tempqmatLookup[	796	]|=4;
tempqmatLookup[	812	]|=4;
tempqmatLookup[	828	]|=4;
tempqmatLookup[	833	]|=4;
tempqmatLookup[	837	]|=4;
tempqmatLookup[	841	]|=4;
tempqmatLookup[	846	]|=4;
tempqmatLookup[	861	]|=4;
tempqmatLookup[	877	]|=4;
tempqmatLookup[	893	]|=4;
tempqmatLookup[	898	]|=4;
tempqmatLookup[	902	]|=4;
tempqmatLookup[	906	]|=4;
tempqmatLookup[	908	]|=4;
tempqmatLookup[	909	]|=4;
tempqmatLookup[	911	]|=4;
tempqmatLookup[	926	]|=4;
tempqmatLookup[	942	]|=4;
tempqmatLookup[	958	]|=4;
tempqmatLookup[	963	]|=4;
tempqmatLookup[	967	]|=4;
tempqmatLookup[	971	]|=4;
tempqmatLookup[	974	]|=4;
tempqmatLookup[	991	]|=4;
tempqmatLookup[	1007	]|=4;
tempqmatLookup[	1023	]|=4;
tempqmatLookup[	1024	]|=4;
tempqmatLookup[	1041	]|=4;
tempqmatLookup[	1043	]|=4;
tempqmatLookup[	1044	]|=4;
tempqmatLookup[	1048	]|=4;
tempqmatLookup[	1052	]|=4;
tempqmatLookup[	1056	]|=4;
tempqmatLookup[	1089	]|=4;
tempqmatLookup[	1104	]|=4;
tempqmatLookup[	1106	]|=4;
tempqmatLookup[	1109	]|=4;
tempqmatLookup[	1113	]|=4;
tempqmatLookup[	1117	]|=4;
tempqmatLookup[	1121	]|=4;
tempqmatLookup[	1137	]|=4;
tempqmatLookup[	1154	]|=4;
tempqmatLookup[	1169	]|=4;
tempqmatLookup[	1171	]|=4;
tempqmatLookup[	1174	]|=4;
tempqmatLookup[	1178	]|=4;
tempqmatLookup[	1182	]|=4;
tempqmatLookup[	1186	]|=4;
tempqmatLookup[	1219	]|=4;
tempqmatLookup[	1232	]|=4;
tempqmatLookup[	1234	]|=4;
tempqmatLookup[	1239	]|=4;
tempqmatLookup[	1243	]|=4;
tempqmatLookup[	1247	]|=4;
tempqmatLookup[	1251	]|=4;
tempqmatLookup[	1267	]|=4;
tempqmatLookup[	1284	]|=4;
tempqmatLookup[	1296	]|=4;
tempqmatLookup[	1304	]|=4;
tempqmatLookup[	1308	]|=4;
tempqmatLookup[	1316	]|=4;
tempqmatLookup[	1332	]|=4;
tempqmatLookup[	1349	]|=4;
tempqmatLookup[	1361	]|=4;
tempqmatLookup[	1369	]|=4;
tempqmatLookup[	1373	]|=4;
tempqmatLookup[	1381	]|=4;
tempqmatLookup[	1397	]|=4;
tempqmatLookup[	1414	]|=4;
tempqmatLookup[	1426	]|=4;
tempqmatLookup[	1434	]|=4;
tempqmatLookup[	1438	]|=4;
tempqmatLookup[	1446	]|=4;
tempqmatLookup[	1462	]|=4;
tempqmatLookup[	1479	]|=4;
tempqmatLookup[	1491	]|=4;
tempqmatLookup[	1499	]|=4;
tempqmatLookup[	1503	]|=4;
tempqmatLookup[	1511	]|=4;
tempqmatLookup[	1527	]|=4;
tempqmatLookup[	1552	]|=4;
tempqmatLookup[	1556	]|=4;
tempqmatLookup[	1564	]|=4;
tempqmatLookup[	1576	]|=4;
tempqmatLookup[	1609	]|=4;
tempqmatLookup[	1617	]|=4;
tempqmatLookup[	1621	]|=4;
tempqmatLookup[	1629	]|=4;
tempqmatLookup[	1641	]|=4;
tempqmatLookup[	1657	]|=4;
tempqmatLookup[	1682	]|=4;
tempqmatLookup[	1686	]|=4;
tempqmatLookup[	1694	]|=4;
tempqmatLookup[	1706	]|=4;
tempqmatLookup[	1722	]|=4;
tempqmatLookup[	1739	]|=4;
tempqmatLookup[	1747	]|=4;
tempqmatLookup[	1751	]|=4;
tempqmatLookup[	1759	]|=4;
tempqmatLookup[	1771	]|=4;
tempqmatLookup[	1787	]|=4;
tempqmatLookup[	1804	]|=4;
tempqmatLookup[	1808	]|=4;
tempqmatLookup[	1812	]|=4;
tempqmatLookup[	1816	]|=4;
tempqmatLookup[	1836	]|=4;
tempqmatLookup[	1869	]|=4;
tempqmatLookup[	1873	]|=4;
tempqmatLookup[	1877	]|=4;
tempqmatLookup[	1881	]|=4;
tempqmatLookup[	1901	]|=4;
tempqmatLookup[	1917	]|=4;
tempqmatLookup[	1934	]|=4;
tempqmatLookup[	1938	]|=4;
tempqmatLookup[	1942	]|=4;
tempqmatLookup[	1946	]|=4;
tempqmatLookup[	1966	]|=4;
tempqmatLookup[	1999	]|=4;
tempqmatLookup[	2003	]|=4;
tempqmatLookup[	2007	]|=4;
tempqmatLookup[	2011	]|=4;
tempqmatLookup[	2031	]|=4;
tempqmatLookup[	2047	]|=4;
tempqmatLookup[	2048	]|=4;
tempqmatLookup[	2064	]|=4;
tempqmatLookup[	2081	]|=4;
tempqmatLookup[	2083	]|=4;
tempqmatLookup[	2084	]|=4;
tempqmatLookup[	2088	]|=4;
tempqmatLookup[	2092	]|=4;
tempqmatLookup[	2113	]|=4;
tempqmatLookup[	2129	]|=4;
tempqmatLookup[	2144	]|=4;
tempqmatLookup[	2146	]|=4;
tempqmatLookup[	2149	]|=4;
tempqmatLookup[	2153	]|=4;
tempqmatLookup[	2157	]|=4;
tempqmatLookup[	2161	]|=4;
tempqmatLookup[	2178	]|=4;
tempqmatLookup[	2194	]|=4;
tempqmatLookup[	2209	]|=4;
tempqmatLookup[	2211	]|=4;
tempqmatLookup[	2214	]|=4;
tempqmatLookup[	2218	]|=4;
tempqmatLookup[	2222	]|=4;
tempqmatLookup[	2243	]|=4;
tempqmatLookup[	2259	]|=4;
tempqmatLookup[	2272	]|=4;
tempqmatLookup[	2274	]|=4;
tempqmatLookup[	2279	]|=4;
tempqmatLookup[	2283	]|=4;
tempqmatLookup[	2287	]|=4;
tempqmatLookup[	2291	]|=4;
tempqmatLookup[	2308	]|=4;
tempqmatLookup[	2324	]|=4;
tempqmatLookup[	2336	]|=4;
tempqmatLookup[	2344	]|=4;
tempqmatLookup[	2348	]|=4;
tempqmatLookup[	2356	]|=4;
tempqmatLookup[	2373	]|=4;
tempqmatLookup[	2389	]|=4;
tempqmatLookup[	2401	]|=4;
tempqmatLookup[	2409	]|=4;
tempqmatLookup[	2413	]|=4;
tempqmatLookup[	2421	]|=4;
tempqmatLookup[	2438	]|=4;
tempqmatLookup[	2454	]|=4;
tempqmatLookup[	2466	]|=4;
tempqmatLookup[	2474	]|=4;
tempqmatLookup[	2478	]|=4;
tempqmatLookup[	2486	]|=4;
tempqmatLookup[	2503	]|=4;
tempqmatLookup[	2519	]|=4;
tempqmatLookup[	2531	]|=4;
tempqmatLookup[	2539	]|=4;
tempqmatLookup[	2543	]|=4;
tempqmatLookup[	2551	]|=4;
tempqmatLookup[	2568	]|=4;
tempqmatLookup[	2584	]|=4;
tempqmatLookup[	2592	]|=4;
tempqmatLookup[	2596	]|=4;
tempqmatLookup[	2604	]|=4;
tempqmatLookup[	2633	]|=4;
tempqmatLookup[	2649	]|=4;
tempqmatLookup[	2657	]|=4;
tempqmatLookup[	2661	]|=4;
tempqmatLookup[	2669	]|=4;
tempqmatLookup[	2681	]|=4;
tempqmatLookup[	2698	]|=4;
tempqmatLookup[	2714	]|=4;
tempqmatLookup[	2722	]|=4;
tempqmatLookup[	2726	]|=4;
tempqmatLookup[	2734	]|=4;
tempqmatLookup[	2746	]|=4;
tempqmatLookup[	2763	]|=4;
tempqmatLookup[	2779	]|=4;
tempqmatLookup[	2787	]|=4;
tempqmatLookup[	2791	]|=4;
tempqmatLookup[	2799	]|=4;
tempqmatLookup[	2811	]|=4;
tempqmatLookup[	2828	]|=4;
tempqmatLookup[	2844	]|=4;
tempqmatLookup[	2848	]|=4;
tempqmatLookup[	2852	]|=4;
tempqmatLookup[	2856	]|=4;
tempqmatLookup[	2876	]|=4;
tempqmatLookup[	2893	]|=4;
tempqmatLookup[	2909	]|=4;
tempqmatLookup[	2913	]|=4;
tempqmatLookup[	2917	]|=4;
tempqmatLookup[	2921	]|=4;
tempqmatLookup[	2941	]|=4;
tempqmatLookup[	2958	]|=4;
tempqmatLookup[	2974	]|=4;
tempqmatLookup[	2978	]|=4;
tempqmatLookup[	2982	]|=4;
tempqmatLookup[	2986	]|=4;
tempqmatLookup[	3006	]|=4;
tempqmatLookup[	3023	]|=4;
tempqmatLookup[	3039	]|=4;
tempqmatLookup[	3043	]|=4;
tempqmatLookup[	3047	]|=4;
tempqmatLookup[	3051	]|=4;
tempqmatLookup[	3071	]|=4;
tempqmatLookup[	3137	]|=4;
tempqmatLookup[	3153	]|=4;
tempqmatLookup[	3169	]|=4;
tempqmatLookup[	3189	]|=4;
tempqmatLookup[	3193	]|=4;
tempqmatLookup[	3197	]|=4;
tempqmatLookup[	3267	]|=4;
tempqmatLookup[	3283	]|=4;
tempqmatLookup[	3299	]|=4;
tempqmatLookup[	3319	]|=4;
tempqmatLookup[	3323	]|=4;
tempqmatLookup[	3327	]|=4;
tempqmatLookup[	3332	]|=4;
tempqmatLookup[	3348	]|=4;
tempqmatLookup[	3364	]|=4;
tempqmatLookup[	3388	]|=4;
tempqmatLookup[	3397	]|=4;
tempqmatLookup[	3413	]|=4;
tempqmatLookup[	3429	]|=4;
tempqmatLookup[	3441	]|=4;
tempqmatLookup[	3449	]|=4;
tempqmatLookup[	3453	]|=4;
tempqmatLookup[	3462	]|=4;
tempqmatLookup[	3478	]|=4;
tempqmatLookup[	3494	]|=4;
tempqmatLookup[	3514	]|=4;
tempqmatLookup[	3518	]|=4;
tempqmatLookup[	3527	]|=4;
tempqmatLookup[	3543	]|=4;
tempqmatLookup[	3559	]|=4;
tempqmatLookup[	3571	]|=4;
tempqmatLookup[	3579	]|=4;
tempqmatLookup[	3583	]|=4;
tempqmatLookup[	3657	]|=4;
tempqmatLookup[	3673	]|=4;
tempqmatLookup[	3689	]|=4;
tempqmatLookup[	3697	]|=4;
tempqmatLookup[	3701	]|=4;
tempqmatLookup[	3706	]|=4;
tempqmatLookup[	3709	]|=4;
tempqmatLookup[	3722	]|=4;
tempqmatLookup[	3738	]|=4;
tempqmatLookup[	3754	]|=4;
tempqmatLookup[	3766	]|=4;
tempqmatLookup[	3769	]|=4;
tempqmatLookup[	3771	]|=4;
tempqmatLookup[	3774	]|=4;
tempqmatLookup[	3787	]|=4;
tempqmatLookup[	3803	]|=4;
tempqmatLookup[	3819	]|=4;
tempqmatLookup[	3827	]|=4;
tempqmatLookup[	3831	]|=4;
tempqmatLookup[	3834	]|=4;
tempqmatLookup[	3839	]|=4;
tempqmatLookup[	3852	]|=4;
tempqmatLookup[	3884	]|=4;
tempqmatLookup[	3892	]|=4;
tempqmatLookup[	3901	]|=4;
tempqmatLookup[	3903	]|=4;
tempqmatLookup[	3917	]|=4;
tempqmatLookup[	3933	]|=4;
tempqmatLookup[	3949	]|=4;
tempqmatLookup[	3953	]|=4;
tempqmatLookup[	3957	]|=4;
tempqmatLookup[	3961	]|=4;
tempqmatLookup[	3964	]|=4;
tempqmatLookup[	3966	]|=4;
tempqmatLookup[	3982	]|=4;
tempqmatLookup[	4014	]|=4;
tempqmatLookup[	4022	]|=4;
tempqmatLookup[	4026	]|=4;
tempqmatLookup[	4029	]|=4;
tempqmatLookup[	4031	]|=4;
tempqmatLookup[	4047	]|=4;
tempqmatLookup[	4063	]|=4;
tempqmatLookup[	4079	]|=4;
tempqmatLookup[	4083	]|=4;
tempqmatLookup[	4087	]|=4;
tempqmatLookup[	4091	]|=4;
tempqmatLookup[	4092	]|=4;
tempqmatLookup[	4094	]|=4;
	
	int hskip, vskip=0;
	for(int i=0;i<61;i++){
		if(i==48||i+1==50||i+2==56) vskip++;
		hskip=0;
		for(int j=0;j<61;j++){
			if(i==26&&j==47){
				hskip=hskip+1 -1;
				}
			if(j==48||j+1==50||j+2==56) hskip++;
			qmatLookup[i*61+j]=tempqmatLookup[(i+vskip)*64+j+hskip];
			}
		}
	}


void Model::UpdateQMatCodon(){
//	ofstream dout("AAdist.log");

	double composition[61]={0.12, 0.484, 0.12, 0.484, 0.0727, 0.0727, 0.0727, 0.0727, 0.236, 0.516, 0.236, 0.516, 0, 0, 0, 0, 0.324, 0.211, 0.324, 0.211, 0.142, 0.142, 0.142, 0.142, 0.236, 0.236, 0.236, 0.236, 0, 0, 0, 0, 0.335, 0.502, 0.335, 0.502, 0, 0, 0, 0, 0.269, 0.269, 0.269, 0.269, 0, 0, 0, 0, 0.0727, 0.0727, 0.516, 0.516, 0.516, 0.516, 1, 0.0473, 1, 0, 0, 0, 0};
	double polarity[61]={0.79, 0.827, 0.79, 0.827, 0.457, 0.457, 0.457, 0.457, 0.691, 0.531, 0.691, 0.531, 0.037, 0.037, 0.0988, 0.037, 0.691, 0.679, 0.691, 0.679, 0.383, 0.383, 0.383, 0.383, 0.691, 0.691, 0.691, 0.691, 0, 0, 0, 0, 0.914, 1, 0.914, 1, 0.395, 0.395, 0.395, 0.395, 0.506, 0.506, 0.506, 0.506, 0.123, 0.123, 0.123, 0.123, 0.16, 0.16, 0.531, 0.531, 0.531, 0.531, 0.0741, 0.0617, 0.0741, 0, 0, 0, 0};
	double molvol[61]={0.695, 0.317, 0.695, 0.317, 0.347, 0.347, 0.347, 0.347, 0.725, 0.647, 0.725, 0.647, 0.647, 0.647, 0.611, 0.647, 0.491, 0.557, 0.491, 0.557, 0.177, 0.177, 0.177, 0.177, 0.725, 0.725, 0.725, 0.725, 0.647, 0.647, 0.647, 0.647, 0.479, 0.305, 0.479, 0.305, 0.168, 0.168, 0.168, 0.168, 0, 0, 0, 0, 0.485, 0.485, 0.485, 0.485, 0.796, 0.796, 0.647, 0.647, 0.647, 0.647, 0.311, 1, 0.311, 0.647, 0.772, 0.647, 0.772};
	for(int i=0;i<61;i++){
		for(int j=0;j<61;j++){
			if(qmatLookup[i*61+j]==0){//if two codons are >1 appart
				qmat[i][j]=0.0;
				}
			else{
				qmat[i][j] = *stateFreqs[j];
				if(nst == 2){
					if(qmatLookup[i*61+j] & 2){//if the difference is a transition
						qmat[i][j] *= *relNucRates[1];
						}
					}
				else if(nst == 6){
					if(qmatLookup[i*61+j] & 8) qmat[i][j] *= *relNucRates[0];
					else if(qmatLookup[i*61+j] & 16) qmat[i][j] *= *relNucRates[1];
					else if(qmatLookup[i*61+j] & 32) qmat[i][j] *= *relNucRates[2];
					else if(qmatLookup[i*61+j] & 64) qmat[i][j] *= *relNucRates[3];
					else if(qmatLookup[i*61+j] & 128) qmat[i][j] *= *relNucRates[4];
					else if(qmatLookup[i*61+j] & 256) qmat[i][j] *= *relNucRates[5];
					}
				if(qmatLookup[i*61+j]&4){
					//this is where omega or AA property stuff will go
					//double phi=pow(abs((composition[i]-scalerC->val)/(composition[j]-scalerC->val)),Vc->val);
					//double phi=pow(abs((polarity[i]-scalerC->val)/(polarity[j]-scalerC->val)),Vc->val);
					//double phi=pow(exp(abs(polarity[i]-polarity[j])),Vc->val);
					//double phi=pow(exp(abs(composition[i]-composition[j])),Vc->val);
					//double phi=pow(1-abs(polarity[i]-polarity[j]),Vc->val);

/*
					double compdist=abs(composition[i]-composition[j]);
					double poldist=abs(polarity[i]-polarity[j]);
					double voldist=abs(molvol[i]-molvol[j]);
					
					double comp, pol, vol;
*/
					/*
					//this is essentially Yang's geometric relationship for each distance separately
					//there is a separate 'b' variable for each property, and a single 'a'
					double comp=exp(-Vc->val*abs(composition[i]-composition[j]));
					double pol=exp(-Vc->val*abs(composition[i]-composition[j]));
					double vol=exp(-Vc->val*abs(composition[i]-composition[j]));
					qmat[i][j] *= comp;
					qmat[i][j] *= scalerC->val; //overall omega type thing 
					qmat[i][j] *= pol;
					qmat[i][j] *= vol;	
*/
					//DEBUG
					int nParams = 0;

					if(nParams==5){
						//this is essentially Yang's linear relationship for each distance separately
						//there is a separate 'b' variable for each property, and a single 'a'
/*						comp=(1-scalerC->val*compdist);
						pol=(1-scalerP->val*poldist);
						vol=(1-scalerM->val*voldist);
						qmat[i][j] *= comp;
						qmat[i][j] *= omega->val; //overall omega type thing 
						qmat[i][j] *= pol;
						qmat[i][j] *= vol;
*/						}

					if(nParams==0){
						qmat[i][j] *= *omegas[0]; //overall omega
						}

					if(nParams==1){
						//raw distances with overall omega
/*						comp=1.0-compdist;
						pol=1.0-poldist;
						vol=1.0-voldist;
						qmat[i][j] *= comp;
						qmat[i][j] *= omega->val; //overall omega
						qmat[i][j] *= pol;
						qmat[i][j] *= vol;
*/						}

					else if(nParams==4){
						//powered distances with overall omega
/*						comp=pow((1.001-compdist),powerC->val-1.0);
						pol=pow((1.001-poldist),powerP->val-1.0);
						vol=pow((1.001-voldist),powerM->val-1.0);
						qmat[i][j] *= comp;
						qmat[i][j] *= omega->val; //overall omega type thing 
						qmat[i][j] *= pol;
						qmat[i][j] *= vol;
*/						}

					else if(nParams==7){
						//powered distances with scalers and overall omega
/*						comp=pow(1-scalerC->val * abs(composition[i]-composition[j]),powerC->val);
						pol=pow(1-scalerP->val * abs(polarity[i]-polarity[j]),powerP->val);
						vol=pow(1-scalerM->val * abs(molvol[i]-molvol[j]),powerM->val);
						qmat[i][j] *= comp;
						qmat[i][j] *= omega->val; //overall omega type thing 
						qmat[i][j] *= pol;
						qmat[i][j] *= vol;
*/						}
				
/*					//raw distances with overall omega
					double comp=1-abs(composition[i]-composition[j]);
					double pol=1-abs(polarity[i]-polarity[j]);
					double vol=1-abs(molvol[i]-molvol[j]);
					qmat[i][j] *= comp;
					qmat[i][j] *= scalerC->val; //overall omega type thing 
					qmat[i][j] *= pol;
					qmat[i][j] *= vol;	
*/
/*					//raw distances converted to single euclidian and not rescaled to new max, with overall omega
					double eucdist=1 - sqrt(comp*comp + pol*pol + vol*vol);
					if(eucdist<0){
						eucdist=0;
						}
					qmat[i][j] *= eucdist;
					qmat[i][j] *= scalerC->val; //overall omega type thing 					
*/
/*
					//powered distances converted to a euclidian with overall omega, not rescaled
					double comp=pow(1-abs(composition[i]-composition[j]),scalerC->val);
					double pol=pow(1-abs(polarity[i]-polarity[j]),powerP->val);
					double vol=pow(1-abs(molvol[i]-molvol[j]),powerM->val);
					double eucdist=1 - sqrt((1-comp)*(1-comp) + (1-pol)*(1-pol) + (1-vol)*(1-vol));
					qmat[i][j] *= eucdist;
					qmat[i][j] *= Vc->val; //overall omega type thing 
*/					
/*					//powered distances converted to a euclidian with overall omega, rescaled to max
					double comp=pow(1-abs(composition[i]-composition[j]),scalerC->val);
					double pol=pow(1-abs(polarity[i]-polarity[j]),powerP->val);
					double vol=pow(1-abs(molvol[i]-molvol[j]),powerM->val);
					double eucdist=1 - (sqrt((1-comp)*(1-comp) + (1-pol)*(1-pol) + (1-vol)*(1-vol)))/sqrt(3.0);
					qmat[i][j] *= eucdist;
					qmat[i][j] *= Vc->val; //overall omega type thing 
*/
/*
					//raw distances converted to single euclidian rescaled to euc max, with overall omega
					double eucdist=1.0 - sqrt(comp*comp + pol*pol + vol*vol) / sqrt(3)); //make sure to rescale the max here
					qmat[i][j] *= eucdist;
					qmat[i][j] *= scalerC->val; //overall omega type thing
*/					
					//raw distances with estimated weights, converted to single euclidian, with overall omega
					//the weights are really only relative, with the composition weight fixed to 1
/*					//distance rescaled to new max value
					double eucdist=sqrt(comp*comp + powerP->val*pol*pol + powerM->val*vol*vol);
					double eucmax=sqrt(1 + powerP->val*powerP->val + powerM->val*powerM->val);
					qmat[i][j] *= 1.0 - eucdist/eucmax;
					qmat[i][j] *= scalerC->val; //overall omega type thing
*/
/*					//raw distances converted to single euclidian rescaled to euc max, with overall omega
					//also with a parameter like the 'b' in Yang's linear model
					double eucdist=1.0 - Vc->val*sqrt(comp*comp + pol*pol + vol*vol) / sqrt(3); //make sure to rescale the max here
					if(eucdist<0) eucdist=0.0;
					qmat[i][j] *= eucdist;
					qmat[i][j] *= scalerC->val; //overall omega type thing
*/
					//raw distances converted to single euclidian rescaled to euc max, with overall omega
/*					//raised to an estimated power
					double eucdist=pow(1.0 - sqrt(comp*comp + pol*pol + vol*vol) / sqrt(3), powerP->val); //make sure to rescale the max here
					if(eucdist<0) eucdist=0.0;
					qmat[i][j] *= eucdist;
					qmat[i][j] *= scalerC->val; //overall omega type thing
*/
/*
					//alternative forumulation allowing the curve to go above 1.
					//infers "intercept" and power
					//double eucdist=sqrt(comp*comp + pol*pol + vol*vol) / sqrt(3.0); //make sure to rescale the max here
					double eucdist=comp; //make sure to rescale the max here
					eucdist=pow((1.0-(eucdist)*(eucdist-powerP->val)), scalerC->val);
					qmat[i][j] *= eucdist;
					qmat[i][j] *= Vc->val; //overall omega type thing
*/
					//raw distances converted to single euclidian rescaled to euc max, with overall omega
/*					//raised to an estimated power, with a scaler (ie 'b' on the euc) <0 set to 0
					double eucdist=1.0 - powerP->val * sqrt(comp*comp + pol*pol + vol*vol) / sqrt(3); //make sure to rescale the max here
					if(eucdist<0){
						eucdist=0.0;
						}
					eucdist=pow(eucdist, Vc->val);
					qmat[i][j] *= eucdist;
					qmat[i][j] *= scalerC->val; //overall omega type thing
*/
					//raw distances with estimated weights, converted to single euclidian, with overall omega
					//the weights are really only relative, with the composition weight fixed to 1
					//distance rescaled to new max value, also with the b of Yang
/*					double eucdist=scalerC->val * sqrt(comp*comp + powerP->val*pol*pol + powerM->val*vol*vol);
					double eucmax=sqrt(1 + powerP->val*powerP->val + powerM->val*powerM->val);
					eucdist = 1.0 - eucdist/eucmax;
					if(eucdist<0) eucdist=0.0;
					qmat[i][j] *= eucdist;
					qmat[i][j] *= Vc->val; //overall omega type thing
*/
					//raw distances with estimated weights, converted to single euclidian, with overall omega
					//the weights are really only relative, with the composition weight fixed to 1
/*					//distance rescaled to new max value, also with the b of Yang
					double eucdist=scalerC->val * sqrt(comp*comp + powerP->val*pol*pol + powerM->val*vol*vol);
					double eucmax=sqrt(1 + powerP->val*powerP->val + powerM->val*powerM->val);
					eucdist=1.0 - eucdist/eucmax;
					if(eucdist<0) eucdist=0.0;
					eucdist = pow(eucdist, scalerP->val);
					qmat[i][j] *= eucdist;
					qmat[i][j] *= Vc->val; //overall omega type thing
*/
					/*
					//this is called "3propPowerDenom"
					double comp=pow(1-(abs(composition[i]-composition[j])/(1+scalerC->val)),Vc->val);
					double pol=pow(1-(abs(polarity[i]-polarity[j])/(1+scalerP->val)),powerP->val);
					double vol=pow(1-(abs(molvol[i]-molvol[j])/(1+scalerM->val)),powerM->val);
					qmat[i][j] *= comp;
					qmat[i][j] *= pol;
					qmat[i][j] *= vol;
					*/
					}
				}
			}
		}

	//set diags to sum rows to 0
	double sum;
	for(int x=0;x<nstates;x++){
		sum=0.0;
		for(int y=0;y<nstates;y++){
			if(x!=y) sum+=qmat[x][y];
			}
		qmat[x][x]=-sum;
		}
	}


void Model::UpdateQMatAminoAcid(){

	//here's the JTT model
	for(int from=0;from<20;from++)
		for(int to=0;to<20;to++)
			qmat[from][to] = *stateFreqs[to];

	if(modSpec.IsJonesAAMatrix()) MultiplyByJonesAAMatrix();
	else if(modSpec.IsDayhoffAAMatrix()) MultiplyByDayhoffAAMatrix();
	else if(modSpec.IsWAGAAMatrix()) MultiplyByWAGAAMatrix();
	
	for(int from=0;from<20;from++){
		qmat[from][from] = 0.0;
		for(int to=0;to<20;to++){
			if(from != to) qmat[from][from] -= qmat[from][to];
			}
		}
				
}

void Model::CalcEigenStuff(){
	//if rate params or basefreqs have been altered, requiring the recalculation of the eigenvectors and c_ijk
	if(modSpec.IsCodon()) UpdateQMatCodon();
	else if(modSpec.IsAminoAcid()||modSpec.IsCodonAminoAcid()) UpdateQMatAminoAcid();
	else UpdateQMat();

	memcpy(*tempqmat, *qmat, nstates*nstates*sizeof(FLOAT_TYPE));
	EigenRealGeneral(nstates, tempqmat, eigvals, eigvalsimag, eigvecs, iwork, work);

	memcpy(*teigvecs, *eigvecs, nstates*nstates*sizeof(FLOAT_TYPE));
	InvertMatrix(teigvecs, nstates, col, indx, inveigvecs);
	CalcCijk(c_ijk, nstates, (const FLOAT_TYPE**) eigvecs, (const FLOAT_TYPE**) inveigvecs);
	
	if(modSpec.IsNucleotide() == false){
		double diagsum=0;
		for(int i=0;i<nstates;i++)
			diagsum+=qmat[i][i];
		blen_multiplier=-nstates/diagsum;
		}
	else
		blen_multiplier=(ZERO_POINT_FIVE/((qmat[0][1]**stateFreqs[0])+(qmat[0][2]**stateFreqs[0])+(qmat[0][3]**stateFreqs[0])+(qmat[1][2]**stateFreqs[1])+(qmat[1][3]**stateFreqs[1])+(qmat[2][3]**stateFreqs[2])));
	eigenDirty=false;
	}

void Model::CalcPmat(FLOAT_TYPE blen, FLOAT_TYPE *metaPmat, bool flip /*=false*/){
	ProfCalcPmat.Start();
	assert(flip == false);
	//DEBUG
	if(modSpec.IsNucleotide() == false){
		CalcPmatNState(blen, metaPmat);
		ProfCalcPmat.Stop();
		return;
		}

	//this will be a wacky pmat calculation that combines the pmats for all of the rates
	//assert(blen>ZERO_POINT_ZERO && blen<10);
	//assuming 4 state for now
	FLOAT_TYPE tmpFreqs[4];
	for(int i=0;i<nstates;i++) tmpFreqs[i] = *stateFreqs[i];

	for(int r=0;r<NRateCats();r++){
		if(nst==6){
			if(eigenDirty==true)
				CalcEigenStuff();

			FLOAT_TYPE tempblen;
			if(NoPinvInModel()==true || modSpec.flexRates==true)//if we're using flex rates, pinv should already be included
				//in the rate normalization, and doesn't need to be figured in here
				tempblen=(blen * blen_multiplier * rateMults[r]);
			else
				tempblen=(blen * blen_multiplier * rateMults[r]) / (ONE_POINT_ZERO-*propInvar);
			
			CalcPij(c_ijk, nstates, eigvals, 1, tempblen, pmat[0], EigValexp);
			}
		else if(nst==2 || modSpec.equalStateFreqs == false){
			//remember that relNucRates[1] is kappa for nst=2 models
			FLOAT_TYPE PI, A, K=*relNucRates[1];
			FLOAT_TYPE R=tmpFreqs[0]+tmpFreqs[2];
			FLOAT_TYPE Y=ONE_POINT_ZERO - R;
			blen_multiplier=(ZERO_POINT_FIVE/((R*Y)+K*((tmpFreqs[0])*((tmpFreqs[2]))+(tmpFreqs[1])*((tmpFreqs[3])))));
			FLOAT_TYPE tempblen ;
			if(NoPinvInModel()==true || modSpec.flexRates==true)//if we're using flex rates, pinv should already be included
				//in the rate normalization, and doesn't need to be figured in here
				tempblen=(blen * blen_multiplier * rateMults[r]);
			else
				tempblen=(blen * blen_multiplier * rateMults[r]) / (ONE_POINT_ZERO-*propInvar);
			FLOAT_TYPE expblen=exp(-tempblen);

			for(register int f=0;f<4;f++){
				for(register int t=0;t<4;t++){	
					if(f==t){
						if(t==0||t==2) PI = R;
						else PI = Y;
						A=ONE_POINT_ZERO + PI * (K - ONE_POINT_ZERO);
						(**pmat)[f*4+t]=(tmpFreqs[t])+(tmpFreqs[t])*((ONE_POINT_ZERO/PI)-ONE_POINT_ZERO)*expblen+((PI-(tmpFreqs[t]))/PI)*exp(-A*tempblen);
						assert((**pmat)[f*4+t] > ZERO_POINT_ZERO);
						assert((**pmat)[f*4+t] < ONE_POINT_ZERO);
						}
					else if((f+t)%2){
						(**pmat)[f*4+t]=((tmpFreqs[t]))*(ONE_POINT_ZERO-expblen);//tranversion
						assert((**pmat)[f*4+t] > ZERO_POINT_ZERO);
						assert((**pmat)[f*4+t] < ONE_POINT_ZERO);
						}
					else{
						if(t==0||t==2) PI=R;
						else PI = Y;
						A=ONE_POINT_ZERO + PI * (K - ONE_POINT_ZERO);
						(**pmat)[f*4+t]=(tmpFreqs[t])+(tmpFreqs[t])*((ONE_POINT_ZERO/PI)-ONE_POINT_ZERO)*expblen-((tmpFreqs[t])/PI)*exp(-A*tempblen);//transition
						assert((**pmat)[f*4+t] > ZERO_POINT_ZERO);
						assert((**pmat)[f*4+t] < ONE_POINT_ZERO);
						}
					}
				}
			}
		else if(nst==1){
			blen_multiplier=(FLOAT_TYPE)(4.0/3.0);
			//	}
			FLOAT_TYPE tempblen ;
			if(NoPinvInModel()==true || modSpec.flexRates==true)//if we're using flex rates, pinv should already be included
				//in the rate normalization, and doesn't need to be figured in here
				tempblen=(blen * blen_multiplier * rateMults[r]);
			else
				tempblen=(blen * blen_multiplier * rateMults[r]) / (ONE_POINT_ZERO-*propInvar);
			FLOAT_TYPE expblen=exp(-tempblen);			
			for(register int f=0;f<4;f++){
				for(register int t=0;t<4;t++){
					if(f==t)
						(**pmat)[f*4+t]=expblen+(FLOAT_TYPE) 0.25*(ONE_POINT_ZERO-expblen);
					else 
						(**pmat)[f*4+t]=(FLOAT_TYPE)0.25*((ONE_POINT_ZERO)-expblen);
					}
				}
			}
	
		if(flip==true){
			//copy and flip the calculated pmat into the metaPmat
			for(int i=0;i<4;i++)
				for(int j=0;j<4;j++){
					metaPmat[i*16+j+r*4]=pmat[0][0][i+j*4];
					}
			}
		else{
			//Copy the pmats into the metaPmat in order
			for(int i=0;i<4;i++)
				for(int j=0;j<4;j++){
					metaPmat[i*4+j+r*16]=pmat[0][0][i*4+j];
					}
			}
		}	
	ProfCalcPmat.Stop();
	}	

void Model::CalcPmatNState(FLOAT_TYPE blen, FLOAT_TYPE *metaPmat){

	if(eigenDirty==true)
		CalcEigenStuff();

	for(int r=0;r<NRateCats();r++){
		FLOAT_TYPE tempblen;
		if(NoPinvInModel()==true || modSpec.flexRates==true)//if we're using flex rates, pinv should already be included
			//in the rate normalization, and doesn't need to be figured in here
			tempblen=(blen * blen_multiplier * rateMults[r]);
		else
			tempblen=(blen * blen_multiplier * rateMults[r]) / (ONE_POINT_ZERO-*propInvar);

		CalcPij(c_ijk, nstates, eigvals, 1, tempblen, pmat[0], EigValexp);

		//Copy the pmats into the metaPmat in order
		for(int i=0;i<nstates;i++)
			for(int j=0;j<nstates;j++)
				metaPmat[r*nstates*nstates + i*nstates + j]=pmat[0][0][i*nstates + j];
		}

/*	char filename[50];
	char temp[10];
	sprintf(temp, "%.2f.log", blen);	
	strcpy(filename, "pmatdebug");
	strcat(filename, temp);	
	*/
	
	//output pmat
/*	ofstream pmd(filename);
	for(int i=0;i<61;i++){
		for(int j=0;j<61;j++){
			pmd << p[i][j] << "\t";
			}
		pmd << endl;
		}
*/	}

void Model::CalcDerivatives(FLOAT_TYPE dlen, FLOAT_TYPE ***&pr, FLOAT_TYPE ***&one, FLOAT_TYPE ***&two){
	if(eigenDirty==true)
		CalcEigenStuff();

	for(int rate=0;rate<NRateCats();rate++){
		const unsigned rateOffset = nstates*rate; 
		for(int k=0; k<nstates; k++){
			FLOAT_TYPE scaledEigVal;
			if(NoPinvInModel()==true || modSpec.flexRates==true)//if we're using flex rates, pinv should already be included
				//in the rate normalization, and doesn't need to be figured in here
				scaledEigVal = eigvals[k]*rateMults[rate]*blen_multiplier;	
			else
				scaledEigVal = eigvals[k]*rateMults[rate]*blen_multiplier/(ONE_POINT_ZERO-*propInvar);

			EigValexp[k+rateOffset] = exp(scaledEigVal * dlen);
			EigValderiv[k+rateOffset] = scaledEigVal*EigValexp[k+rateOffset];
			EigValderiv2[k+rateOffset] = scaledEigVal*EigValderiv[k+rateOffset];
			}
		}

	for(int rate=0;rate<NRateCats();rate++)
		{
		const unsigned rateOffset = nstates*rate;
		for (int i = 0; i < nstates; i++){
			for (int j = 0; j < nstates; j++){
				FLOAT_TYPE sum_p=ZERO_POINT_ZERO;
				FLOAT_TYPE sum_d1p=ZERO_POINT_ZERO;
				FLOAT_TYPE sum_d2p = ZERO_POINT_ZERO;
				for (int k = 0; k < nstates; k++){ 
				//for (int k = 0; k < 4; k++){ 
					//const FLOAT_TYPE x = eigvecs[i][k]*inveigvecs[j][k];
					const FLOAT_TYPE x = eigvecs[i][k]*inveigvecs[k][j];
					sum_p   += x*EigValexp[k+rateOffset];
					sum_d1p += x*EigValderiv[k+rateOffset];
					sum_d2p += x*EigValderiv2[k+rateOffset];
					}

				pmat[rate][i][j] = (sum_p > ZERO_POINT_ZERO ? sum_p : ZERO_POINT_ZERO);
				deriv1[rate][i][j] = sum_d1p;
				deriv2[rate][i][j] = sum_d2p;
				}
			}
		}
	one=deriv1;
	two=deriv2;
	pr=pmat;
	}

/*	

			CalcPij(c_ijk, nstates, eigvals, 1, tempblen, p, EigValexp);
	{

	register int		nsq = n * n;
	FLOAT_TYPE				sum;
	const FLOAT_TYPE *ptr;
	FLOAT_TYPE *pMat = p[0];
	FLOAT_TYPE vr = v * r;
	FLOAT_TYPE *g = EigValexp;
	for (int k=0; k<n; k++)
		*g++ = exp(*eigenValues++ * vr);

	ptr = c_ijk;
#if 1
	for(int i=0; i<nsq; i++){
		g = EigValexp;
		sum = ZERO_POINT_ZERO;
		for(int k=0; k<n; k++)
			sum += (*ptr++) * (*g++);
		*pMat++ = (sum < ZERO_POINT_ZERO) ? ZERO_POINT_ZERO : sum;
		}
#else
	for(i=0; i<n; i++)
		{
		for(j=0; j<n; j++)
			{
			g = EigValexp;
			sum = ZERO_POINT_ZERO;
			for(k=0; k<n; k++)
				sum += (*ptr++) * (*g++);
			//p[i][j] = (sum < ZERO_POINT_ZERO) ? ZERO_POINT_ZERO : sum;
			*pMat++ = (sum < ZERO_POINT_ZERO) ? ZERO_POINT_ZERO : sum;
					}
				}
#endif

			}
		}
*/

void Model::SetDefaultModelParameters(const HKYData *data){
	//some of these depend on having read the data already
	//also note that this resets the values in the case of 
	//bootstrapping.  Any of this could be overridden by
	//values specified in a start file

	for(vector<BaseParameter*>::iterator pit=paramsToMutate.begin();pit != paramsToMutate.end();pit++){
		(*pit)->SetToDefaultValues();
		}
	if(modSpec.numRateCats > 1) DiscreteGamma(rateMults, rateProbs, *alpha);

	if(modSpec.equalStateFreqs == false){
		if(modSpec.IsCodon()){
			FLOAT_TYPE f[61];
			static_cast<const CodonData *>(data)->GetEmpiricalFreqs(f);
			SetPis(f, false);
			}
		else if(modSpec.IsAminoAcid() && (modSpec.empiricalStateFreqs == true || modSpec.fixStateFreqs == false)){
			FLOAT_TYPE f[20];
			(data)->GetEmpiricalFreqs(f);
			SetPis(f, false);			
			}
		else if(modSpec.IsCodonAminoAcid() && (modSpec.empiricalStateFreqs == true || modSpec.fixStateFreqs == false)){
			FLOAT_TYPE f[20];
			static_cast<const CodonData *>(data)->GetEmpiricalFreqs(f);
			SetPis(f, false);			
			}
		else if(modSpec.IsNucleotide()){
			FLOAT_TYPE f[4];
			data->GetEmpiricalFreqs(f);
			SetPis(f, false);
			}
		}

	if(modSpec.includeInvariantSites==false){
		SetPinv(ZERO_POINT_ZERO, false);
		SetMaxPinv(ZERO_POINT_ZERO);
		}
	else{
		//if there are no constant sites, warn user that Pinv should not be used
		if(data->NConstant() == 0) throw(ErrorException("This dataset contains no constant characters!\nInference of the proportion of invariant sites is therefore meaningless.\nPlease set invariantsites to \"none\""));
		SetPinv((FLOAT_TYPE)0.25 * ((FLOAT_TYPE)data->NConstant()/(data->NConstant()+data->NInformative()+data->NAutapomorphic())), false);
		SetMaxPinv((FLOAT_TYPE)data->NConstant()/(data->NConstant()+data->NInformative()+data->NAutapomorphic()));
		if(modSpec.flexRates == true) NormalizeRates();
		else AdjustRateProportions();
		}
	}

void Model::MutateRates(){
	//paramsToMutate[1]->Mutator(Model::mutationShape);
	//assert(Rates(0) == Rates(2));

/*	int rateToChange=int(rnd.uniform()*(nst));
	
	if(rateToChange<nst-1){
		rates[rateToChange] *= rnd.gamma( Model::mutationShape );
//		rates[rateToChange] *= exp(MODEL_CHANGE_SCALER * (params->rnd.uniform()-.5));
		if(rates[rateToChange]>99.9) rates[rateToChange]=99.9;
		}

	else{//if we alter the reference rate GT (fixed to 1.0)
		//scale all of the other rates
		//FLOAT_TYPE scaler=exp(MODEL_CHANGE_SCALER * (params->rnd.uniform()-.5));
		FLOAT_TYPE scaler= rnd.gamma( Model::mutationShape );
		for(int i=0;i<nst-1;i++){
			rates[i] /= scaler;
			}
		}

	// don't let rates[0] become greater than 99.0
	// if this upper limit is changed, be sure to check consequences
	// in AllocMigrantStrings function in gamlmain.C (i.e., currently
	// only allow 3 characters plus number needed for precision)
*/	eigenDirty=true;
	}

void Model::MutatePis(){
	assert(0);
	//basetest->Mutator(Model::mutationShape);
//	paramsToMutate[0]->Mutator(Model::mutationShape);
//	dirty=true;

	//alternative:change one pi with a multiplier and rescale the rest
/*	int piToChange=int(rnd.uniform()*4.0);
	
	FLOAT_TYPE newPi=pi[piToChange] * rnd.gamma( Model::mutationShape );
	for(int b=0;b<4;b++)
		if(b!=piToChange) pi[b] *= (1.0-newPi)/(1.0-pi[piToChange]);
	pi[piToChange]=newPi;
	dirty=true;
*/	}
/*
void Model::MutateRateProbs(){
	int ProbToChange=int(rnd.uniform()*(FLOAT_TYPE) NRateCats());
	
	FLOAT_TYPE newProb=rateProbs[ProbToChange] * rnd.gamma( Model::mutationShape / 10.0 );
	for(int b=0;b<NRateCats();b++)
		if(b!=ProbToChange) rateProbs[b] *= (1.0-newProb)/(1.0-rateProbs[ProbToChange]);
	rateProbs[ProbToChange]=newProb;
	NormalizeRates();
	}

void Model::MutateRateMults(){
	int rateToChange=int(rnd.uniform()*NRateCats());
	rateMults[rateToChange] *= rnd.gamma( Model::mutationShape / 10.0);
	NormalizeRates();
	}
	
void Model::MutateAlpha(){
//	alpha *= exp(MODEL_CHANGE_SCALER * (params->rnd.uniform()-.5));
	*alpha *=rnd.gamma( Model::mutationShape );
	DiscreteGamma(rateMults, rateProbs, *alpha);
	//change the proportion of rates in each gamma cat
	}
	
void Model::MutatePropInvar(){
//	propInvar *= exp(MODEL_CHANGE_SCALER * (params->rnd.uniform()-.5));
	FLOAT_TYPE mult=rnd.gamma( Model::mutationShape );
	if(*propInvar == maxPropInvar && (mult > 1.0)) mult=1.0/mult;
	*propInvar *= mult;
	*propInvar = (*propInvar > maxPropInvar ? maxPropInvar : *propInvar);
	//change the proportion of rates in each gamma cat
	for(int i=0;i<NRateCats();i++){
		rateProbs[i]=(1.0-*propInvar)/NRateCats();
		}
	}
*/
void Model::CopyModel(const Model *from){
	if(modSpec.IsCodon()){
		for(int i=0;i<omegas.size();i++)
			*omegas[i]=*(from->omegas[i]);
		for(int i=0;i<omegaProbs.size();i++)
			*omegaProbs[i]=*(from->omegaProbs[i]);
		}

	if(modSpec.IsAminoAcid() == false && modSpec.IsCodonAminoAcid() == false)
		for(int i=0;i<6;i++)
			*relNucRates[i]=*(from->relNucRates[i]);
	
	for(int i=0;i<nstates;i++)
		*stateFreqs[i]=*(from->stateFreqs[i]);

	//memcpy(pi, from->pi, sizeof(FLOAT_TYPE)*4);

	memcpy(rateMults, from->rateMults, sizeof(FLOAT_TYPE)*NRateCats());
	memcpy(rateProbs, from->rateProbs, sizeof(FLOAT_TYPE)*NRateCats());

	if(modSpec.flexRates == false && modSpec.numRateCats > 1)
		*alpha=*(from->alpha);
	*propInvar=*(from->propInvar);

	eigenDirty=true;
	}	

void Model::SetModel(FLOAT_TYPE *model_string){
	int slot=0;
	for(int i=0;i<nst-1;i++)
		*relNucRates[i]=model_string[slot++];
	for(int j=0;j<4;j++)
		*stateFreqs[j]=model_string[slot++];
		
	if(NRateCats()>1) *alpha=model_string[slot++];
	DiscreteGamma(rateMults, rateProbs, *alpha);
	//using whether or not this individual had a PI of >0 in the first
	//place to decide whether we should expect one in the string.
	//Seems safe.
	if(*propInvar!=ZERO_POINT_ZERO) *propInvar=model_string[slot++];
	eigenDirty=true;
	}

FLOAT_TYPE Model::TRatio() const{
	FLOAT_TYPE numerator = *relNucRates[1] * ( *stateFreqs[0]**stateFreqs[2] + *stateFreqs[1]**stateFreqs[3] );
	FLOAT_TYPE denominator = ( *stateFreqs[0] + *stateFreqs[2] ) * ( *stateFreqs[1] + *stateFreqs[3] );
	return ( numerator / denominator );
	}

bool Model::IsModelEqual(const Model *other) const {
	//this will need to be generalized if other models are introduced
	for(int i=0;i<6;i++)
		if(*relNucRates[i]!=*(other->relNucRates[i])) return false;
	
	for(int i=0;i<nstates;i++)
		if(*stateFreqs[i]!=*(other->stateFreqs[i])) return false;

	if(rateMults[0] != other->rateMults[0]) return false;
	if(rateMults[1] != other->rateMults[1]) return false;
	if(rateMults[2] != other->rateMults[2]) return false;
	if(rateMults[3] != other->rateMults[3]) return false;
	
	if(rateProbs[0] != other->rateProbs[0]) return false;
	if(rateProbs[1] != other->rateProbs[1]) return false;
	if(rateProbs[2] != other->rateProbs[2]) return false;
	if(rateProbs[3] != other->rateProbs[3]) return false;

	if(alpha!=other->alpha) return false;
	if(propInvar!=other->propInvar) return false;

	return true;
	}

//a bunch of the gamma rate het machinery
//from MrBayes

/*-------------------------------------------------------------------------------
|                                                                               |
|  Discretization of gamma distribution with equal proportions in each          |
|  category.                                                                    |
|                                                                               |
-------------------------------------------------------------------------------*/ 
#ifdef SINGLE_PRECISION_FLOATS
#define POINTGAMMA(prob,alpha,beta) 		PointChi2(prob,2.0f*(alpha))/(2.0f*(beta))
#else
#define POINTGAMMA(prob,alpha,beta) 		PointChi2(prob,2.0*(alpha))/(2.0*(beta))
#endif

/* ------------------------------------------------------------------------------
|                                                                               |
|  Returns z so That Prob{x<z} = prob where x ~ N(0,1) and                      |
|  (1e-12) < prob < 1-(1e-12).  Returns (-9999) if in error.                    |
|                                                                               |
|  Odeh, R. E. and J. O. Evans.  1974.  The percentage points of the normal     |
|     distribution.  Applied Statistics, 22:96-97 (AS70)                        |
|                                                                               |
|  Newer methods:                                                               |
|                                                                               |
|  Wichura, M. J.  1988.  Algorithm AS 241: The percentage points of the        |
|     normal distribution.  37:477-484.                                         |
|  Beasley, JD & S. G. Springer.  1977.  Algorithm AS 111: The percentage       |
|     points of the normal distribution.  26:118-121.                           |
|                                                                               |
-------------------------------------------------------------------------------*/   
FLOAT_TYPE PointNormal (FLOAT_TYPE prob){
#ifdef SINGLE_PRECISION_FLOATS
	FLOAT_TYPE 		a0 = -0.322232431088f, a1 = -1.0f, a2 = -0.342242088547f, a3 = -0.0204231210245f,
 					a4 = -0.453642210148e-4f, b0 = 0.0993484626060f, b1 = 0.588581570495f,
 					b2 = 0.531103462366f, b3 = 0.103537752850f, b4 = 0.0038560700634f,
 					y, z = 0.0f, p = prob, p1;
#else
	FLOAT_TYPE 		a0 = -0.322232431088, a1 = -1.0, a2 = -0.342242088547, a3 = -0.0204231210245,
 					a4 = -0.453642210148e-4, b0 = 0.0993484626060, b1 = 0.588581570495,
 					b2 = 0.531103462366, b3 = 0.103537752850, b4 = 0.0038560700634,
 					y, z = 0, p = prob, p1;
#endif

	p1 = (p<ZERO_POINT_FIVE ? p : 1-p);
	if (p1<1e-20) 
	   return (-9999);
	y = sqrt (log(1/(p1*p1)));   
	z = y + ((((y*a4+a3)*y+a2)*y+a1)*y+a0) / ((((y*b4+b3)*y+b2)*y+b1)*y+b0);
	return (p<ZERO_POINT_FIVE ? -z : z);

}

/*-------------------------------------------------------------------------------
|                                                                               |
|  Returns the incomplete gamma ratio I(x,alpha) where x is the upper           |
|  limit of the integration and alpha is the shape parameter.  Returns (-1)     |
|  if in error.                                                                 |
|  LnGamma_alpha = ln(Gamma(alpha)), is almost redundant.                      |
|  (1) series expansion     if (alpha>x || x<=1)                                |
|  (2) continued fraction   otherwise                                           |
|                                                                               |
|  RATNEST FORTRAN by                                                           |
|  Bhattacharjee, G. P.  1970.  The incomplete gamma integral.  Applied         |
|     Statistics, 19:285-287 (AS32)                                             |
|                                                                               |
-------------------------------------------------------------------------------*/   
FLOAT_TYPE IncompleteGamma (FLOAT_TYPE x, FLOAT_TYPE alpha, FLOAT_TYPE LnGamma_alpha){
	int 			i;
#ifdef SINGLE_PRECISION_FLOATS
	FLOAT_TYPE 		p = alpha, g = LnGamma_alpha,
					accurate = 1e-8f, overflow = 1e30f,
					factor, gin = 0.0f, rn = 0.0f, a = 0.0f, b = 0.0f, an = 0.0f, 
					dif = 0.0f, term = 0.0f, pn[6];
#else
	FLOAT_TYPE 		p = alpha, g = LnGamma_alpha,
					accurate = 1e-8, overflow = 1e30,
					factor, gin = 0.0, rn = 0.0, a = 0.0, b = 0.0, an = 0.0, 
					dif = 0.0, term = 0.0, pn[6];
#endif

	if (x == ZERO_POINT_ZERO) 
		return (ZERO_POINT_ZERO);
	if (x < 0 || p <= 0) 
		return (-ONE_POINT_ZERO);

	factor = exp(p*log(x)-x-g);   
	if (x>1 && x>=p) 
		goto l30;
	gin = ONE_POINT_ZERO;  
	term = ONE_POINT_ZERO;  
	rn = p;
	l20:
		rn++;
		term *= x/rn;   
		gin += term;
		if (term > accurate) 
			goto l20;
		gin *= factor/p;
		goto l50;
	l30:
		a = ONE_POINT_ZERO-p;   
		b = a+x+ONE_POINT_ZERO;  
		term = ZERO_POINT_ZERO;
		pn[0] = ONE_POINT_ZERO;  
		pn[1] = x;  
		pn[2] = x+1;  
		pn[3] = x*b;
		gin = pn[2]/pn[3];
	l32:
		a++;  
		b += 2.0;  
		term++;   
		an = a*term;
		for (i=0; i<2; i++) 
			pn[i+4] = b*pn[i+2]-an*pn[i];
		if (pn[5] == 0) 
			goto l35;
		rn = pn[4]/pn[5];   
		dif = fabs(gin-rn);
		if (dif>accurate) 
			goto l34;
		if (dif<=accurate*rn) 
			goto l42;
	l34:
		gin = rn;
	l35:
		for (i=0; i<4; i++) 
			pn[i] = pn[i+2];
		if (fabs(pn[4]) < overflow) 
			goto l32;
		for (i=0; i<4; i++) 
			pn[i] /= overflow;
		goto l32;
	l42:
		gin = ONE_POINT_ZERO-factor*gin;
	l50:
		return (gin);

}

inline FLOAT_TYPE LnGamma (FLOAT_TYPE alp){
/*	FLOAT_TYPE cof[6];
	cof[0]=76.18009172947146;
    cof[1]=-86.50532032941677;
    cof[2]=24.01409824083091;
    cof[3]=-1.231739572450155;
    cof[4]=0.1208650973866179e-2;
    cof[5]=-0.5395239384953e-5;	
	FLOAT_TYPE xx=alp;
	FLOAT_TYPE yy=alp;
	FLOAT_TYPE tmp=xx + 5.5 - (xx + 0.5) * log(xx + 5.5);
	FLOAT_TYPE ser = 1.000000000190015;
	for(int j=0;j<5;j++){
		ser += (cof[j] / ++yy);
		}
	return log(2.5066282746310005*ser/xx)-tmp;
	}
*/
	FLOAT_TYPE x = alp, f=ZERO_POINT_ZERO, z;
	
	if (x < 7) 
		{
		f = ONE_POINT_ZERO;  
		z = x-ONE_POINT_ZERO;
		while (++z < 7.0)  
			f *= z;
		x = z;   
		f = -log(f);
		}
	z = ONE_POINT_ZERO/(x*x);
#ifdef SINGLE_PRECISION_FLOATS
	return  (f + (x-0.5f)*log(x) - x + 0.918938533204673f + 
			(((-0.000595238095238f*z+0.000793650793651f)*z-0.002777777777778f)*z +
			0.083333333333333f)/x);
#else
	return  (f + (x-0.5)*log(x) - x + 0.918938533204673 + 
			(((-0.000595238095238*z+0.000793650793651)*z-0.002777777777778)*z +
			0.083333333333333)/x);
#endif
	}

FLOAT_TYPE PointChi2 (FLOAT_TYPE prob, FLOAT_TYPE v){
#ifdef SINGLE_PRECISION_FLOATS
	//potential error e needs to be increased here
	//because of lesser decimal precision of floats
	FLOAT_TYPE 		e = 0.5e-4f, aa = 0.6931471805f, p = prob, g,
					xx, c, ch, a = 0.0f, q = 0.0f, p1 = 0.0f, p2 = 0.0f, t = 0.0f, 
					x = 0.0f, b = 0.0f, s1, s2, s3, s4, s5, s6;
	if (p < 0.000002f || p > 0.999998f || v <= 0.0f) 
		return (-1.0f);

	g = LnGamma (v*ZERO_POINT_FIVE);
	xx = v/2.0f;   
	c = xx - ONE_POINT_ZERO;
	if (v >= -1.24f*log(p)) 
		goto l1;
#else
	FLOAT_TYPE 		e = 0.5e-6, aa = 0.6931471805, p = prob, g,
					xx, c, ch, a = 0.0, q = 0.0, p1 = 0.0, p2 = 0.0, t = 0.0, 
					x = 0.0, b = 0.0, s1, s2, s3, s4, s5, s6;
	if (p < 0.000002 || p > 0.999998 || v <= 0.0) 
		return (-ONE_POINT_ZERO);
	
	g = LnGamma (v*ZERO_POINT_FIVE);
	xx = v/2.0;   
	c = xx - ONE_POINT_ZERO;
	if (v >= -1.24*log(p)) 
		goto l1;
#endif

	ch = pow((p*xx*exp(g+xx*aa)), ONE_POINT_ZERO/xx);
	if (ch-e < ZERO_POINT_ZERO) 
		return (ch);
	goto l4;
#ifdef SINGLE_PRECISION_FLOATS
	l1:
		if (v > 0.32f) 
			goto l3;
		ch = 0.4f;
		a = log(ONE_POINT_ZERO-p);
	l2:
		q = ch;  
		p1 = ONE_POINT_ZERO+ch*(4.67f+ch);  
		p2 = ch*(6.73f+ch*(6.66f+ch));
		t = -0.5f+(4.67f+2.0f*ch)/p1 - (6.73f+ch*(13.32f+3.0f*ch))/p2;
		ch -= (ONE_POINT_ZERO-exp(a+g+0.5f*ch+c*aa)*p2/p1)/t;
		if (fabs(q/ch-ONE_POINT_ZERO)-0.01f <= ZERO_POINT_ZERO) 
			goto l4;
		else                       
			goto l2;
	l3: 
		x = PointNormal (p);
		p1 =  0.222222f/v;   
		ch = v*pow((x*sqrt(p1)+ONE_POINT_ZERO-p1), 3.0f);
		if (ch > 2.2f*v+6.0f)  
			ch =  -2.0f*(log(ONE_POINT_ZERO-p)-c*log(0.5f*ch)+g);
#else
	l1:
		if (v > 0.32) 
			goto l3;
		ch = 0.4;
		a = log(ONE_POINT_ZERO-p);
	l2:
		q = ch;  
		p1 = ONE_POINT_ZERO+ch*(4.67+ch);  
		p2 = ch*(6.73+ch*(6.66+ch));
		t = -0.5+(4.67+2.0*ch)/p1 - (6.73+ch*(13.32+3.0*ch))/p2;
		ch -= (ONE_POINT_ZERO-exp(a+g+0.5*ch+c*aa)*p2/p1)/t;
		if (fabs(q/ch-ONE_POINT_ZERO)-0.01 <= ZERO_POINT_ZERO) 
			goto l4;
		else                       
			goto l2;
	l3: 
		x = PointNormal (p);
		p1 =  0.222222/v;   
		ch = v*pow((x*sqrt(p1)+ONE_POINT_ZERO-p1), 3.0);
		if (ch > 2.2*v+6.0)  
			ch =  -2.0*(log(ONE_POINT_ZERO-p)-c*log(0.5*ch)+g);
#endif
	l4:
		q = ch;
		p1 = ZERO_POINT_FIVE*ch;
		if ((t = IncompleteGamma (p1, xx, g)) < ZERO_POINT_ZERO) 
			{
			printf ("\nerr IncompleteGamma");
			return (-ONE_POINT_ZERO);
			}
		p2 = p-t;
		t = p2*exp(xx*aa+g+p1-c*log(ch));   
		b = t/ch;
#ifdef SINGLE_PRECISION_FLOATS
		a = 0.5f*t-b*c;
		s1 = (210.0f+a*(140.0f+a*(105.0f+a*(84.0f+a*(70.0f+60.0f*a))))) / 420.0f;
		s2 = (420.0f+a*(735.0f+a*(966.0f+a*(1141.0f+1278.0f*a))))/2520.0f;
		s3 = (210.0f+a*(462.0f+a*(707.0f+932.0f*a)))/2520.0f;
		s4 = (252.0f+a*(672.0f+1182.0f*a)+c*(294.0f+a*(889.0f+1740.0f*a)))/5040.0f;
		s5 = (84.0f+264.0f*a+c*(175.0f+606.0f*a))/2520.0f;
		s6 = (120.0f+c*(346.0f+127.0f*c))/5040.0f;
		ch += t*(1+0.5f*t*s1-b*c*(s1-b*(s2-b*(s3-b*(s4-b*(s5-b*s6))))));
#else
		a = 0.5*t-b*c;
		s1 = (210.0+a*(140.0+a*(105.0+a*(84.0+a*(70.0+60.0*a))))) / 420.0;
		s2 = (420.0+a*(735.0+a*(966.0+a*(1141.0+1278.0*a))))/2520.0;
		s3 = (210.0+a*(462.0+a*(707.0+932.0*a)))/2520.0;
		s4 = (252.0+a*(672.0+1182.0*a)+c*(294.0+a*(889.0+1740.0*a)))/5040.0;
		s5 = (84.0+264.0*a+c*(175.0+606.0*a))/2520.0;
		s6 = (120.0+c*(346.0+127.0*c))/5040.0;
		ch += t*(1+0.5*t*s1-b*c*(s1-b*(s2-b*(s3-b*(s4-b*(s5-b*s6))))));
#endif
		if (fabs(q/ch-ONE_POINT_ZERO) > e) 
			goto l4;
		return (ch);

}

//function taken from MB and hard wired for use here	
void Model::DiscreteGamma(FLOAT_TYPE *rates, FLOAT_TYPE *props, FLOAT_TYPE shape){
	bool median=false;
	int 	i;
	FLOAT_TYPE 	gap05 = ZERO_POINT_FIVE/NRateCats(), t, factor = shape/shape*NRateCats(), lnga1;

	if (median){
		for (i=0; i<NRateCats(); i++) 
			rates[i] = POINTGAMMA((i/ZERO_POINT_FIVE+ONE_POINT_ZERO)*gap05, shape, shape);
		for (i=0,t=0; i<NRateCats(); i++) 
			t += rates[i];
		for (i=0; i<NRateCats(); i++)     
			rates[i] *= factor/t;
		}
	else {
		lnga1 = LnGamma(shape+1);
		
		//DZ HACK
		//I don't think that these lines are needed, since the frequencies are fixed at .25 anyway.
		for (i=0; i<NRateCats()-1; i++) 
			props[i] = POINTGAMMA((i+ONE_POINT_ZERO)/NRateCats(), shape, shape);
		for (i=0; i<NRateCats()-1; i++) 
			props[i] = IncompleteGamma(props[i]*shape, shape+1, lnga1);
		
		
		rates[0] = props[0]*factor;
		rates[NRateCats()-1] = (1-props[NRateCats()-2])*factor;
		for (i=1; i<NRateCats()-1; i++)  
			rates[i] = (props[i]-props[i-1])*factor;
		}
	for (i=0; i<NRateCats(); i++) 
		props[i]=(ONE_POINT_ZERO-*propInvar)/NRateCats();
	}	
	
void Model::OutputPaupBlockForModel(ofstream &outf, const char *treefname) const{
	outf << "begin paup;\nclear;\ngett file=" << treefname << " storebr;\nlset userbr ";
	if(Nst() == 2) outf << "nst=2 trat= " << TRatio();
	else if(Nst() == 1) outf << "nst=1 ";
	else outf << "nst=6 rmat=(" << Rates(0) << " " << Rates(1) << " " << Rates(2) << " " << Rates(3) << " " << Rates(4) << ")";
	
	if(modSpec.equalStateFreqs == true) outf << " base=eq ";
	else if(modSpec.empiricalStateFreqs == true) outf << " base=emp ";
	else outf << " base=(" << StateFreq(0) << " " << StateFreq(1) << " " << StateFreq(2) << ")";
	
	if(modSpec.flexRates==false){
		if(NRateCats()>1) outf << " rates=gamma shape= " << Alpha() << " ncat=" << NRateCats();
		else outf << " rates=equal";
		outf << " pinv= " << PropInvar();
		outf << ";\nend;\n";
		}
	else{
		outf << " pinv= " << PropInvar();
		outf << " [FLEX RATES:\t";
		for(int i=0;i<NRateCats();i++){
			outf << rateMults[i] << "\t";
			outf << rateProbs[i] << "\t";
			}
		outf << "];\nend;\n";
		outf << "[!THIS TREE INFERRED UNDER FLEX RATE MODEL WITH GARLI.\nNO COMPARABLE MODEL IS AVAILABLE IN PAUP!]" << endl;
		}
	}

void Model::FillPaupBlockStringForModel(string &str, const char *treefname) const{
	char temp[200];
	sprintf(temp, "begin paup;\nclear;\ngett file=%s storebr;\nlset userbr ", treefname);
	str += temp;
	if(Nst() == 2){
		sprintf(temp, "st=2 trat=%f ", TRatio());
		str += temp;
		}
	else if(Nst() == 1) str += "nst=1 ";
	else{
		sprintf(temp,"nst=6 rmat=(%f %f %f %f %f)", Rates(0), Rates(1), Rates(2), Rates(3), Rates(4));
		str += temp;
		}
	if(modSpec.equalStateFreqs == true) str +=" base=eq ";
	else if(modSpec.empiricalStateFreqs == true) str += " base=emp ";
	else{
		sprintf(temp," base=( %f %f %f)", StateFreq(0), StateFreq(1), StateFreq(2));
		str += temp;
		}

	if(modSpec.flexRates==false){
		if(NRateCats()>1){
			sprintf(temp, " rates=gamma shape=%f ncat=%d", Alpha(), NRateCats());
			str += temp;
			}
		else str += " rates=equal";
		sprintf(temp, " pinv=%f;\nend;\n", PropInvar());;
		str += temp;
		}
	else{
		sprintf(temp, " pinv=%f  [FLEX RATES:\t", PropInvar());
		str += temp;
		for(int i=0;i<NRateCats();i++){
			sprintf(temp, "%f\t%f\t", rateMults[i], rateProbs[i]);
			str += temp;
			}
		str += "];\nend;\n[!THIS TREE INFERRED UNDER FLEX RATE MODEL WITH GARLI.\nNO COMPARABLE MODEL IS AVAILABLE IN PAUP!]\n";
		}
	}

void Model::OutputGarliFormattedModel(ostream &outf) const{
	if(modSpec.IsCodon()){
		outf << "o ";
		for(int i=0;i<omegas.size();i++){
			outf << *omegas[i] << " " << *omegaProbs[i] << " ";
			}
		}

	if(modSpec.IsAminoAcid() == false && modSpec.IsCodonAminoAcid() == false)
		outf << " r " << Rates(0) << " " << Rates(1) << " " << Rates(2) << " " << Rates(3) << " " << Rates(4);
	outf << " e " ;
	for(int i=0;i<nstates;i++)
		outf << StateFreq(i) << " ";;
	
	if(modSpec.flexRates==true){
		outf << " f ";
		for(int i=0;i<NRateCats();i++){
			outf << " " << rateMults[i] << "\t";
			outf << rateProbs[i] << "\t";
			}
		}
	else{
		if(NRateCats()>1) outf << " a " << Alpha();
		}
	if(PropInvar()!=ZERO_POINT_ZERO) outf << " p " << PropInvar();
	outf << " ";
	}

void Model::FillGarliFormattedModelString(string &s) const{
	char temp[50];
	if(modSpec.IsCodon()){
		s += " o";
		for(int i=0;i<omegas.size();i++){
			sprintf(temp," %f %f", *omegas[i], *omegaProbs[i]);
			s += temp;
			}
		}
	if(modSpec.IsAminoAcid() == false && modSpec.IsCodonAminoAcid() == false){
		sprintf(temp," r %f %f %f %f %f", Rates(0), Rates(1), Rates(2), Rates(3), Rates(4));
		s += temp;
		}
	if(modSpec.IsNucleotide()){
		sprintf(temp," e %f %f %f %f", StateFreq(0), StateFreq(1), StateFreq(2), StateFreq(3));
		s += temp;
		}
	else{
		sprintf(temp," e ");
		for(int i=0;i<nstates;i++){
			sprintf(temp," %f ", StateFreq(i));
			s += temp;
			}
		}

	if(modSpec.flexRates==true){
		s += " f ";
		for(int i=0;i<NRateCats();i++){
			sprintf(temp, " %f %f ", rateMults[i], rateProbs[i]);
			s += temp;
			}
		}
	else{
		if(NRateCats()>1){
			sprintf(temp, " a %f", Alpha());
			s += temp;
			}
		}
	if(PropInvar()!=ZERO_POINT_ZERO){
		sprintf(temp, " p %f", PropInvar());
		s += temp;
		}
	s += " ";
	}

/*	
void Model::ReadModelFromFile(NexusToken &token){
	token.GetNextToken();

	do{
		if(token.Equals("r")){//rate parameters
			token.GetNextToken();
			FLOAT_TYPE r[5];
			for(int i=0;i<5;i++){
				r[i]=atof(token.GetToken().c_str());
				token.GetNextToken();
				}
			SetRmat(r);
			if(token.IsNumericalToken()) token.GetNextToken();//this is necessary incase GT is included
			}
		else if(token.Equals("b")){
			token.GetNextToken();
			FLOAT_TYPE b[3];
			for(int i=0;i<3;i++){
				b[i]=atof(token.GetToken().c_str());
				token.GetNextToken();
				}
			SetPis(b);
			if(token.IsNumericalToken()) token.GetNextToken();//this is necessary incase T is included						
			}
		else if(token.Equals("a")){
			token.GetNextToken();
			SetAlpha(atof(token.GetToken().c_str()));
			token.GetNextToken();
			}				
		else if(token.Equals("p")){
			token.GetNextToken();
			SetPinv(atof(token.GetToken().c_str()));
			token.GetNextToken();
			}
		else if(token.Begins("(") == false){
			token.GetNextToken();
			}
		}while(token.Begins("(") == false);
	UpdateQMat();
}
*/	
	

void Model::CreateModelFromSpecification(){
	nstates = modSpec.nstates;
	nst = modSpec.nst;
	
	//deal with rate het models
	propInvar = new FLOAT_TYPE;
	if(modSpec.includeInvariantSites){
		assert(modSpec.IsNucleotide());
		*propInvar=(FLOAT_TYPE)0.2;
		if(modSpec.fixInvariantSites == false){
			ProportionInvariant *pi = new ProportionInvariant("proportion invariant", (FLOAT_TYPE **) &propInvar);
			pi->SetWeight(1);
			paramsToMutate.push_back(pi);
			}			
		}
	else *propInvar=ZERO_POINT_ZERO;

	if(NRateCats() > 1){
		assert(modSpec.IsNucleotide() || modSpec.IsAminoAcid());
		alpha = new FLOAT_TYPE;
		*alpha = ZERO_POINT_FIVE;
		
		if(modSpec.flexRates == false){
			DiscreteGamma(rateMults, rateProbs, *alpha);
			if(modSpec.fixAlpha == false){
				AlphaShape *a= new AlphaShape("alpha", &alpha);
				a->SetWeight(1);
				paramsToMutate.push_back(a);
				}
			}
		else{
			//start the flex rates out being equivalent to
			//a gamma with alpha=.5
			DiscreteGamma(rateMults, rateProbs, ZERO_POINT_FIVE);
			if(modSpec.includeInvariantSites == true) NormalizeRates();

			vector<FLOAT_TYPE*> dummy;
			dummy.reserve(NRateCats());
			
			for(int i=0;i<NRateCats();i++)
				dummy.push_back(&rateProbs[i]);
			RateProportions *rateP=new RateProportions(&dummy[0], NRateCats());
			rateP->SetWeight((FLOAT_TYPE)NRateCats());
			paramsToMutate.push_back(rateP);

			dummy.clear();
			for(int i=0;i<NRateCats();i++)
				dummy.push_back(&rateMults[i]);
			RateMultipliers *rateM=new RateMultipliers(&dummy[0], NRateCats());
			rateM->SetWeight((FLOAT_TYPE)NRateCats());
			paramsToMutate.push_back(rateM);
			}
		}
	else{
		rateMults[0]=ONE_POINT_ZERO;
		rateProbs[0]=ONE_POINT_ZERO;
		alpha=NULL;
		}

	//deal with the state frequencies
	for(int i=0;i<nstates;i++){
		FLOAT_TYPE *f=new FLOAT_TYPE;
		*f=(ONE_POINT_ZERO/(FLOAT_TYPE) nstates);
		stateFreqs.push_back(f);
		}
	if(modSpec.equalStateFreqs == false && modSpec.fixStateFreqs == false){
		StateFrequencies *s=new StateFrequencies(&stateFreqs[0], nstates);
		s->SetWeight(nstates);
		paramsToMutate.push_back(s);
		}
	if(modSpec.IsAminoAcid() || modSpec.IsCodonAminoAcid()){
		if(modSpec.IsJonesAAFreqs()) SetJonesAAFreqs();
		if(modSpec.IsDayhoffAAFreqs()) SetDayhoffAAFreqs();
		if(modSpec.IsWAGAAFreqs()) SetWAGAAFreqs();
		}

	//deal with the relative rates

	if(modSpec.IsAminoAcid() == false && modSpec.IsCodonAminoAcid() == false){
		if(modSpec.nst==6){
			//make the transitions higher to begin with
			for(int i=0;i<6;i++){
				FLOAT_TYPE *d=new FLOAT_TYPE;
				relNucRates.push_back(d);
				}
			*relNucRates[0]=*relNucRates[2]=*relNucRates[3]=*relNucRates[5] = ONE_POINT_ZERO;
			*relNucRates[1]=*relNucRates[4] = 4.0;
			if(modSpec.fixRelativeRates == false){
				RelativeRates *r=new RelativeRates("Rate matrix", &relNucRates[0], 6);
				r->SetWeight(6);
				paramsToMutate.push_back(r);
				}
			}
		else if(modSpec.nst==2){
			FLOAT_TYPE *a=new FLOAT_TYPE;
			FLOAT_TYPE *b=new FLOAT_TYPE;
			*a=ONE_POINT_ZERO;
			*b=4.0;
			relNucRates.push_back(a);
			relNucRates.push_back(b);
			relNucRates.push_back(a);
			relNucRates.push_back(a);
			relNucRates.push_back(b);
			relNucRates.push_back(a);
			if(modSpec.fixRelativeRates == false){
				RelativeRates *r=new RelativeRates("Rate matrix", &b, 1);
				r->SetWeight(2);
				paramsToMutate.push_back(r);
				}
			}
		else if(modSpec.nst==1){
			FLOAT_TYPE *a=new FLOAT_TYPE;
			*a=ONE_POINT_ZERO;
			for(int i=0;i<6;i++)
				relNucRates.push_back(a);
			}
		}

	AllocateEigenVariables();//these need to be allocated regardless of
		//nst because I don't feel like simplifying the deriv calcs for simpler
		//models.  Pmat calcs for simpler models are simplified, and don't
		//require the Eigen stuff	

	if(modSpec.IsNucleotide()) UpdateQMat();
	else if(modSpec.IsCodon()){
		FLOAT_TYPE *d;
		for(int i=0;i<NOmegaCats();i++){
			d = new FLOAT_TYPE;
			*d = 0.1 * (FLOAT_TYPE) (i + 1);
			omegas.push_back(d);
			d = new FLOAT_TYPE;
			*d = 1.0 / (FLOAT_TYPE) (i + 1);
			omegaProbs.push_back(d);
			}

		if(NOmegaCats() > 1){
			RateProportions *omegaP=new RateProportions(&omegaProbs[0], NOmegaCats());
			omegaP->SetWeight((FLOAT_TYPE)NOmegaCats());
			paramsToMutate.push_back(omegaP);
			}
	
		
		RateMultipliers *omegaM=new RateMultipliers(&omegas[0], NOmegaCats());
		omegaM->SetWeight((FLOAT_TYPE)NOmegaCats());
		paramsToMutate.push_back(omegaM);
			
/*		FLOAT_TYPE *NS=new FLOAT_TYPE;
		*NS = 0.5;
		FLOAT_TYPE *S=new FLOAT_TYPE;
		*S = 1.0;
		omegas.push_back(NS);
		omegas.push_back(S);
		RelativeRates *o=new RelativeRates("Omega", &omega[0], 2);

		o->SetWeight(2);
		paramsToMutate.push_back(o);
*/
		qmatLookup = new int[nstates * nstates];
		FillQMatLookup();
		UpdateQMatCodon();
		}
	else UpdateQMatAminoAcid();

	eigenDirty=true;
	}

void Model::SetJonesAAFreqs(){
		*stateFreqs[0] =0.076748;
		*stateFreqs[14]=0.051691;
		*stateFreqs[11]=0.042645;
		*stateFreqs[2]=0.051544;
		*stateFreqs[1]=0.019803;
		*stateFreqs[13]=0.040752;
		*stateFreqs[3]=0.06183;
		*stateFreqs[5]=0.073152;
		*stateFreqs[6]=0.022944;
		*stateFreqs[7]=0.053761;
		*stateFreqs[9]=0.091904;
		*stateFreqs[8]=0.058676;
		*stateFreqs[10]=0.023826;
		*stateFreqs[4]=0.040126;
		*stateFreqs[12]=0.050901;
		*stateFreqs[15]=0.068765;
		*stateFreqs[16]=0.058565;
		*stateFreqs[18]=0.014261;
		*stateFreqs[19]=0.032101;
		*stateFreqs[17]=0.066005;
		}
		
void Model::SetDayhoffAAFreqs(){
	*stateFreqs[0]		=0.087127;
	*stateFreqs[14]	=0.040904;
	*stateFreqs[11]	=0.040432;
	*stateFreqs[2]		=0.046872;
	*stateFreqs[1]		=0.033474;
	*stateFreqs[13]	=0.038255;
	*stateFreqs[3]		=0.04953;
	*stateFreqs[5]		=0.088612;
	*stateFreqs[6]		=0.033618;
	*stateFreqs[7]		=0.036886;
	*stateFreqs[9]		=0.085357;
	*stateFreqs[8]		=0.080482;
	*stateFreqs[10]	=0.014753;
	*stateFreqs[4]		=0.039772;
	*stateFreqs[12]	=0.05068;
	*stateFreqs[15]	=0.069577;
	*stateFreqs[16]	=0.058542;
	*stateFreqs[18]	=0.010494;
	*stateFreqs[19]	=0.029916;
	*stateFreqs[17]	=0.064718;
	}		

void Model::SetWAGAAFreqs(){
	*stateFreqs[0]=0.0866279;
	*stateFreqs[14]=0.043972;
	*stateFreqs[11]=0.0390894;
	*stateFreqs[2]=0.0570451;
	*stateFreqs[1]=0.0193078;
	*stateFreqs[13]=0.0367281;
	*stateFreqs[3]=0.0580589;
	*stateFreqs[5]=0.0832518;
	*stateFreqs[6]=0.0244313;
	*stateFreqs[7]=0.048466;
	*stateFreqs[9]=0.086209;
	*stateFreqs[8]=0.0620286;
	*stateFreqs[10]=0.0195027;
	*stateFreqs[4]=0.0384319;
	*stateFreqs[12]=0.0457631;
	*stateFreqs[15]=0.0695179;
	*stateFreqs[16]=0.0610127;
	*stateFreqs[18]=0.0143859;
	*stateFreqs[19]=0.0352742;
	*stateFreqs[17]=0.0708956;
	}

int Model::PerformModelMutation(){
	if(paramsToMutate.empty()) return 0;
	BaseParameter *mut = SelectModelMutation();
	assert(mut != NULL);
	mut->Mutator(mutationShape);
	int retType;

	if(mut->Type() == RELATIVERATES){
		UpdateQMat();
		retType=Individual::rates;
		eigenDirty=true;
		}
	else if(mut->Type() == STATEFREQS){
		UpdateQMat();
		retType=Individual::pi;
		eigenDirty=true;
		}
	
	else if(mut->Type() == PROPORTIONINVARIANT){
		//this max checking should really be rolled into the parameter class
		*propInvar = (*propInvar > maxPropInvar ? maxPropInvar : *propInvar);
		//the non invariant rates need to be rescaled even if there is only 1
		if(modSpec.flexRates == false) AdjustRateProportions();
		else NormalizeRates();
		retType=Individual::pinv;
		}
	else if(mut->Type() == ALPHASHAPE){
		DiscreteGamma(rateMults, rateProbs, *alpha);
		retType=Individual::alpha;
		}
	else if(mut->Type() == RATEPROPS || mut->Type() == RATEMULTS){
		//DEBUG - for now omega muts come through here too
		if(modSpec.flexRates == true)
			NormalizeRates();
		retType=Individual::alpha;
		}
	
	return retType;
	}


BaseParameter *Model::SelectModelMutation(){
	CalcMutationProbsFromWeights();
	if(paramsToMutate.empty() == true) return NULL;
	FLOAT_TYPE r=rnd.uniform();
	vector<BaseParameter*>::iterator it;
	for(it=paramsToMutate.begin();it!=paramsToMutate.end();it++){
		if((*it)->GetProb() > r) return *it;
		}
	it--;
	return *it;
	}

void Model::CalcMutationProbsFromWeights(){
	FLOAT_TYPE tot=ZERO_POINT_ZERO, running=ZERO_POINT_ZERO;
	for(vector<BaseParameter*>::iterator it=paramsToMutate.begin();it!=paramsToMutate.end();it++){
		tot += (*it)->GetWeight();
		}
	for(vector<BaseParameter*>::iterator it=paramsToMutate.begin();it!=paramsToMutate.end();it++){
		running += (*it)->GetWeight() / tot;
		(*it)->SetProb(running);
		}
	}

/*
void Model::OutputBinaryFormattedModel(OUTPUT_CLASS &out) const{
	FLOAT_TYPE *r = new FLOAT_TYPE;
	for(int i=0;i<5;i++){
		*r = Rates(i);
		out.write((char *) r, sizeof(FLOAT_TYPE));
		}
	for(int i=0;i<NStates();i++){
		*r = StateFreq(i);
		out.write((char *) r, sizeof(FLOAT_TYPE));
		}
	
	if(modSpec.flexRates==true){
		for(int i=0;i<NRateCats();i++){
			out.write((char *) &rateMults[i], sizeof(FLOAT_TYPE));
			out.write((char *) &rateProbs[i], sizeof(FLOAT_TYPE));
			}
		}
	else{
		if(NRateCats()>1){
			*r = Alpha();
			out.write((char *) r, sizeof(FLOAT_TYPE));
			}
		}
	if(PropInvar()!=ZERO_POINT_ZERO){
		*r = PropInvar();
		out.write((char *) r, sizeof(FLOAT_TYPE));
		}
	delete r;
	}
*/

void Model::OutputBinaryFormattedModel(OUTPUT_CLASS &out) const{
	FLOAT_TYPE *r = new FLOAT_TYPE;
	if(modSpec.IsAminoAcid() == false){
		for(int i=0;i<5;i++){
			*r = Rates(i);
			out.WRITE_TO_FILE(r, sizeof(FLOAT_TYPE), 1);
			}
		}
	//for codon models, output omega(s)
	if(modSpec.IsCodon()){
		for(int i=0;i<omegas.size();i++){
			*r = *omegas[i];
			out.WRITE_TO_FILE(r, sizeof(FLOAT_TYPE), 1);
			*r = *omegaProbs[i];
			out.WRITE_TO_FILE(r, sizeof(FLOAT_TYPE), 1);
			}
		}

	//these may not actually be free params, but output them anyway
	for(int i=0;i<NStates();i++){
		*r = StateFreq(i);
		out.WRITE_TO_FILE(r, sizeof(FLOAT_TYPE), 1);
		}
	
	if(modSpec.flexRates==true){
		for(int i=0;i<NRateCats();i++){
			out.WRITE_TO_FILE(&rateMults[i], sizeof(FLOAT_TYPE), 1);
			out.WRITE_TO_FILE(&rateProbs[i], sizeof(FLOAT_TYPE), 1);
			}
		}
	else{
		if(NRateCats()>1){
			*r = Alpha();
			out.WRITE_TO_FILE(r, sizeof(FLOAT_TYPE), 1);
			}
		}
	if(PropInvar()!=ZERO_POINT_ZERO){
		*r = PropInvar();
		out.WRITE_TO_FILE(r, sizeof(FLOAT_TYPE), 1);
		}
	delete r;
	}

void Model::ReadBinaryFormattedModel(FILE *in){
	if(modSpec.IsAminoAcid() == false){
		FLOAT_TYPE r[5];
		for(int i=0;i<5;i++){
			assert(ferror(in) == false);
			fread(r+i, sizeof(FLOAT_TYPE), 1, in);
			}
		SetRmat(r, false);
		}

	if(modSpec.IsCodon()){
		FLOAT_TYPE o;
		for(int i=0;i<omegas.size();i++){
			fread(&o, sizeof(FLOAT_TYPE), 1, in);
			*omegas[i] = o;
			fread(&o, sizeof(FLOAT_TYPE), 1, in);
			*omegaProbs[i] = o;
			}
		}	

	FLOAT_TYPE *b = new FLOAT_TYPE[NStates()];
	for(int i=0;i<NStates();i++){
		fread((char*) &(b[i]), sizeof(FLOAT_TYPE), 1, in);
		}
	SetPis(b, false);
	delete []b;

	if(modSpec.flexRates==true){
		for(int i=0;i<NRateCats();i++){
			fread((char*) &(rateMults[i]), sizeof(FLOAT_TYPE), 1, in);
			fread((char*) &(rateProbs[i]), sizeof(FLOAT_TYPE), 1, in);
			}
		}
	else{
		if(NRateCats()>1){
			FLOAT_TYPE a;
			assert(ferror(in) == false);
			fread((char*) &a, sizeof(FLOAT_TYPE), 1, in);
			SetAlpha(a, false);
			}
		}
	if(PropInvar()!=ZERO_POINT_ZERO){
		FLOAT_TYPE p;
		fread((char*) &p, sizeof(FLOAT_TYPE), 1, in);
		SetPinv(p, false);
		}
	}

void Model::MultiplyByJonesAAMatrix(){
	qmat[0][1] *= 0.056; qmat[1][0] *= 0.056; qmat[0][2] *= 0.081; qmat[2][0] *= 0.081; qmat[0][3] *= 0.105; qmat[3][0] *= 0.105; 
	qmat[0][4] *= 0.015; qmat[4][0] *= 0.015; qmat[0][5] *= 0.179; qmat[5][0] *= 0.179; qmat[0][6] *= 0.027; qmat[6][0] *= 0.027; 
	qmat[0][7] *= 0.036; qmat[7][0] *= 0.036; qmat[0][8] *= 0.035; qmat[8][0] *= 0.035; qmat[0][9] *= 0.03; qmat[9][0] *= 0.03; 
	qmat[0][10] *= 0.054; qmat[10][0] *= 0.054; qmat[0][11] *= 0.054; qmat[11][0] *= 0.054; qmat[0][12] *= 0.194; qmat[12][0] *= 0.194; 
	qmat[0][13] *= 0.057; qmat[13][0] *= 0.057; qmat[0][14] *= 0.058; qmat[14][0] *= 0.058; qmat[0][15] *= 0.378; qmat[15][0] *= 0.378; 
	qmat[0][16] *= 0.475; qmat[16][0] *= 0.475; qmat[0][17] *= 0.298; qmat[17][0] *= 0.298; qmat[0][18] *= 0.009; qmat[18][0] *= 0.009; 
	qmat[0][19] *= 0.011; qmat[19][0] *= 0.011; qmat[1][2] *= 0.01; qmat[2][1] *= 0.01; qmat[1][3] *= 0.005; qmat[3][1] *= 0.005; 
	qmat[1][4] *= 0.078; qmat[4][1] *= 0.078; qmat[1][5] *= 0.059; qmat[5][1] *= 0.059; qmat[1][6] *= 0.069; qmat[6][1] *= 0.069; 
	qmat[1][7] *= 0.017; qmat[7][1] *= 0.017; qmat[1][8] *= 0.007; qmat[8][1] *= 0.007; qmat[1][9] *= 0.023; qmat[9][1] *= 0.023; 
	qmat[1][10] *= 0.031; qmat[10][1] *= 0.031; qmat[1][11] *= 0.034; qmat[11][1] *= 0.034; qmat[1][12] *= 0.014; qmat[12][1] *= 0.014; 
	qmat[1][13] *= 0.009; qmat[13][1] *= 0.009; qmat[1][14] *= 0.113; qmat[14][1] *= 0.113; qmat[1][15] *= 0.223; qmat[15][1] *= 0.223; 
	qmat[1][16] *= 0.042; qmat[16][1] *= 0.042; qmat[1][17] *= 0.062; qmat[17][1] *= 0.062; qmat[1][18] *= 0.115; qmat[18][1] *= 0.115; 
	qmat[1][19] *= 0.209; qmat[19][1] *= 0.209; qmat[2][3] *= 0.767; qmat[3][2] *= 0.767; qmat[2][4] *= 0.004; qmat[4][2] *= 0.004; 
	qmat[2][5] *= 0.13; qmat[5][2] *= 0.13; qmat[2][6] *= 0.112; qmat[6][2] *= 0.112; qmat[2][7] *= 0.011; qmat[7][2] *= 0.011; 
	qmat[2][8] *= 0.026; qmat[8][2] *= 0.026; qmat[2][9] *= 0.007; qmat[9][2] *= 0.007; qmat[2][10] *= 0.015; qmat[10][2] *= 0.015; 
	qmat[2][11] *= 0.528; qmat[11][2] *= 0.528; qmat[2][12] *= 0.015; qmat[12][2] *= 0.015; qmat[2][13] *= 0.049; qmat[13][2] *= 0.049; 
	qmat[2][14] *= 0.016; qmat[14][2] *= 0.016; qmat[2][15] *= 0.059; qmat[15][2] *= 0.059; qmat[2][16] *= 0.038; qmat[16][2] *= 0.038; 
	qmat[2][17] *= 0.031; qmat[17][2] *= 0.031; qmat[2][18] *= 0.004; qmat[18][2] *= 0.004; qmat[2][19] *= 0.046; qmat[19][2] *= 0.046; 
	qmat[3][4] *= 0.005; qmat[4][3] *= 0.005; qmat[3][5] *= 0.119; qmat[5][3] *= 0.119; qmat[3][6] *= 0.026; qmat[6][3] *= 0.026; 
	qmat[3][7] *= 0.012; qmat[7][3] *= 0.012; qmat[3][8] *= 0.181; qmat[8][3] *= 0.181; qmat[3][9] *= 0.009; qmat[9][3] *= 0.009; 
	qmat[3][10] *= 0.018; qmat[10][3] *= 0.018; qmat[3][11] *= 0.058; qmat[11][3] *= 0.058; qmat[3][12] *= 0.018; qmat[12][3] *= 0.018; 
	qmat[3][13] *= 0.323; qmat[13][3] *= 0.323; qmat[3][14] *= 0.029; qmat[14][3] *= 0.029; qmat[3][15] *= 0.03; qmat[15][3] *= 0.03; 
	qmat[3][16] *= 0.032; qmat[16][3] *= 0.032; qmat[3][17] *= 0.045; qmat[17][3] *= 0.045; qmat[3][18] *= 0.01; qmat[18][3] *= 0.01; 
	qmat[3][19] *= 0.007; qmat[19][3] *= 0.007; qmat[4][5] *= 0.005; qmat[5][4] *= 0.005; qmat[4][6] *= 0.04; qmat[6][4] *= 0.04; 
	qmat[4][7] *= 0.089; qmat[7][4] *= 0.089; qmat[4][8] *= 0.004; qmat[8][4] *= 0.004; qmat[4][9] *= 0.248; qmat[9][4] *= 0.248; 
	qmat[4][10] *= 0.043; qmat[10][4] *= 0.043; qmat[4][11] *= 0.01; qmat[11][4] *= 0.01; qmat[4][12] *= 0.017; qmat[12][4] *= 0.017; 
	qmat[4][13] *= 0.004; qmat[13][4] *= 0.004; qmat[4][14] *= 0.005; qmat[14][4] *= 0.005; qmat[4][15] *= 0.092; qmat[15][4] *= 0.092; 
	qmat[4][16] *= 0.012; qmat[16][4] *= 0.012; qmat[4][17] *= 0.062; qmat[17][4] *= 0.062; qmat[4][18] *= 0.053; qmat[18][4] *= 0.053; 
	qmat[4][19] *= 0.536; qmat[19][4] *= 0.536; qmat[5][6] *= 0.023; qmat[6][5] *= 0.023; qmat[5][7] *= 0.006; qmat[7][5] *= 0.006; 
	qmat[5][8] *= 0.027; qmat[8][5] *= 0.027; qmat[5][9] *= 0.006; qmat[9][5] *= 0.006; qmat[5][10] *= 0.014; qmat[10][5] *= 0.014;
	qmat[5][11] *= 0.081; qmat[11][5] *= 0.081; qmat[5][12] *= 0.024; qmat[12][5] *= 0.024; qmat[5][13] *= 0.026; qmat[13][5] *= 0.026; 
	qmat[5][14] *= 0.137; qmat[14][5] *= 0.137; qmat[5][15] *= 0.201; qmat[15][5] *= 0.201; qmat[5][16] *= 0.033; qmat[16][5] *= 0.033; 
	qmat[5][17] *= 0.047; qmat[17][5] *= 0.047; qmat[5][18] *= 0.055; qmat[18][5] *= 0.055; qmat[5][19] *= 0.008; qmat[19][5] *= 0.008; 
	qmat[6][7] *= 0.016; qmat[7][6] *= 0.016; qmat[6][8] *= 0.045; qmat[8][6] *= 0.045; qmat[6][9] *= 0.056; qmat[9][6] *= 0.056; 
	qmat[6][10] *= 0.033; qmat[10][6] *= 0.033; qmat[6][11] *= 0.391; qmat[11][6] *= 0.391; qmat[6][12] *= 0.115; qmat[12][6] *= 0.115; 
	qmat[6][13] *= 0.597; qmat[13][6] *= 0.597; qmat[6][14] *= 0.328; qmat[14][6] *= 0.328; qmat[6][15] *= 0.073; qmat[15][6] *= 0.073; 
	qmat[6][16] *= 0.046; qmat[16][6] *= 0.046; qmat[6][17] *= 0.011; qmat[17][6] *= 0.011; qmat[6][18] *= 0.008; qmat[18][6] *= 0.008; 
	qmat[6][19] *= 0.573; qmat[19][6] *= 0.573; qmat[7][8] *= 0.021; qmat[8][7] *= 0.021; qmat[7][9] *= 0.229; qmat[9][7] *= 0.229; 
	qmat[7][10] *= 0.479; qmat[10][7] *= 0.479; qmat[7][11] *= 0.047; qmat[11][7] *= 0.047; qmat[7][12] *= 0.01; qmat[12][7] *= 0.01; 
	qmat[7][13] *= 0.009; qmat[13][7] *= 0.009; qmat[7][14] *= 0.022; qmat[14][7] *= 0.022; qmat[7][15] *= 0.04; qmat[15][7] *= 0.04; 
	qmat[7][16] *= 0.245; qmat[16][7] *= 0.245; qmat[7][17] *= 0.961; qmat[17][7] *= 0.961; qmat[7][18] *= 0.009; qmat[18][7] *= 0.009; 
	qmat[7][19] *= 0.032; qmat[19][7] *= 0.032; qmat[8][9] *= 0.014; qmat[9][8] *= 0.014; qmat[8][10] *= 0.065; qmat[10][8] *= 0.065; 
	qmat[8][11] *= 0.263; qmat[11][8] *= 0.263; qmat[8][12] *= 0.021; qmat[12][8] *= 0.021; qmat[8][13] *= 0.292; qmat[13][8] *= 0.292; 
	qmat[8][14] *= 0.646; qmat[14][8] *= 0.646; qmat[8][15] *= 0.047; qmat[15][8] *= 0.047; qmat[8][16] *= 0.103; qmat[16][8] *= 0.103; 
	qmat[8][17] *= 0.014; qmat[17][8] *= 0.014; qmat[8][18] *= 0.01; qmat[18][8] *= 0.01; qmat[8][19] *= 0.008; qmat[19][8] *= 0.008; 
	qmat[9][10] *= 0.388; qmat[10][9] *= 0.388; qmat[9][11] *= 0.012; qmat[11][9] *= 0.012; qmat[9][12] *= 0.102; qmat[12][9] *= 0.102; 
	qmat[9][13] *= 0.072; qmat[13][9] *= 0.072; qmat[9][14] *= 0.038; qmat[14][9] *= 0.038; qmat[9][15] *= 0.059; qmat[15][9] *= 0.059; 
	qmat[9][16] *= 0.025; qmat[16][9] *= 0.025; qmat[9][17] *= 0.18; qmat[17][9] *= 0.18; qmat[9][18] *= 0.052; qmat[18][9] *= 0.052; 
	qmat[9][19] *= 0.024; qmat[19][9] *= 0.024; qmat[10][11] *= 0.03; qmat[11][10] *= 0.03; qmat[10][12] *= 0.016; qmat[12][10] *= 0.016; 
	qmat[10][13] *= 0.043; qmat[13][10] *= 0.043; qmat[10][14] *= 0.044; qmat[14][10] *= 0.044; qmat[10][15] *= 0.029; qmat[15][10] *= 0.029; 
	qmat[10][16] *= 0.226; qmat[16][10] *= 0.226; qmat[10][17] *= 0.323; qmat[17][10] *= 0.323; qmat[10][18] *= 0.024; qmat[18][10] *= 0.024; 
	qmat[10][19] *= 0.018; qmat[19][10] *= 0.018; qmat[11][12] *= 0.015; qmat[12][11] *= 0.015; qmat[11][13] *= 0.086; qmat[13][11] *= 0.086; 
	qmat[11][14] *= 0.045; qmat[14][11] *= 0.045; qmat[11][15] *= 0.503; qmat[15][11] *= 0.503; qmat[11][16] *= 0.232; qmat[16][11] *= 0.232; 
	qmat[11][17] *= 0.016; qmat[17][11] *= 0.016; qmat[11][18] *= 0.008; qmat[18][11] *= 0.008; qmat[11][19] *= 0.07; qmat[19][11] *= 0.07; 
	qmat[12][13] *= 0.164; qmat[13][12] *= 0.164; qmat[12][14] *= 0.074; qmat[14][12] *= 0.074; qmat[12][15] *= 0.285; qmat[15][12] *= 0.285; 
	qmat[12][16] *= 0.118; qmat[16][12] *= 0.118; qmat[12][17] *= 0.023; qmat[17][12] *= 0.023; qmat[12][18] *= 0.006; qmat[18][12] *= 0.006; 
	qmat[12][19] *= 0.01; qmat[19][12] *= 0.01; qmat[13][14] *= 0.31; qmat[14][13] *= 0.31; qmat[13][15] *= 0.053; qmat[15][13] *= 0.053; 
	qmat[13][16] *= 0.051; qmat[16][13] *= 0.051; qmat[13][17] *= 0.02; qmat[17][13] *= 0.02; qmat[13][18] *= 0.018; qmat[18][13] *= 0.018; 
	qmat[13][19] *= 0.024; qmat[19][13] *= 0.024; qmat[14][15] *= 0.101; qmat[15][14] *= 0.101; qmat[14][16] *= 0.064; qmat[16][14] *= 0.064; 
	qmat[14][17] *= 0.017; qmat[17][14] *= 0.017; qmat[14][18] *= 0.126; qmat[18][14] *= 0.126; qmat[14][19] *= 0.02; qmat[19][14] *= 0.02; 
	qmat[15][16] *= 0.477; qmat[16][15] *= 0.477; qmat[15][17] *= 0.038; qmat[17][15] *= 0.038; qmat[15][18] *= 0.035; qmat[18][15] *= 0.035; 
	qmat[15][19] *= 0.063; qmat[19][15] *= 0.063; qmat[16][17] *= 0.112; qmat[17][16] *= 0.112; qmat[16][18] *= 0.012; qmat[18][16] *= 0.012; 
	qmat[16][19] *= 0.021; qmat[19][16] *= 0.021; qmat[17][18] *= 0.025; qmat[18][17] *= 0.025; qmat[17][19] *= 0.016; qmat[19][17] *= 0.016; 
	qmat[18][19] *= 0.071; qmat[19][18] *= 0.071;
	}

void Model::MultiplyByDayhoffAAMatrix(){
	qmat[0][1] *= 0.036; qmat[0][2] *= 0.12; qmat[0][3] *= 0.198; qmat[0][4] *= 0.018; qmat[0][5] *= 0.24; qmat[0][6] *= 0.023;
	qmat[0][7] *= 0.065; qmat[0][8] *= 0.026; qmat[0][9] *= 0.041; qmat[0][10] *= 0.072; qmat[0][11] *= 0.098; qmat[0][12] *= 0.25;
	qmat[0][13] *= 0.089; qmat[0][14] *= 0.027; qmat[0][15] *= 0.409; qmat[0][16] *= 0.371; qmat[0][17] *= 0.208; qmat[0][19] *= 0.024;
	qmat[1][0] *= 0.036; qmat[1][5] *= 0.011; qmat[1][6] *= 0.028; qmat[1][7] *= 0.044; qmat[1][12] *= 0.019; qmat[1][14] *= 0.023;
	qmat[1][15] *= 0.161; qmat[1][16] *= 0.016; qmat[1][17] *= 0.049; qmat[1][19] *= 0.096; qmat[2][0] *= 0.12; qmat[2][3] *= 1.153; 
	qmat[2][5] *= 0.125; qmat[2][6] *= 0.086; qmat[2][7] *= 0.024; qmat[2][8] *= 0.071; qmat[2][11] *= 0.905; qmat[2][12] *= 0.013; 
	qmat[2][13] *= 0.134; qmat[2][15] *= 0.095; qmat[2][16] *= 0.066; qmat[2][17] *= 0.018; qmat[3][0] *= 0.198; qmat[3][2] *= 1.153; 
	qmat[3][5] *= 0.081; qmat[3][6] *= 0.043; qmat[3][7] *= 0.061; qmat[3][8] *= 0.083; qmat[3][9] *= 0.011; qmat[3][10] *= 0.03; 
	qmat[3][11] *= 0.148; qmat[3][12] *= 0.051; qmat[3][13] *= 0.716; qmat[3][14] *= 0.001; qmat[3][15] *= 0.079; qmat[3][16] *= 0.034; 
	qmat[3][17] *= 0.037; qmat[3][19] *= 0.022; qmat[4][0] *= 0.018; qmat[4][5] *= 0.015; qmat[4][6] *= 0.048; qmat[4][7] *= 0.196; 
	qmat[4][9] *= 0.157; qmat[4][10] *= 0.092; qmat[4][11] *= 0.014; qmat[4][12] *= 0.011; qmat[4][14] *= 0.014; qmat[4][15] *= 0.046; 
	qmat[4][16] *= 0.013; qmat[4][17] *= 0.012; qmat[4][18] *= 0.076; qmat[4][19] *= 0.698; qmat[5][0] *= 0.24; qmat[5][1] *= 0.011; 
	qmat[5][2] *= 0.125; qmat[5][3] *= 0.081; qmat[5][4] *= 0.015; qmat[5][6] *= 0.01; qmat[5][8] *= 0.027; qmat[5][9] *= 0.007; 
	qmat[5][10] *= 0.017; qmat[5][11] *= 0.139; qmat[5][12] *= 0.034; qmat[5][13] *= 0.028; qmat[5][14] *= 0.009; qmat[5][15] *= 0.234; 
	qmat[5][16] *= 0.03; qmat[5][17] *= 0.054; qmat[6][0] *= 0.023; qmat[6][1] *= 0.028; qmat[6][2] *= 0.086; qmat[6][3] *= 0.043; 
	qmat[6][4] *= 0.048; qmat[6][5] *= 0.01; qmat[6][7] *= 0.007; qmat[6][8] *= 0.026; qmat[6][9] *= 0.044; qmat[6][11] *= 0.535; 
	qmat[6][12] *= 0.094; qmat[6][13] *= 0.606; qmat[6][14] *= 0.24; qmat[6][15] *= 0.035; qmat[6][16] *= 0.022; qmat[6][17] *= 0.044; 
	qmat[6][18] *= 0.027; qmat[6][19] *= 0.127; qmat[7][0] *= 0.065; qmat[7][1] *= 0.044; qmat[7][2] *= 0.024; qmat[7][3] *= 0.061; 
	qmat[7][4] *= 0.196; qmat[7][6] *= 0.007; qmat[7][8] *= 0.046; qmat[7][9] *= 0.257; qmat[7][10] *= 0.336; qmat[7][11] *= 0.077; 
	qmat[7][12] *= 0.012; qmat[7][13] *= 0.018; qmat[7][14] *= 0.064; qmat[7][15] *= 0.024; qmat[7][16] *= 0.192; qmat[7][17] *= 0.889; 
	qmat[7][19] *= 0.037; qmat[8][0] *= 0.026; qmat[8][2] *= 0.071; qmat[8][3] *= 0.083; qmat[8][5] *= 0.027; qmat[8][6] *= 0.026; 
	qmat[8][7] *= 0.046; qmat[8][9] *= 0.018; qmat[8][10] *= 0.243; qmat[8][11] *= 0.318; qmat[8][12] *= 0.033; qmat[8][13] *= 0.153; 
	qmat[8][14] *= 0.464; qmat[8][15] *= 0.096; qmat[8][16] *= 0.136; qmat[8][17] *= 0.01; qmat[8][19] *= 0.013; qmat[9][0] *= 0.041; 
	qmat[9][3] *= 0.011; qmat[9][4] *= 0.157; qmat[9][5] *= 0.007; qmat[9][6] *= 0.044; qmat[9][7] *= 0.257; qmat[9][8] *= 0.018; 
	qmat[9][10] *= 0.527; qmat[9][11] *= 0.034; qmat[9][12] *= 0.032; qmat[9][13] *= 0.073; qmat[9][14] *= 0.015; qmat[9][15] *= 0.017; 
	qmat[9][16] *= 0.033; qmat[9][17] *= 0.175; qmat[9][18] *= 0.046; qmat[9][19] *= 0.028; qmat[10][0] *= 0.072; qmat[10][3] *= 0.03; 
	qmat[10][4] *= 0.092; qmat[10][5] *= 0.017; qmat[10][7] *= 0.336; qmat[10][8] *= 0.243; qmat[10][9] *= 0.527; qmat[10][11] *= 0.001; 
	qmat[10][12] *= 0.017; qmat[10][13] *= 0.114; qmat[10][14] *= 0.09; qmat[10][15] *= 0.062; qmat[10][16] *= 0.104; qmat[10][17] *= 0.258;
	qmat[11][0] *= 0.098; qmat[11][2] *= 0.905; qmat[11][3] *= 0.148; qmat[11][4] *= 0.014; qmat[11][5] *= 0.139; qmat[11][6] *= 0.535; 
	qmat[11][7] *= 0.077; qmat[11][8] *= 0.318; qmat[11][9] *= 0.034; qmat[11][10] *= 0.001; qmat[11][12] *= 0.042; qmat[11][13] *= 0.103;
	qmat[11][14] *= 0.032; qmat[11][15] *= 0.495; qmat[11][16] *= 0.229; qmat[11][17] *= 0.015; qmat[11][18] *= 0.023; qmat[11][19] *= 0.095;
	qmat[12][0] *= 0.25; qmat[12][1] *= 0.019; qmat[12][2] *= 0.013; qmat[12][3] *= 0.051; qmat[12][4] *= 0.011; qmat[12][5] *= 0.034;
	qmat[12][6] *= 0.094; qmat[12][7] *= 0.012; qmat[12][8] *= 0.033; qmat[12][9] *= 0.032; qmat[12][10] *= 0.017; qmat[12][11] *= 0.042;
	qmat[12][13] *= 0.153; qmat[12][14] *= 0.103; qmat[12][15] *= 0.245; qmat[12][16] *= 0.078; qmat[12][17] *= 0.048; qmat[13][0] *= 0.089;
	qmat[13][2] *= 0.134; qmat[13][3] *= 0.716; qmat[13][5] *= 0.028; qmat[13][6] *= 0.606; qmat[13][7] *= 0.018; qmat[13][8] *= 0.153;
	qmat[13][9] *= 0.073; qmat[13][10] *= 0.114; qmat[13][11] *= 0.103; qmat[13][12] *= 0.153; qmat[13][14] *= 0.246; qmat[13][15] *= 0.056;
	qmat[13][16] *= 0.053; qmat[13][17] *= 0.035; qmat[14][0] *= 0.027; qmat[14][1] *= 0.023; qmat[14][3] *= 0.001; qmat[14][4] *= 0.014;
	qmat[14][5] *= 0.009; qmat[14][6] *= 0.24; qmat[14][7] *= 0.064; qmat[14][8] *= 0.464; qmat[14][9] *= 0.015; qmat[14][10] *= 0.09;
	qmat[14][11] *= 0.032; qmat[14][12] *= 0.103; qmat[14][13] *= 0.246; qmat[14][15] *= 0.154; qmat[14][16] *= 0.026; qmat[14][17] *= 0.024;
	qmat[14][18] *= 0.201; qmat[14][19] *= 0.008; qmat[15][0] *= 0.409; qmat[15][1] *= 0.161; qmat[15][2] *= 0.095; qmat[15][3] *= 0.079;
	qmat[15][4] *= 0.046; qmat[15][5] *= 0.234; qmat[15][6] *= 0.035; qmat[15][7] *= 0.024; qmat[15][8] *= 0.096; qmat[15][9] *= 0.017;
	qmat[15][10] *= 0.062; qmat[15][11] *= 0.495; qmat[15][12] *= 0.245; qmat[15][13] *= 0.056; qmat[15][14] *= 0.154; qmat[15][16] *= 0.55;
	qmat[15][17] *= 0.03; qmat[15][18] *= 0.075; qmat[15][19] *= 0.034; qmat[16][0] *= 0.371; qmat[16][1] *= 0.016; qmat[16][2] *= 0.066;
	qmat[16][3] *= 0.034; qmat[16][4] *= 0.013; qmat[16][5] *= 0.03; qmat[16][6] *= 0.022; qmat[16][7] *= 0.192; qmat[16][8] *= 0.136;
	qmat[16][9] *= 0.033; qmat[16][10] *= 0.104; qmat[16][11] *= 0.229; qmat[16][12] *= 0.078; qmat[16][13] *= 0.053; qmat[16][14] *= 0.026;
	qmat[16][15] *= 0.55; qmat[16][17] *= 0.157; qmat[16][19] *= 0.042; qmat[17][0] *= 0.208; qmat[17][1] *= 0.049; qmat[17][2] *= 0.018;
	qmat[17][3] *= 0.037; qmat[17][4] *= 0.012; qmat[17][5] *= 0.054; qmat[17][6] *= 0.044; qmat[17][7] *= 0.889; qmat[17][8] *= 0.01;
	qmat[17][9] *= 0.175; qmat[17][10] *= 0.258; qmat[17][11] *= 0.015; qmat[17][12] *= 0.048; qmat[17][13] *= 0.035; qmat[17][14] *= 0.024;
	qmat[17][15] *= 0.03; qmat[17][16] *= 0.157; qmat[17][19] *= 0.028; qmat[18][4] *= 0.076; qmat[18][6] *= 0.027; qmat[18][9] *= 0.046;
	qmat[18][11] *= 0.023; qmat[18][14] *= 0.201; qmat[18][15] *= 0.075; qmat[18][19] *= 0.061; qmat[19][0] *= 0.024; qmat[19][1] *= 0.096;
	qmat[19][3] *= 0.022; qmat[19][4] *= 0.698; qmat[19][6] *= 0.127; qmat[19][7] *= 0.037; qmat[19][8] *= 0.013; qmat[19][9] *= 0.028;
	qmat[19][11] *= 0.095; qmat[19][14] *= 0.008; qmat[19][15] *= 0.034; qmat[19][16] *= 0.042; qmat[19][17] *= 0.028; 
	qmat[19][18] *= 0.061;
	//here are the zero entries
	qmat[0][18]=qmat[18][0]=0.0;
	qmat[2][14]=qmat[14][2]=0.0;
	qmat[1][11]=qmat[11][1]=0.0;
	qmat[1][2]=qmat[2][1]=0.0;
	qmat[9][2]=qmat[2][9]=0.0;
	qmat[10][2]=qmat[2][10]=0.0;
	qmat[4][2]=qmat[2][4]=0.0;
	qmat[18][2]=qmat[2][18]=0.0;
	qmat[19][2]=qmat[2][19]=0.0;
	qmat[13][1]=qmat[1][13]=0.0;
	qmat[3][1]=qmat[1][3]=0.0;
	qmat[9][1]=qmat[1][9]=0.0;
	qmat[8][1]=qmat[1][8]=0.0;
	qmat[10][1]=qmat[1][10]=0.0;
	qmat[4][1]=qmat[1][4]=0.0;
	qmat[18][1]=qmat[1][18]=0.0;
	qmat[4][13]=qmat[13][4]=0.0;
	qmat[18][13]=qmat[13][18]=0.0;
	qmat[19][13]=qmat[13][19]=0.0;
	qmat[4][3]=qmat[3][4]=0.0;
	qmat[18][3]=qmat[3][18]=0.0;
	qmat[7][5]=qmat[5][7]=0.0;
	qmat[18][5]=qmat[5][18]=0.0;
	qmat[19][5]=qmat[5][19]=0.0;
	qmat[10][6]=qmat[6][10]=0.0;
	qmat[18][7]=qmat[7][18]=0.0;
	qmat[4][8]=qmat[8][4]=0.0;
	qmat[18][8]=qmat[8][18]=0.0;
	qmat[18][10]=qmat[10][18]=0.0;
	qmat[19][10]=qmat[10][19]=0.0;
	qmat[18][12]=qmat[12][18]=0.0;
	qmat[19][12]=qmat[12][19]=0.0;
	qmat[18][16]=qmat[16][18]=0.0;
	qmat[17][18]=qmat[18][17]=0.0;
	}

void Model::MultiplyByWAGAAMatrix(){
	qmat[14][0] *= 1.75252;
	qmat[0][14] *= 1.75252;
	qmat[11][0] *= 1.61995;
	qmat[0][11] *= 1.61995;
	qmat[11][14] *= 2.0187;
	qmat[14][11] *= 2.0187;
	qmat[2][0] *= 2.34804;
	qmat[0][2] *= 2.34804;
	qmat[2][14] *= 0.468033;
	qmat[14][2] *= 0.468033;
	qmat[2][11] *= 17.251;
	qmat[11][2] *= 17.251;
	qmat[1][0] *= 3.26324;
	qmat[0][1] *= 3.26324;
	qmat[1][14] *= 1.67824;
	qmat[14][1] *= 1.67824;
	qmat[1][11] *= 0.842805;
	qmat[11][1] *= 0.842805;
	qmat[1][2] *= 0.0962568;
	qmat[2][1] *= 0.0962568;
	qmat[13][0] *= 2.88691;
	qmat[0][13] *= 2.88691;
	qmat[13][14] *= 9.64477;
	qmat[14][13] *= 9.64477;
	qmat[13][11] *= 4.90465;
	qmat[11][13] *= 4.90465;
	qmat[13][2] *= 1.95972;
	qmat[2][13] *= 1.95972;
	qmat[13][1] *= 0.313977;
	qmat[1][13] *= 0.313977;
	qmat[3][0] *= 5.02923;
	qmat[0][3] *= 5.02923;
	qmat[3][14] *= 1.39535;
	qmat[14][3] *= 1.39535;
	qmat[3][11] *= 3.00956;
	qmat[11][3] *= 3.00956;
	qmat[3][2] *= 19.6173;
	qmat[2][3] *= 19.6173;
	qmat[3][1] *= 0.0678423;
	qmat[1][3] *= 0.0678423;
	qmat[3][13] *= 17.3783;
	qmat[13][3] *= 17.3783;
	qmat[5][0] *= 4.50138;
	qmat[0][5] *= 4.50138;
	qmat[5][14] *= 1.85767;
	qmat[14][5] *= 1.85767;
	qmat[5][11] *= 3.57627;
	qmat[11][5] *= 3.57627;
	qmat[5][2] *= 2.75024;
	qmat[2][5] *= 2.75024;
	qmat[5][1] *= 0.974403;
	qmat[1][5] *= 0.974403;
	qmat[5][13] *= 1.04868;
	qmat[13][5] *= 1.04868;
	qmat[5][3] *= 1.80382;
	qmat[3][5] *= 1.80382;
	qmat[6][0] *= 1.00707;
	qmat[0][6] *= 1.00707;
	qmat[6][14] *= 6.79042;
	qmat[14][6] *= 6.79042;
	qmat[6][11] *= 12.5704;
	qmat[11][6] *= 12.5704;
	qmat[6][2] *= 2.95706;
	qmat[2][6] *= 2.95706;
	qmat[6][1] *= 0.791065;
	qmat[1][6] *= 0.791065;
	qmat[6][13] *= 13.6438;
	qmat[13][6] *= 13.6438;
	qmat[6][3] *= 1.81116;
	qmat[3][6] *= 1.81116;
	qmat[6][5] *= 0.792457;
	qmat[5][6] *= 0.792457;
	qmat[7][0] *= 0.614288;
	qmat[0][7] *= 0.614288;
	qmat[7][14] *= 0.594093;
	qmat[14][7] *= 0.594093;
	qmat[7][11] *= 1.76099;
	qmat[11][7] *= 1.76099;
	qmat[7][2] *= 0.125304;
	qmat[2][7] *= 0.125304;
	qmat[7][1] *= 0.540574;
	qmat[1][7] *= 0.540574;
	qmat[7][13] *= 0.361952;
	qmat[13][7] *= 0.361952;
	qmat[7][3] *= 0.404776;
	qmat[3][7] *= 0.404776;
	qmat[7][5] *= 0.0967499;
	qmat[5][7] *= 0.0967499;
	qmat[7][6] *= 0.439075;
	qmat[6][7] *= 0.439075;
	qmat[9][0] *= 1.26431;
	qmat[0][9] *= 1.26431;
	qmat[9][14] *= 1.58126;
	qmat[14][9] *= 1.58126;
	qmat[9][11] *= 0.417907;
	qmat[11][9] *= 0.417907;
	qmat[9][2] *= 0.269452;
	qmat[2][9] *= 0.269452;
	qmat[9][1] *= 1.22101;
	qmat[1][9] *= 1.22101;
	qmat[9][13] *= 2.76265;
	qmat[13][9] *= 2.76265;
	qmat[9][3] *= 0.490144;
	qmat[3][9] *= 0.490144;
	qmat[9][5] *= 0.194782;
	qmat[5][9] *= 0.194782;
	qmat[9][6] *= 1.58695;
	qmat[6][9] *= 1.58695;
	qmat[9][7] *= 10.0752;
	qmat[7][9] *= 10.0752;
	qmat[8][0] *= 2.8795;
	qmat[0][8] *= 2.8795;
	qmat[8][14] *= 17.0032;
	qmat[14][8] *= 17.0032;
	qmat[8][11] *= 9.57014;
	qmat[11][8] *= 9.57014;
	qmat[8][2] *= 1.52466;
	qmat[2][8] *= 1.52466;
	qmat[8][1] *= 0.23523;
	qmat[1][8] *= 0.23523;
	qmat[8][13] *= 12.3754;
	qmat[13][8] *= 12.3754;
	qmat[8][3] *= 8.21158;
	qmat[3][8] *= 8.21158;
	qmat[8][5] *= 1.18692;
	qmat[5][8] *= 1.18692;
	qmat[8][6] *= 2.82919;
	qmat[6][8] *= 2.82919;
	qmat[8][7] *= 1.02892;
	qmat[7][8] *= 1.02892;
	qmat[8][9] *= 0.818336;
	qmat[9][8] *= 0.818336;
	qmat[10][0] *= 2.83893;
	qmat[0][10] *= 2.83893;
	qmat[10][14] *= 2.17063;
	qmat[14][10] *= 2.17063;
	qmat[10][11] *= 0.629813;
	qmat[11][10] *= 0.629813;
	qmat[10][2] *= 0.32966;
	qmat[2][10] *= 0.32966;
	qmat[10][1] *= 1.24069;
	qmat[1][10] *= 1.24069;
	qmat[10][13] *= 4.9098;
	qmat[13][10] *= 4.9098;
	qmat[10][3] *= 1.00125;
	qmat[3][10] *= 1.00125;
	qmat[10][5] *= 0.553173;
	qmat[5][10] *= 0.553173;
	qmat[10][6] *= 1.28409;
	qmat[6][10] *= 1.28409;
	qmat[10][7] *= 13.5273;
	qmat[7][10] *= 13.5273;
	qmat[10][9] *= 15.4228;
	qmat[9][10] *= 15.4228;
	qmat[10][8] *= 2.9685;
	qmat[8][10] *= 2.9685;
	qmat[4][0] *= 0.668808;
	qmat[0][4] *= 0.668808;
	qmat[4][14] *= 0.326346;
	qmat[14][4] *= 0.326346;
	qmat[4][11] *= 0.305538;
	qmat[11][4] *= 0.305538;
	qmat[4][2] *= 0.148478;
	qmat[2][4] *= 0.148478;
	qmat[4][1] *= 1.26464;
	qmat[1][4] *= 1.26464;
	qmat[4][13] *= 0.317481;
	qmat[13][4] *= 0.317481;
	qmat[4][3] *= 0.257789;
	qmat[3][4] *= 0.257789;
	qmat[4][5] *= 0.158647;
	qmat[5][4] *= 0.158647;
	qmat[4][6] *= 2.15858;
	qmat[6][4] *= 2.15858;
	qmat[4][7] *= 3.36628;
	qmat[7][4] *= 3.36628;
	qmat[4][9] *= 6.72059;
	qmat[9][4] *= 6.72059;
	qmat[4][8] *= 0.282261;
	qmat[8][4] *= 0.282261;
	qmat[4][10] *= 3.78302;
	qmat[10][4] *= 3.78302;
	qmat[12][0] *= 4.57074;
	qmat[0][12] *= 4.57074;
	qmat[12][14] *= 2.15896;
	qmat[14][12] *= 2.15896;
	qmat[12][11] *= 0.619836;
	qmat[11][12] *= 0.619836;
	qmat[12][2] *= 1.34714;
	qmat[2][12] *= 1.34714;
	qmat[12][1] *= 0.347612;
	qmat[1][12] *= 0.347612;
	qmat[12][13] *= 2.96563;
	qmat[13][12] *= 2.96563;
	qmat[12][3] *= 2.16806;
	qmat[3][12] *= 2.16806;
	qmat[12][5] *= 0.773901;
	qmat[5][12] *= 0.773901;
	qmat[12][6] *= 2.21205;
	qmat[6][12] *= 2.21205;
	qmat[12][7] *= 0.317506;
	qmat[7][12] *= 0.317506;
	qmat[12][9] *= 1.32127;
	qmat[9][12] *= 1.32127;
	qmat[12][8] *= 1.76944;
	qmat[8][12] *= 1.76944;
	qmat[12][10] *= 0.544368;
	qmat[10][12] *= 0.544368;
	qmat[12][4] *= 0.51296;
	qmat[4][12] *= 0.51296;
	qmat[15][0] *= 10.7101;
	qmat[0][15] *= 10.7101;
	qmat[15][14] *= 3.88965;
	qmat[14][15] *= 3.88965;
	qmat[15][11] *= 12.6274;
	qmat[11][15] *= 12.6274;
	qmat[15][2] *= 3.40533;
	qmat[2][15] *= 3.40533;
	qmat[15][1] *= 4.4726;
	qmat[1][15] *= 4.4726;
	qmat[15][13] *= 3.26906;
	qmat[13][15] *= 3.26906;
	qmat[15][3] *= 2.23982;
	qmat[3][15] *= 2.23982;
	qmat[15][5] *= 4.2634;
	qmat[5][15] *= 4.2634;
	qmat[15][6] *= 2.35176;
	qmat[6][15] *= 2.35176;
	qmat[15][7] *= 1.01497;
	qmat[7][15] *= 1.01497;
	qmat[15][9] *= 1.09535;
	qmat[9][15] *= 1.09535;
	qmat[15][8] *= 3.07289;
	qmat[8][15] *= 3.07289;
	qmat[15][10] *= 1.5693;
	qmat[10][15] *= 1.5693;
	qmat[15][4] *= 1.7346;
	qmat[4][15] *= 1.7346;
	qmat[15][12] *= 5.12592;
	qmat[12][15] *= 5.12592;
	qmat[16][0] *= 6.73946;
	qmat[0][16] *= 6.73946;
	qmat[16][14] *= 1.76155;
	qmat[14][16] *= 1.76155;
	qmat[16][11] *= 6.45016;
	qmat[11][16] *= 6.45016;
	qmat[16][2] *= 1.19107;
	qmat[2][16] *= 1.19107;
	qmat[16][1] *= 1.62992;
	qmat[1][16] *= 1.62992;
	qmat[16][13] *= 2.72592;
	qmat[13][16] *= 2.72592;
	qmat[16][3] *= 2.61419;
	qmat[3][16] *= 2.61419;
	qmat[16][5] *= 0.717545;
	qmat[5][16] *= 0.717545;
	qmat[16][6] *= 1.50385;
	qmat[6][16] *= 1.50385;
	qmat[16][7] *= 4.63305;
	qmat[7][16] *= 4.63305;
	qmat[16][9] *= 1.03778;
	qmat[9][16] *= 1.03778;
	qmat[16][8] *= 4.40689;
	qmat[8][16] *= 4.40689;
	qmat[16][10] *= 4.81721;
	qmat[10][16] *= 4.81721;
	qmat[16][4] *= 0.546192;
	qmat[4][16] *= 0.546192;
	qmat[16][12] *= 2.52719;
	qmat[12][16] *= 2.52719;
	qmat[16][15] *= 13.9104;
	qmat[15][16] *= 13.9104;
	qmat[18][0] *= 0.35946;
	qmat[0][18] *= 0.35946;
	qmat[18][14] *= 3.69815;
	qmat[14][18] *= 3.69815;
	qmat[18][11] *= 0.228503;
	qmat[11][18] *= 0.228503;
	qmat[18][2] *= 0.412312;
	qmat[2][18] *= 0.412312;
	qmat[18][1] *= 2.27837;
	qmat[1][18] *= 2.27837;
	qmat[18][13] *= 0.685467;
	qmat[13][18] *= 0.685467;
	qmat[18][3] *= 0.497433;
	qmat[3][18] *= 0.497433;
	qmat[18][5] *= 1.07071;
	qmat[5][18] *= 1.07071;
	qmat[18][6] *= 0.834267;
	qmat[6][18] *= 0.834267;
	qmat[18][7] *= 0.675128;
	qmat[7][18] *= 0.675128;
	qmat[18][9] *= 2.1139;
	qmat[9][18] *= 2.1139;
	qmat[18][8] *= 0.436898;
	qmat[8][18] *= 0.436898;
	qmat[18][10] *= 1.63857;
	qmat[10][18] *= 1.63857;
	qmat[18][4] *= 4.86017;
	qmat[4][18] *= 4.86017;
	qmat[18][12] *= 0.442935;
	qmat[12][18] *= 0.442935;
	qmat[18][15] *= 1.6641;
	qmat[15][18] *= 1.6641;
	qmat[18][16] *= 0.352251;
	qmat[16][18] *= 0.352251;
	qmat[19][0] *= 0.764894;
	qmat[0][19] *= 0.764894;
	qmat[19][14] *= 1.21225;
	qmat[14][19] *= 1.21225;
	qmat[19][11] *= 3.45058;
	qmat[11][19] *= 3.45058;
	qmat[19][2] *= 1.03489;
	qmat[2][19] *= 1.03489;
	qmat[19][1] *= 1.72794;
	qmat[1][19] *= 1.72794;
	qmat[19][13] *= 0.723509;
	qmat[13][19] *= 0.723509;
	qmat[19][3] *= 0.623719;
	qmat[3][19] *= 0.623719;
	qmat[19][5] *= 0.329184;
	qmat[5][19] *= 0.329184;
	qmat[19][6] *= 12.3072;
	qmat[6][19] *= 12.3072;
	qmat[19][7] *= 1.33502;
	qmat[7][19] *= 1.33502;
	qmat[19][9] *= 1.26654;
	qmat[9][19] *= 1.26654;
	qmat[19][8] *= 0.423423;
	qmat[8][19] *= 0.423423;
	qmat[19][10] *= 1.36128;
	qmat[10][19] *= 1.36128;
	qmat[19][4] *= 20.5074;
	qmat[4][19] *= 20.5074;
	qmat[19][12] *= 0.686449;
	qmat[12][19] *= 0.686449;
	qmat[19][15] *= 2.50053;
	qmat[15][19] *= 2.50053;
	qmat[19][16] *= 0.925072;
	qmat[16][19] *= 0.925072;
	qmat[19][18] *= 7.8969;
	qmat[18][19] *= 7.8969;
	qmat[17][0] *= 6.37375;
	qmat[0][17] *= 6.37375;
	qmat[17][14] *= 0.800207;
	qmat[14][17] *= 0.800207;
	qmat[17][11] *= 0.623538;
	qmat[11][17] *= 0.623538;
	qmat[17][2] *= 0.484018;
	qmat[2][17] *= 0.484018;
	qmat[17][1] *= 3.18413;
	qmat[1][17] *= 3.18413;
	qmat[17][13] *= 0.957268;
	qmat[13][17] *= 0.957268;
	qmat[17][3] *= 1.87059;
	qmat[3][17] *= 1.87059;
	qmat[17][5] *= 0.594945;
	qmat[5][17] *= 0.594945;
	qmat[17][6] *= 0.376062;
	qmat[6][17] *= 0.376062;
	qmat[17][7] *= 24.8508;
	qmat[7][17] *= 24.8508;
	qmat[17][9] *= 5.72027;
	qmat[9][17] *= 5.72027;
	qmat[17][8] *= 0.970464;
	qmat[8][17] *= 0.970464;
	qmat[17][10] *= 6.54037;
	qmat[10][17] *= 6.54037;
	qmat[17][4] *= 2.06492;
	qmat[4][17] *= 2.06492;
	qmat[17][12] *= 1.0005;
	qmat[12][17] *= 1.0005;
	qmat[17][15] *= 0.739488;
	qmat[15][17] *= 0.739488;
	qmat[17][16] *= 4.41086;
	qmat[16][17] *= 4.41086;
	qmat[17][18] *= 1.1609;
	qmat[18][17] *= 1.1609;
	qmat[17][19] *= 1;
	qmat[19][17] *= 1;
	}