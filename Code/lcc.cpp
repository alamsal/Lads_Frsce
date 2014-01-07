//----------------------------------------------------------------------------
// File:		lcc.cpp
// Author: 		Ashis Lamsal
// Last Update:	12/18/2012
// Decription:	The lcc.cpp module merge man made and non vegetated areas with buffer and prevent them from fire simulation.
// It also generates explicitly coded LCC output for landscape fire simulation snapshot results based on LCC classes provided in lcc.gis file.
//----------------------------------------------------------------------------
//#include <boost/thread.hpp>
//#include <boost/array.hpp>			//Got this error after VS & Java udpate but not sure which one casue this one need to figure out for parallel threads.
//#include <boost/lambda/lambda.hpp>

#include "ladsio.h"
#include "lcc.h"
#include "lads.h"
#include "randnum.h"
#include "celllist.h"
#include "probSurface.h"
#include "demand.h"
#include "lccGrow.h"
#include "hni.h"
#include <iostream>
#include <vector>
#include <map>
#include <cstdlib>
#include <string>
#include <algorithm>
#include <fstream>

# define NO_OF_ITERATION 100

using namespace std;


std::vector<lccCells> neighborVecObjList, neighbourVecCells;// vector to hold 8 neighborhing cells from all eligible cells temporaily
//std::map<int,vector<lccCells> > extracted_lcc[50]; // Extracted  cells from LCC
std::vector<std::map<int,vector<lccCells> > > extracted_lcc;

// Merge buffer cells and Non Vegetated areas (with LCC flag 0) with buffer to prevent them from burning during LADS simulation.
void merg_lccBuffer()
{
	/*
	for(int index=0;index<size;index++)
	{
	//printf("%d",buffer[index]);
	//printf("%d",lccgrid[index]);
	if(buffergrid[index]>0 && lcc[index]>0)
	{
	if(buffergrid[index]==2 ||(lcc[index]>=11 && lcc[index]<41)||(lcc[index]>=81 && lcc[index]<=95)) //Extract man made & non vegetated areas areas only(Exclude- natural vegetated areas)
	{
	buffergrid[index]=2;
	}

	}
	else
	{
	buffergrid[index]=0;
	}

	} */
	int j=0;
	int lclass;
	for (int index=0;index<size;index++)
	{
		if(((int)buffer[index]>=0) && ((int)lccgrid[index]>0))
		{
			for(lclass=0;lclass<numlcc;lclass++) //loop to determine the lcc class and their corresponding lcc flag and input code 
			{
				if(((int)buffer[index]==0 )||(((int)lccgrid[index]==(int)inlcccode[lclass]) && ((int)lcc_flag[lclass]==0))) //Extract man made & non vegetated areas areas only(Exclude- natural vegetated areas) and treat them as background (buffer=0)
				{
					buffer[index]=0;
					j++;
					break;

				}
			}

		}
		// Incase of LCC<0
		else
		{
			buffer[index]=0;
		}
	}
	cout <<"Merge buffer with lcc grid" <<endl;
}

/****************************************************************************
//Comparing 8 neighbouring lcc cells to initialize the LCC patch seed
Function: 	getNeighbour
Parameters: cell row, cell column
Returns:	bool
****************************************************************************/
bool getNeighbour(int row,int col,int lcccode)
{
	if(((row<maxrow) && (row>0)) && ((col<maxcol)&& (col>0)))
	{
		try{
			if(((int)lccgrid[((row-1)*maxcol+(col-1))]==lcccode)||((int)lccgrid[((row-1)*maxcol+(col))]==lcccode)||((int)lccgrid[((row-1)*maxcol+(col+1))]==lcccode)||((int)lccgrid[((row)*maxcol+(col-1))]==lcccode)
				||((int)lccgrid[((row)*maxcol+(col+1))]==lcccode)||((int)lccgrid[((row+1)*maxcol+(col-1))]==lcccode)||((int)lccgrid[((row+1)*maxcol+(col))]==lcccode)||((int)lccgrid[((row+1)*maxcol+(col+1))]==lcccode)) 
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		catch(exception e) 
		{
			cout<<e.what();	
			cout<< row << col<< lcccode;
		}
	}

}
//LCC neighbour lag  iscompactNeighbour for compacting the neighbourhood cells
bool getNeighbourLag(int row,int col,int lcccode,int lagdistance, bool iscompactNeighbour)
{
	int count_neighbour=0; // Neighbour member count
	int rwlag=-lagdistance;
	int cclag=-lagdistance;
	bool isNeighboor=false;
	int cindex;
	if(((row<maxrow) && (row>0)) && ((col<maxcol)&& (col>0)))
	{
		try{
			for (int rlag=rwlag;rlag<lagdistance+1;rlag++)
			{
				for(int clag=cclag; clag<lagdistance+1;clag++ )
				{					
					cindex=((row-rlag)*maxcol+(col-clag));
					if((cindex<size) && (cindex>-1))
					{
						if((int)lccgrid[cindex]==lcccode)
						{
							if((rlag!= 0)||(clag=!0))
							{
								if(iscompactNeighbour)
								{
									count_neighbour++;
									if(count_neighbour>1) // Eligible cell should have more than 3 same neighbourhoods
									{
										isNeighboor=true;
										count_neighbour=0;
										return true;
									}
								}
								else
								{
									isNeighboor=true;
									return true;
							
								}
							}
						}

					}					
				}
			}
			if(!isNeighboor)
			{
				return false;		
			}
		}
		catch(exception e) 
		{
			cout<<e.what();	
			cout<<"Neighbour LAG out of Bound:: Press ENTER to continue"<<endl;
			cin.get();
		}
	}
}
bool cellTrasition(int cellindex, int lcccode, bool hni_trasition)
{
	//Allow broad LCLU classes transitions only on hni cells
	//HNI Cells can either go to Natural or Human
	//In both cases HNI no longer exist in HNI cell and hnigrid[index]=0 and lccgrid[index]=lcccode along with other veg and non veg parameters to look around.
	int nohnicode=1;
	if(hni_trasition)  // Trasitions to/from hni to broader lcc
	{
		bool natural=false;
		for(unsigned int l=0;l<numlcc;l++)
		{
			if(hnitempgridFlag[cellindex]==0)
			{
				if((lcccode==inlcccode[l])&&(lcc_flag[l]==1)) //Natrual trasition flag 1=Nat, 0=human
				{
					hnigrid[cellindex]=nohnicode; // All succession states, regimes,lcc code and parameters  already out there. Just flip to hni or non hni.
					hnitempgridFlag[cellindex]=1;
					return true;
					break;
				}
				else
				{
					hnitempgridFlag[cellindex]=1;
					hnigrid[cellindex]=nohnicode;
					lccgrid[cellindex]=lcccode;     // Assign new LCC class
					buffer[cellindex]=0;			//Prevent from simulation human dominated cell
					return true;
					break;
				}
			}
			return false;
		}

	}
	//Allow broad LCLU classes transitions only on non hni cells
	else
	{
		if((int)hnigrid[cellindex]==nohnicode)
		{	
			int cellValue= (int)lccgrid[cellindex];
			bool toHuman=true;

			for( unsigned int i=0; i<numlcc;i++)
			{
				//Trasition from Human dominted to Nature and Human dominated to Human
				if((cellValue==inlcccode[i]) && (lcc_flag[i]==0))
				{
					for(unsigned int j=0; j<numlcc;j++)
					{
						if((lcccode==inlcccode[j])&&(lcc_flag[j]==1))
						{
							stategrid[cellindex]=1;			// When forsce simulate veg to veg/ non-veg to veg the successional stage= 1;timeinstage =0; age=0; time since fire=0. 
							// When forsce simulate non-veg to non-veg * veg to non-veg the buffer is  set to 0, which will handle by "merg_lccBuffer()" after the completion this demand allocation look at lads.cpp.
							timeinstage[cellindex]=1;		// time in current successional stage grid
							age[cellindex]=1;				// patch age grid
							lccgrid[cellindex]=inlcccode[j];
							tsfire[cellindex]=1;			// time since last fire grid
							buffer[cellindex]=1;			//Active simulation
							tempgridFlag[cellindex]=1;		// To prevent from further trasition of the same cell
							toHuman=false;
							return true;
							break;
						}
					}
					if(toHuman)
					{
						lccgrid[cellindex]=lcccode; 
						buffer[cellindex]=0;			//Prevent from simulation human dominated cell
						tempgridFlag[cellindex]=1;		// To prevent from further trasition of the same cell
						return true;

					}


				}
				//Trasition from Nature to Nature (No pixel transition-just for reference) and Nature to Human
				else
				{
					if((cellValue==inlcccode[i]) && (lcc_flag[i]==1))
					{
						for(unsigned int k=0;k<numlcc;k++)
						{
							if((lcccode==inlcccode[k])&&(lcc_flag[k]==1))
							{
								return false;
								break;
							}
							else
							{
								lccgrid[cellindex]=lcccode; 
								buffer[cellindex]=0;			//Prevent from simulation human dominated cell
								tempgridFlag[cellindex]=1;		// To prevent from further trasition of the same cell
								return true;
							}
						}

					}

				}

			}
		}
		else
		{
			return false;
		}
	}

}

/****************************************************************************
//Allocation of cells in eight neighbourhoood locations recursively
Function: 	getEightNeighborhood
Parameters: Extracted LCLU cell vectors to change, cell row, cell column,lcccode to be assigned after change, probability surface index in the probaility surfaces vector, total # of demand

Returns:	
****************************************************************************/
std::vector<lccCells> fillEightNeighborhood(std::vector<lccCells> vecobj, int irow, int icol,int lcccode,int prob_index,int &demand, int &patch_size)
{
	std::vector<lccCells> neighborVecObj; // vector to hold 8 neighborhing cells temporaily
	int initdem=demand;
	int veclen=vecobj.size();
	unsigned int iteration_count1=0;
	unsigned int iteration_count2=0;
	unsigned int iteration_count3=0;
	bool iterationflag= true;
	if((demand>0) && (veclen>0))
	{	
		int ilcccol2;
		int ilccrow2;
		int index1;
		int index2;

		lccCells tempCells;

		for(int j=-1;j<=1;j++)
		{
			for (int k=-1;k<=1;k++)
			{
				if(j!=0||k!=0)
				{
					irow=irow+j;
					icol=icol+k;

					index1=irow*maxcol+icol;

					for(unsigned int i=0;i<veclen;i++)
					{
						ilcccol2=vecobj.at(i).lccCol;
						ilccrow2=vecobj.at(i).lccRow;	
						index2=ilccrow2*maxcol+ilcccol2;

						//cout<<"ilcccol2:"<<ilcccol2<<endl;
						//cout<<"ilccrow2:"<<ilccrow2<<endl;

						if((((icol>=0) && (icol<=maxcol)) && ((ilcccol2>=0) && (ilcccol2<=maxcol))) && (((irow>=0) && (irow<=maxrow)) && ((ilccrow2>=0) && (ilccrow2<=maxrow))))
						{
							if(demand>0 && patch_size<meanpatchSize)
							{
								if((index1==index2)&& (tempgridFlag[index1]==0))  
								{ 
									tempCells.lccCol=ilcccol2;
									tempCells.lccRow=ilccrow2;
									neighborVecObj.push_back(tempCells);	// Store the changed neighbour cells
									/*
									if(lccgrid[index1]!=lcccode)
									{
									while((iterationflag) && (iteration_count3<NO_OF_ITERATION))
									{
									//Get the transitional probability value;
									trans_probaility=(double)(probability_surfaces[prob_index][ilccrow2][ilcccol2]);
									//Generate a uniform random variable;
									irand=u0_1();
									if(irand<trans_probaility)
									{

									//			int index_value= (int)(lccgrid[index1]);
									//			//If FORSCE change the cell transition form non-veg to vegetated; we must assign succesational stage [index]==1 to start future successional stages.
									//			if(((index_value==11)||(index_value==12)||(index_value==20)||(index_value==30)||(index_value==81)||(index_value==82)||(index_value==41)||(index_value==42)||(index_value==43)||(index_value==52)||(index_value==71))&&((lcccode==41)||(lcccode==42)||(lcccode==52)||(lcccode==71)))	
									//			{												//Put the smallest value from state,initage,and tsfire grids
									//				stategrid[index1]=1; // When forsce simulate veg to veg/ non-veg to veg the successional stage= 1;timeinstage =0; age=0; time since fire=0. 
									//										// When forsce simulate non-veg to non-veg * veg to non-veg the buffer is  set to 0, which will handle by "merg_lccBuffer()" after the completion this demand allocation look at lads.cpp.
									//				timeinstage[index1]=1;	// time in current successional stage grid
									//				age[index1]=5;		    // patch age grid
									//				lccgrid[index1]=lcccode;
									//				tsfire[index1]=1;      // time since last fire grid
									//				buffer[index1]=1;		//Active simulation
									//				tempgridFlag[index1]=1;		// To prevent from further trasition of the same cell
									//
									//			}
									//			else
									//			{
									//				lccgrid[index1]=lcccode; 
									//				buffer[index1]=0;			//Prevent from simulation human dominated cell
									//				tempgridFlag[index1]=1;		// To prevent from further trasition of the same cell
									//			}


									bool transFlag=cellTrasition(index1,lcccode);

									if(transFlag)
									{
									demand--;
									patch_size++;
									iterationflag=false;
									iteration_count3++;
									writelog<< "Rem. to accomp demand::"<<demand <<"\t Lcc Code::"<<lcccode<<"\t Rem. trans. prob pixel #::" <<vecobj.size() <<endl; 
									}

									}
									}

									} */
								}

							}
						}

					}							
				}						
			}
		}
		//writelog<<"demand:\t"<<demand<<endl;
	}
	return(neighborVecObj);
}
//Find neighbourhood based on lag distance
std::vector<lccCells> fillNeighborhood(std::vector<lccCells> vecobj, int irow, int icol,int lcccode,int prob_index,int &demand, int &patch_size, int dlag,bool hni_trasition)
{
	std::vector<lccCells> neighborVecObj; // vector to hold 8 neighborhing cells temporaily
	int initdem=demand;
	int veclen=vecobj.size();
	unsigned int iteration_count1=0;
	unsigned int iteration_count2=0;
	unsigned int iteration_count3=0;
	bool iterationflag= true;

	int distanceLag=dlag;
	std::vector<int>tempLaggrid; // Container to store status of cell already existed in neighbourhood vector or not
	tempLaggrid.resize(size);		//Resize vetor to landscape size
	std::fill(tempLaggrid.begin(),tempLaggrid.end(),0); // Filling vector with 0s

	if((demand>0) && (veclen>0))
	{	
		int ilcccol2;
		int ilccrow2;
		int index1;
		int index2;

		lccCells tempCells;
		for (int dlag=0;dlag<distanceLag;dlag++)
		{
			for(int j=-1-dlag;j<=1+dlag;j++)
			{
				for (int k=-1-dlag;k<=1+dlag;k++)
				{
					if(j!=0||k!=0)
					{
						irow=irow+j;
						icol=icol+k;

						index1=irow*maxcol+icol;

						for(unsigned int i=0;i<veclen;i++)
						{
							ilcccol2=vecobj.at(i).lccCol;
							ilccrow2=vecobj.at(i).lccRow;	
							index2=ilccrow2*maxcol+ilcccol2;

							//cout<<"ilcccol2:"<<ilcccol2<<endl;
							//cout<<"ilccrow2:"<<ilccrow2<<endl;

							if((((icol>=0) && (icol<=maxcol)) && ((ilcccol2>=0) && (ilcccol2<=maxcol))) && (((irow>=0) && (irow<=maxrow)) && ((ilccrow2>=0) && (ilccrow2<=maxrow))))
							{
								if(demand>0 && patch_size<meanpatchSize)
								{
									if(hni_trasition)
									{
										if((index1==index2) && (tempgridFlag[index1]==0) && (tempLaggrid[index1]==0) && (hnitempgridFlag[index1]==0))  
											 
										{ 
											tempCells.lccCol=ilcccol2;
											tempCells.lccRow=ilccrow2;
											neighborVecObj.push_back(tempCells);	// Store the changed neighbour cells
											tempLaggrid[index1]=1;
										
										}
									}
									else
									{
										if((index1==index2) && (tempgridFlag[index1]==0) && (tempLaggrid[index1]==0))  
											//if((index1==index2) && (tempgridFlag[index1]==0))  
										{ 
											tempCells.lccCol=ilcccol2;
											tempCells.lccRow=ilccrow2;
											neighborVecObj.push_back(tempCells);	// Store the changed neighbour cells
											tempLaggrid[index1]=1;
											/*
											if(lccgrid[index1]!=lcccode)
											{
											while((iterationflag) && (iteration_count3<NO_OF_ITERATION))
											{
											//Get the transitional probability value;
											trans_probaility=(double)(probability_surfaces[prob_index][ilccrow2][ilcccol2]);
											//Generate a uniform random variable;
											irand=u0_1();
											if(irand<trans_probaility)
											{

											//			int index_value= (int)(lccgrid[index1]);
											//			//If FORSCE change the cell transition form non-veg to vegetated; we must assign succesational stage [index]==1 to start future successional stages.
											//			if(((index_value==11)||(index_value==12)||(index_value==20)||(index_value==30)||(index_value==81)||(index_value==82)||(index_value==41)||(index_value==42)||(index_value==43)||(index_value==52)||(index_value==71))&&((lcccode==41)||(lcccode==42)||(lcccode==52)||(lcccode==71)))	
											//			{												//Put the smallest value from state,initage,and tsfire grids
											//				stategrid[index1]=1; // When forsce simulate veg to veg/ non-veg to veg the successional stage= 1;timeinstage =0; age=0; time since fire=0. 
											//										// When forsce simulate non-veg to non-veg * veg to non-veg the buffer is  set to 0, which will handle by "merg_lccBuffer()" after the completion this demand allocation look at lads.cpp.
											//				timeinstage[index1]=1;	// time in current successional stage grid
											//				age[index1]=5;		    // patch age grid
											//				lccgrid[index1]=lcccode;
											//				tsfire[index1]=1;      // time since last fire grid
											//				buffer[index1]=1;		//Active simulation
											//				tempgridFlag[index1]=1;		// To prevent from further trasition of the same cell
											//
											//			}
											//			else
											//			{
											//				lccgrid[index1]=lcccode; 
											//				buffer[index1]=0;			//Prevent from simulation human dominated cell
											//				tempgridFlag[index1]=1;		// To prevent from further trasition of the same cell
											//			}


											bool transFlag=cellTrasition(index1,lcccode);

											if(transFlag)
											{
											demand--;
											patch_size++;
											iterationflag=false;
											iteration_count3++;
											writelog<< "Rem. to accomp demand::"<<demand <<"\t Lcc Code::"<<lcccode<<"\t Rem. trans. prob pixel #::" <<vecobj.size() <<endl; 
											}

											}
											}

											} */
										}
									}
								}
							}

						}							
					}						
				}
			}
		}
	}
	tempLaggrid.clear();
	return(neighborVecObj);
}

/****************************************************************************
// Create and return the map of structure vectors to store all extracted values based upon probability surfaces,LCC, and breakpoints.
Function: 	extract_LandCoverCells
Parameters: lclu grid, lclu class code 
Returns:vector of raster cells that have potiential  to change from one class to another
****************************************************************************/
std::map<int,vector<lccCells> > extract_LandCoverCells(int lccCode)
{
	unsigned int index; // array index
	unsigned int node_counter=0; //Counter to track the lenght of forest list.
		
	lccCells tempLcc; // Struct object to store row and columns of a raster cell.

	std::map<int,vector<lccCells> > ext_lcc_vector;	// Map of vectors containing sturcuture to hold all extracted values based upon probability surfaces, LCC, and ownership.
	ext_lcc_vector.clear();

	unsigned int count=0; //Cell counter for each class of LCC.

	// Read LCC grid and extract forest cell into a separate linked list, named forest_list. 
	for(unsigned int row=0; row<maxrow; row++) 
	{
		for(unsigned int col=0; col<maxcol; col++) 
		{
			index=row*maxcol + col;			
			if((lccgrid[index]==lccCode)&&(hnigrid[index]==1)) //Extract LCC class based upon lcc code and cells that are not in HNI grid
			{
				//filter Lcc based on owenership type and development restriction
				for (unsigned int olayer=0;olayer<numOwnership;olayer++)
				{
					if((ownergrid[index]==ownershipCode[olayer]) &&	(ownershipRestriction[olayer]==0))
					{
						/*
						if((probability_surfaces[0][row][col])>0.95)			//Extract cells having  water transition prob threshold >95% i.e 0.95 ( to water)
						{
						tempLcc.lccRow=row;
						tempLcc.lccCol=col;
						ext_lcc_vector[0].push_back(tempLcc);
						}
						if((probability_surfaces[1][row][col])>0.95)			//Extract cells having  ice transition prob threshold >95% i.e 0.95 ( to ice)
						{
						tempLcc.lccRow=row;
						tempLcc.lccCol=col;
						ext_lcc_vector[1].push_back(tempLcc);
						}
						if((probability_surfaces[2][row][col])>0.93)			//Extract cells having transition prob threshold>93% ie. 0.93 (to urban)
						{
						tempLcc.lccRow=row;
						tempLcc.lccCol=col;
						ext_lcc_vector[2].push_back(tempLcc);
						}
						if((probability_surfaces[3][row][col])>0.85)			//Extract cells having trasnition prob threshold>85 ie. 0.84 ( to barren)
						{
						tempLcc.lccRow=row;
						tempLcc.lccCol=col;
						ext_lcc_vector[3].push_back(tempLcc);
						}
						if((probability_surfaces[4][row][col])>0.85)			//to decdidous forest
						{
						tempLcc.lccRow=row;
						tempLcc.lccCol=col;
						ext_lcc_vector[4].push_back(tempLcc);

						}
						if((probability_surfaces[5][row][col])>0.85)			//to evergreen forest
						{
						tempLcc.lccRow=row;
						tempLcc.lccCol=col;
						ext_lcc_vector[5].push_back(tempLcc);
						}
						if((probability_surfaces[6][row][col])>0.85)			//to shrubland
						{
						tempLcc.lccRow=row;
						tempLcc.lccCol=col;
						ext_lcc_vector[6].push_back(tempLcc);
						}
						if((probability_surfaces[7][row][col])>0.85)			//to grassland
						{
						tempLcc.lccRow=row;
						tempLcc.lccCol=col;
						ext_lcc_vector[7].push_back(tempLcc);
						}
						if((probability_surfaces[8][row][col])>0.90)			//to hay/pasture
						{
						tempLcc.lccRow=row;
						tempLcc.lccCol=col;
						ext_lcc_vector[8].push_back(tempLcc);
						}
						if((probability_surfaces[9][row][col])>0.85)			//to crops
						{
						tempLcc.lccRow=row;
						tempLcc.lccCol=col;
						ext_lcc_vector[9].push_back(tempLcc);
						}
						*/
						for (unsigned int i=0;i<numProbsurface;i++)
						{
							if((probability_surfaces[i][row][col])>(transitionThreshold[i]))			//Extract cells having  water transition prob threshold >95% i.e 0.95 ( to water)
							{
								tempLcc.lccRow=row;
								tempLcc.lccCol=col;
								ext_lcc_vector[i].push_back(tempLcc);

							}

						}

						count++;
					}
				}

			}
		}
	}	
	
	

	return ext_lcc_vector;
}
//Extract and allocte HNI
void extract_hnicells(int demperiod)
{
	allocate_hni2lcc(demperiod);
	extract_allocate_lcc2hni(demperiod);
	
	
}


//Extract and allocate borader LCC
void extract_lcccells(int demperiod)
{	
	allocate_lccCells(demperiod);
}

//Get all eligible trasition cells into a vector; vector stores row & columns of all LCC classes including LCC to hni
std::vector<std::map<int,vector<lccCells> > > getextractedCells(std::vector<std::map<int,vector<lccCells> > > &temp_lcc)
{
	temp_lcc.clear();
	for (unsigned int i=0;i<numlcc;i++)
	{
		extract_LandCoverCells(inlcccode[i]);
		temp_lcc.push_back(extract_LandCoverCells(inlcccode[i]));
	}
	return temp_lcc;
}



//Spatial allocation of Demands
void allocate_lccCells(int demperiod)
{
	/*
	extracted_lcc[0]=extract_LandCoverCells(11); // Extract water cells from LCC
	extracted_lcc[1]=extract_LandCoverCells(12); // Extract ice/snow cells from LCC
	extracted_lcc[2]=extract_LandCoverCells(20); // Extract developed cells from LCC
	extracted_lcc[3]=extract_LandCoverCells(30); // Extract barren cells from LCC 
	extracted_lcc[4]=extract_LandCoverCells(41); // Extract deci forest cells from LCC 
	extracted_lcc[5]=extract_LandCoverCells(42); // Extract evergreen forest cells from LCC
	extracted_lcc[6]=extract_LandCoverCells(52); // Extract shrubland cells from LCC 
	extracted_lcc[7]=extract_LandCoverCells(71); // Extract grassland cells from LCC 
	extracted_lcc[8]=extract_LandCoverCells(81); // Extract hay/pasture cells from LCC
	extracted_lcc[9]=extract_LandCoverCells(82); // Extract cropland cells from LCC
	extracted_lcc[10]=extract_LandCoverCells(90); // Extract cropland cells from LCC
	extracted_lcc[11]=extract_LandCoverCells(95); // Extract cropland cells from LCC
	*/
		
	getextractedCells(extracted_lcc);

	std::map<int,vector<lccCells> > lcc_cells;
	std::vector<lccCells> vec_lcc_cells;

	//Read demand file in row/column order
	for(unsigned int row=0; row<numlcc;row++)	// Total rows upto LCC number notup to demand file to prevent HNI simulation here
	{
		for (unsigned int col=0; col<numlcc; col++)// Total columns  upto LCC number notup to demand file to prevent HNI simulation here
		{
			//cout<<"("<<row<<","<<col<<")"<<demand_matrix[row][col]<<"\t";
			if(row==col)		//No demand allocation diagonally 
			{
				cout<<"No demand allocation"<<endl;

			}
			else
			{
				//demand value comes from here..
				//int demand=(int)demand_matrix[row][col][dfile];	//Convert from string type to integer.
				int demand=(int)demand_matrix[demperiod][row][col];
				lcc_cells=extracted_lcc[row];

				vec_lcc_cells=lcc_cells[col];
				
				cout<<"Eligible cell#:"<<vec_lcc_cells.size()<<" From:"<<inlcccode[row]<<" To:"<<inlcccode[col] <<" Psurface#: "<<col <<" Demand#:"<<demand<<" Lag#" <<pts_distanceLag[col]<<" Psize#" << pts_patchSize[col]<<" Mpsize#"<<meanpatchSize<<" Std#" << pts_stdDeviation[col] <<endl;
				writelog<<"Eligible cell#:"<<vec_lcc_cells.size()<<" From:"<<inlcccode[row]<<"To:"<<inlcccode[col] <<" Psurface#: "<<col <<" Demand#:"<<demand <<" Lag#" <<pts_distanceLag[col]<<" Psize#" << pts_patchSize[col]<<" Mpsize#"<<meanpatchSize<<" Std#" << pts_stdDeviation[col]<<endl;

				//Feed for spatial allocation of broader LCC classes only ( No hni simulation- ishni_trasition=false)
				space_allocation(vec_lcc_cells,inlcccode[col],col,demand,pts_distanceLag[col], pts_patchLag[col],false);
			}

		}
		cout<<endl<<endl;
	}

}


/****************************************************************************
//Spatial allocation of cells in the 2D grid
Function: 	space_allocation
Parameters: Extracted LCLU cell vectors to change, lcccode to be assigned after change, probability surface index in the probaility surfaces vector, total # of demand

Returns:	
****************************************************************************/
void space_allocation( std::vector<lccCells> vecobj,int lcccode, int prob_index, int &demand, int dlag, unsigned int plag, bool ishni_transition)
{
	int lag=dlag;
	int rand_forestrow;
	int rand_forestcol;
	double tras_probability;
	unsigned int cell_index;
	double irand;
	int counter1=0; //Counts the demand allocaiton while loop (1st loop)
	int counter2=0;	//Counts the neighbour demand allocation while loop (2nd loop)
	int patch_size;		//Counter to hold patch size
	int olddemand=0;
	int neighdemand=0;
	bool fillNeighbour=false;	//flag to enable  neighbouring pixel  to fill
	bool afterIteraiton=false;  //flag to enable setting pixel change on not adjacent neighbourig cells after preset total no# iteration accomplished . To prevent from infinite loop during demand allocation
	int patchcol=prob_index;	// column value to generate patch from index
	if(((demand>0) && (vecobj.size()>0)) && (demand<=vecobj.size())) 
	{
		//cout<< "Start Demand::"<<demand <<"\t Lcc Code::"<<lcccode<<"\t Transition prob pixel #::" <<vecobj.size() << endl;

		while((vecobj.size()>0) && (demand>0))
		{
			olddemand=demand;
			patch_size=0;
			
			//Generate mean patch size from normal distribution
			meanpatchSize=genLCCPatchSize(pts_patchSize[patchcol],pts_stdDeviation[patchcol]);
			
			neighborVecObjList.clear();
			neighbourVecCells.clear();
			//Generare a random integer between 1 and cell length.
			unsigned int rand_index=(unsigned int) rand_int(vecobj.size());


			//Retrive random index from vector
			rand_forestrow=vecobj.at(rand_index-1).lccRow;
			rand_forestcol=vecobj.at(rand_index-1).lccCol;


			////Transition probability value
			tras_probability=(double)(probability_surfaces[prob_index][rand_forestrow][rand_forestcol]);		//(1/255*probability_surfaces[1][rand_forestrow][rand_forestcol])

			////Generate a uniform random variable for the forest cell
			irand=u0_1();
			if(irand<tras_probability)
			{
				cell_index=rand_forestrow*maxcol + rand_forestcol;
				//No lag constraion for seed placement
					if((afterIteraiton) || (lag==-9) )
					{
						if(((lcccode != lccgrid[cell_index]) && (tempgridFlag[cell_index]==0) ))
						{

							bool transFlag=cellTrasition(cell_index,lcccode,ishni_transition);

							if(transFlag)
							{
								demand--;
								patch_size++;
								counter1=0;
								cout<< "Rem demand: "<<demand<<"\t Eligible cells: "<< vecobj.size()<<endl;
								vecobj.erase(vecobj.begin()+rand_index-1);
								if(vecobj.size()!=0)
								{
									vecobj.begin(); 
								}
								//neighbourVecCells=fillEightNeighborhood( vecobj, rand_forestrow, rand_forestcol, lcccode, prob_index, demand,patch_size); // Works under 8 neighbourhoood
								neighbourVecCells=fillNeighborhood( vecobj, rand_forestrow, rand_forestcol, lcccode, prob_index, demand,patch_size,plag,ishni_transition);// Works under patch lag
								fillNeighbour=true;
								writelog<< "Code:3-Rem. to accomp demand::"<<demand <<"\t Lcc Code::"<<lcccode<<"\t Rem. trans. prob pixel #::" <<vecobj.size() <<"counter" <<counter1<< endl; 
							}					

						}
					}
					else
					{
						//if(((getNeighbour(rand_forestrow,rand_forestcol,lcccode)) && (lcccode != lccgrid[cell_index]) && (tempgridFlag[cell_index]==0) )) //Prevnet model to put seed pixel randomly on the landscape :enforce adjacency
						//if((lcccode != lccgrid[cell_index]) && (tempgridFlag[cell_index]==0)) //Following Mike's comment to put seed pixel randomly on the landscape
						

						//Lag constraion for seed placement
						if((getNeighbourLag(rand_forestrow,rand_forestcol,lcccode,lag,false))  && (lcccode != lccgrid[cell_index]) && (tempgridFlag[cell_index]==0)) //Prevnet model to put seed pixel randomly on the landscape :enforce lag distance

						{

							bool transFlag=cellTrasition(cell_index,lcccode,ishni_transition);

							if(transFlag)
							{
								demand--;
								patch_size++;
								counter1=0;
								cout<< "Rem demand: "<<demand<<"\t Eligible cells: "<< vecobj.size()<<endl;
								vecobj.erase(vecobj.begin()+rand_index-1);
								if(vecobj.size()!=0)
								{
									vecobj.begin(); 
								}
								//neighbourVecCells=fillEightNeighborhood( vecobj, rand_forestrow, rand_forestcol, lcccode, prob_index, demand,patch_size); // Works under 8 neighbourhoood
								neighbourVecCells=fillNeighborhood( vecobj, rand_forestrow, rand_forestcol, lcccode, prob_index, demand,patch_size,plag,ishni_transition); //Works under distance lag neighbourhood
								fillNeighbour=true;
								writelog<< "Code:1-Rem. to accomp demand::"<<demand <<"\t Lcc Code::"<<lcccode<<"\t Rem. trans. prob pixel #::" <<vecobj.size() <<"counter" <<counter1<< endl; 
							}					

						}
					}


			}


			//Spatial allocation of cells which are in  distance lag neighbourhood 
			/*
			if(((initdem==demand) && (demand>0) && (neighborVecObj.size()<1))|| (patch_size>meanpatchSize)) 
			{
			space_allocation( vecobj, lcccode,  prob_index,  demand); // Assign new seed to change

			}
			else //Recursive algorithm to visit all the neighbouring cells
			{
			if((demand>0) && (neighborVecObj.size()>0))
			{
			for( unsigned int i=0;i<neighborVecObj.size();i++)
			{
			int neighrow=neighborVecObj.at(i).lccRow;
			int neighcol=neighborVecObj.at(i).lccCol;

			neighborVecObj.erase(neighborVecObj.begin()+i);
			neighborVecObj.begin();			

			fillEightNeighborhood(neighborVecObj,neighrow,neighcol,lcccode,prob_index,demand,patch_size);

			}
			}
			}

			*/
			while((neighbourVecCells.size()>0) && (demand>0) && (patch_size<meanpatchSize) && (fillNeighbour))
			{
				neighdemand=demand;

				for( unsigned int i=0;i<neighbourVecCells.size();i++)
				{ 
					if((demand>0) && (patch_size < meanpatchSize))
					{
						int neighrow=neighbourVecCells.at(i).lccRow;
						int neighcol=neighbourVecCells.at(i).lccCol;


						////Transition probability value
						tras_probability=(double)(probability_surfaces[prob_index][neighrow][neighcol]);		//(1/255*probability_surfaces[1][rand_forestrow][rand_forestcol])

						////Generate a uniform random variable for the forest cell
						irand=u0_1();
						if(irand<tras_probability)
						{
							cell_index=neighrow*maxcol + neighcol;
							//if(((getNeighbour(neighrow,neighcol,lcccode)) &&(lcccode != lccgrid[cell_index]) && (tempgridFlag[cell_index]==0) )) //Enforce adjaceny while cell transition
							if((getNeighbourLag(neighrow,neighcol,lcccode,plag,true))  && (lcccode != lccgrid[cell_index]) && (tempgridFlag[cell_index]==0) )  //Enforce patch shape lag while cell transtion
							{

								bool transFlag=cellTrasition(cell_index,lcccode,ishni_transition);

								if(transFlag)
								{
									demand--;
									patch_size++;
									counter2=0;
									cout<< "Rem demand: "<<demand<<"\t Eligible cells in NeigbourHood List: "<< neighbourVecCells.size()<<endl;
									writelog<< "Rem demand: "<<demand<<"\t Eligible cells in NeigbourHood List: "<< neighbourVecCells.size()<<endl;
									neighbourVecCells.erase(neighbourVecCells.begin()+i);
									if(neighbourVecCells.size()!=0)
									{
										neighbourVecCells.begin(); 
									}

									//neighborVecObjList=fillEightNeighborhood(vecobj,neighrow,neighcol,lcccode,prob_index,demand,patch_size); // Works under 8 neighbourhoood
									neighborVecObjList=fillNeighborhood(vecobj,neighrow,neighcol,lcccode,prob_index,demand,patch_size,plag,ishni_transition); //Works under patch lag neighbourhood
									neighbourVecCells.insert(neighbourVecCells.end(),neighborVecObjList.begin(),neighborVecObjList.end());

									writelog<< "Code:2-Rem. to accomp demand::"<<demand <<"\t Lcc Code::"<<lcccode<<"\t Rem. trans. prob pixel #::" <<vecobj.size() <<"counter" <<counter2<< endl; 
								}					

							}

						}
					}

				} //end for loop

				if(neighdemand==demand && counter2>NO_OF_ITERATION*(neighbourVecCells.size()))
				{

					fillNeighbour=false;
					break;

				}
				counter2++;

			} //end 2nd while

			if(olddemand==demand && counter1>NO_OF_ITERATION*(vecobj.size()))
			{

				//vecobj.erase(vecobj.begin()+rand_index-1);
				//if(vecobj.size()!=0)
				//{
				//	vecobj.begin(); 
				//}
				afterIteraiton=true;
				counter1=0;
			}
			counter1++;


		}//end 1st while

	}  //end if
	else
	{
		cout<< "Demand ::"<<demand <<"\t transition prob pixel size ::"<<vecobj.size()<< endl;

	}



}

// Assign output codes to  NDLC 2006 input LCC codes to generate output grid.
//Place to merge LADS & FORESCE outputs
void merg_lccSnapshot()
{
	/*
	for(int index=0;index<=size;index++)
	{
	// Need to loop  through the lcc file and figure out.....
	if(lccgrid[index]==11)
	{
	temp[index]=211;
	}
	else if(lccgrid[index]==12)
	{
	temp[index]=212;
	}
	else if(lccgrid[index]==20)
	{
	temp[index]=213;
	}
	else if(lccgrid[index]==22)
	{
	temp[index]=214;
	}
	else if(lccgrid[index]==23)
	{
	temp[index]=215;
	}
	else if(lccgrid[index]==24)
	{
	temp[index]=216;
	}
	else if(lccgrid[index]==31)
	{
	temp[index]=217;
	}
	else if(lccgrid[index]==81)
	{
	temp[index]=227;
	}
	else if(lccgrid[index]==82)
	{
	temp[index]=228;
	}
	else if(lccgrid[index]==90)
	{
	temp[index]=229;
	}
	else if(lccgrid[index]==95)
	{
	temp[index]=230;
	}
	else
	{
	temp[index]=temp[index];
	}

	}
	*/

	for(int index=0;index<=size;index++)
	{
		for(int lclass=0;lclass<numlcc;lclass++)
		{
			if((lccgrid[index]==inlcccode[lclass]) && (lcc_flag[lclass]==0))
			{
				temp[index]=outlcccode[lclass];			
			}
		}

	}

}

//Reclassify into broad landcover class
void reclassify_HumanDominated()
{
	for(unsigned int index=0;index<=size;index++)
	{
		for(unsigned int j=0;j<numlcc;j++)
		{
			if(((temp[index])==(outlcccode[j])) && (lcc_flag[j]==0))
			{
				temp[index]=inlcccode[j];
				//cout<< temp[index];
			}
		}

	}

}
//Reclassify into broad landcover class
void reclassify_NatureDominated(unsigned int stateout[],unsigned int lclustate[],unsigned int statecounter)
{
	//for(unsigned int i=0;i<=statecounter;i++)
	//{
	//cout<<stateout[i] <<"****" <<lclustate[i]<<endl;

	//}

	for(unsigned int index=0; index<=size;index++)
	{
		for(unsigned int i=0;i<=statecounter;i++)
		{
			if((int)temp[index]==(int)stateout[i])
			{
				temp[index]=lclustate[i];
				break;
			}
		}
	}
}
//Reclassifying the LADS/FORESCE output into initial LCLU classes.
void reclassify_lclu(unsigned int stateout[],unsigned int lclustate[],unsigned int statecounter)
{
	reclassify_NatureDominated(stateout,lclustate,statecounter);
	reclassify_HumanDominated();

	//Assign temp lcc grid as new land cover land use grid (new lccgrid) for subsequent processing	
	strcpy(lccgrid,reinterpret_cast<char*>(temp)); //convert from 'unsigned char *' to 'char *'



}

