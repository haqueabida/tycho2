/*
    Transport.cc
    
    Implements sweep on one cell given incoming boundary data.
*/

/*
Copyright (c) 2016, Los Alamos National Security, LLC
All rights reserved.

Copyright 2016. Los Alamos National Security, LLC. This software was produced 
under U.S. Government contract DE-AC52-06NA25396 for Los Alamos National 
Laboratory (LANL), which is operated by Los Alamos National Security, LLC for 
the U.S. Department of Energy. The U.S. Government has rights to use, 
reproduce, and distribute this software.  NEITHER THE GOVERNMENT NOR LOS 
ALAMOS NATIONAL SECURITY, LLC MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR 
ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE.  If software is modified 
to produce derivative works, such modified software should be clearly marked, 
so as not to confuse it with the version available from LANL.

Additionally, redistribution and use in source and binary forms, with or 
without modification, are permitted provided that the following conditions 
are met:
1.      Redistributions of source code must retain the above copyright notice, 
        this list of conditions and the following disclaimer.
2.      Redistributions in binary form must reproduce the above copyright 
        notice, this list of conditions and the following disclaimer in the 
        documentation and/or other materials provided with the distribution.
3.      Neither the name of Los Alamos National Security, LLC, Los Alamos 
        National Laboratory, LANL, the U.S. Government, nor the names of its 
        contributors may be used to endorse or promote products derived from 
        this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY LOS ALAMOS NATIONAL SECURITY, LLC AND 
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT 
NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LOS ALAMOS NATIONAL 
SECURITY, LLC OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED 
OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "Transport.hh"
#include "Assert.hh"
#include "Global.hh"
#include "TychoMesh.hh"
#include "PsiData.hh"
#include <iostream>
#include <string>
#include <cmath>
#include <stdio.h>

// Namespaces
using namespace std;


/*
    calcLocalSource
*/
static
void calcLocalSource(const UINT cell,
                     const Mat2<double> &localSource,
                     Mat2<double> &cellSource)//,
                     //const UINT group) 
{
    double volume = g_spTychoMesh->getCellVolume(cell);
    
	#pragma ivdep
	for (UINT group = 0; group < g_nGroups; group++) {
    	double q0 = localSource(0, group);
    	double q1 = localSource(1, group);
    	double q2 = localSource(2, group);
    	double q3 = localSource(3, group);
    
    	cellSource(0, group) = volume / 20.0 * (2.0 * q0 + q1 + q2 + q3);
    	cellSource(1, group) = volume / 20.0 * (q0 + 2.0 * q1 + q2 + q3);
    	cellSource(2, group) = volume / 20.0 * (q0 + q1 + 2.0 * q2 + q3);
    	cellSource(3, group) = volume / 20.0 * (q0 + q1 + q2 + 2.0 * q3);
	}
}


/*
    streamPlusColl
*/
static
void streamPlusColl(const UINT cell, const UINT angle,
                    const double sigmaTotal,
                    double matrix[g_nVrtxPerCell][g_nVrtxPerCell]) 
{
    double volume, area[4];

    // Get cell volume and face areas
    volume = g_spTychoMesh->getCellVolume(cell);
    for(UINT face = 0; face < 4; face++) {
        area[face] = g_spTychoMesh->getFaceArea(cell, face) * 
                     g_spTychoMesh->getOmegaDotN(angle, cell, face);
    }
    
    // Setup LHS volume integrals
    for(UINT row = 0; row < 4; row++) {
    for(UINT col = 0; col < 4; col++) {
        double factor = (row == col) ? 2.0 : 1.0;
        matrix[row][col] = area[row] / 12.0 + sigmaTotal * volume / 20.0 * factor;
    }}
}


/*
    faceDependence
    Takes a lot of computational time, so it has been optimized.
    Beware of changing order of operations.
*/
static
void faceDependence(const UINT cell, const UINT angle,
                    double matrix[g_nVrtxPerCell][g_nVrtxPerCell])//,
                    //const Mat3<double> &localPsiBound, 
                    //double cellSource[g_nVrtxPerCell], const UINT group) 
{
    double area[4];
    UINT cellToFaceVrtx[4][4];
    UINT indices[4][3];
    
    
    // Get face areas
    for(UINT face = 0; face < 4; face++) {
        area[face] = g_spTychoMesh->getFaceArea(cell, face) * 
                     g_spTychoMesh->getOmegaDotN(angle, cell, face);
    }
    
    
    // Populate cellToFaceVrtx
    for(UINT face = 0; face < 4; face++) {
    for(UINT col = 0; col < 4; col++) {
        if(face != col)
            cellToFaceVrtx[face][col] = g_spTychoMesh->getCellToFaceVrtx(cell, face, col);
    }}
    
    
    // Populate indices
    indices[0][0] = 1; indices[0][1] = 2; indices[0][2] = 3;
    indices[1][0] = 0; indices[1][1] = 2; indices[1][2] = 3;
    indices[2][0] = 0; indices[2][1] = 1; indices[2][2] = 3;
    indices[3][0] = 0; indices[3][1] = 1; indices[3][2] = 2;
    
    
    // Update for fluxes.
    for(UINT face = 0; face < 4; face++) {
        
        // Outgoing flux
        if(area[face] > 0) {
            for(UINT rowIndex = 0; rowIndex < 3; rowIndex++) {
            for(UINT colIndex = 0; colIndex < 3; colIndex++) {
                UINT row = indices[face][rowIndex];
                UINT col = indices[face][colIndex];
                double factor = (row == col) ? 2.0 : 1.0;
                matrix[row][col] += area[face] / 12.0 * factor;
            }}
        }
        
        // Incoming flux
        else {
            /*for(UINT rowIndex = 0; rowIndex < 3; rowIndex++) {
            for(UINT colIndex = 0; colIndex < 3; colIndex++) {
                UINT row = indices[face][rowIndex];
                UINT col = indices[face][colIndex];
                double factor = (row == col) ? 2.0 : 1.0;
                UINT faceVertex = cellToFaceVrtx[face][col];
                for (UINT group = 0; group < g_nGroups; group++) {
				double psiNeighbor = localPsiBound(faceVertex, face, group);
                cellSource(row, group) -= area[face] / 12.0 * psiNeighbor * factor;
				}
            }}*/
        }
    }
}


/*
	faceDependence
*/
static
void faceDependence1(const UINT cell, const UINT angle,
					const Mat3<double> &localPsiBound,
					Mat2<double> &cellSource)
{
	double area[4];
	UINT cellToFaceVrtx[4][4];
	UINT indices[4][3];

	// Get face areas
	for(UINT face = 0; face < 4; face++) {
		area[face] = g_spTychoMesh->getFaceArea(cell, face) * 
					g_spTychoMesh->getOmegaDotN(angle, cell, face);
	}


	// Populate cellToFaceVrtx
	for(UINT face = 0; face < 4; face++) {
	for(UINT col = 0; col < 4; col++) {
		if(face != col)
			cellToFaceVrtx[face][col] = g_spTychoMesh->getCellToFaceVrtx(cell, face, col);
	}}
	

	// Populate indices
	indices[0][0] = 1; indices[0][1] = 2; indices[0][2] = 3;
    indices[1][0] = 0; indices[1][1] = 2; indices[1][2] = 3;
    indices[2][0] = 0; indices[2][1] = 1; indices[2][2] = 3;
    indices[3][0] = 0; indices[3][1] = 1; indices[3][2] = 2;


	// Update for fluxes
	for (UINT face = 0; face < 4; face++) {

		// Outgoing flux
		if(area[face] > 0) {
			/*for(UINT rowIndex = 0; rowIndex < 3; rowIndex++) {
			for(UINT colIndex = 0; colIndex < 3; colIndex++) {
				UINT row = indices[face][rowIndex];
				UINT col = indices[face][colIndex];
				double factor = (row == col) ? 2.0 : 1.0;
				matrix[row][col] += area[face] / 12.0 * factor;
			}}*/
		}

	// Incoming flux
		else {
			for(UINT rowIndex = 0; rowIndex < 3; rowIndex++) {
			for(UINT colIndex = 0; colIndex < 3; colIndex++) {
				UINT row = indices[face][rowIndex];
				UINT col = indices[face][colIndex];
				double factor = (row == col) ? 2.0 : 1.0;
				UINT faceVertex = cellToFaceVrtx[face][col];
				for (UINT group = 0; group < g_nGroups; group++) {
					double psiNeighbor = localPsiBound(faceVertex, face, group);
					cellSource(row, group) -= area[face] / 12.0 * psiNeighbor * factor;
				}
			}}
		}
	}
}


/*
    gaussElim4
*/
static 
void gaussElim4(double A[4][4], double b[4])
{
	// flag method: 
	// 0=Cramer's Rule
	// 1=MESA implementation of GLU library
	// 2=Gaussian No Pivot 
	// >2=original gaussian
	const int flag = 2;

	if (flag == 0) { // CRAMER'S RULE
		
		double tmp[12], src[16], dst[16], bCpy[4], det;

		// transpose matrix
		for (int i = 0; i < 4; i++) {
			src[i]      = A[i][0];
			src[i + 4]  = A[i][1];
			src[i + 8]  = A[i][2];
			src[i + 12] = A[i][3];
		}
    
		// calculate pairs for first 8 elements (cofactors)
		tmp[0] = src[10] * src[15];
		tmp[1] = src[11] * src[14];
		tmp[2] = src[9] * src[15];
		tmp[3] = src[11] * src[13];
		
		tmp[4] = src[9] * src[14];
		tmp[5] = src[10] * src[13];
		tmp[6] = src[8] * src[15];
		tmp[7] = src[11] * src[12];
		
		tmp[8] = src[8] * src[14];
		tmp[9] = src[10] * src[12];
		tmp[10] = src[8] * src[13];
		tmp[11] = src[9] * src[12];
    
		// calculate first 8 elements (cofactors)
		dst[0]  = tmp[0]*src[5] + tmp[3]*src[6] + tmp[4]*src[7];
		dst[0] -= tmp[1]*src[5] + tmp[2]*src[6] + tmp[5]*src[7];
		
		dst[1]  = tmp[1]*src[4] + tmp[6]*src[6] + tmp[9]*src[7];
		dst[1] -= tmp[0]*src[4] + tmp[7]*src[6] + tmp[8]*src[7];
		
		dst[2]  = tmp[2]*src[4] + tmp[7]*src[5] + tmp[10]*src[7];
		dst[2] -= tmp[3]*src[4] + tmp[6]*src[5] + tmp[11]*src[7];
		
		dst[3]  = tmp[5]*src[4] + tmp[8]*src[5] + tmp[11]*src[6];
		dst[3] -= tmp[4]*src[4] + tmp[9]*src[5] + tmp[10]*src[6];
		
		dst[4]  = tmp[1]*src[1] + tmp[2]*src[2] + tmp[5]*src[3];
		dst[4] -= tmp[0]*src[1] + tmp[3]*src[2] + tmp[4]*src[3];
		
		dst[5]  = tmp[0]*src[0] + tmp[7]*src[2] + tmp[8]*src[3];
		dst[5] -= tmp[1]*src[0] + tmp[6]*src[2] + tmp[9]*src[3];
		
		dst[6]  = tmp[3]*src[0] + tmp[6]*src[1] + tmp[11]*src[3];
		dst[6] -= tmp[2]*src[0] + tmp[7]*src[1] + tmp[10]*src[3];
		
		dst[7]  = tmp[4]*src[0] + tmp[9]*src[1] + tmp[10]*src[2];
		dst[7] -= tmp[5]*src[0] + tmp[8]*src[1] + tmp[11]*src[2];
		
		// calculate pairs for second 8 elements (cofactors)
		tmp[0]  = src[2]*src[7];
		tmp[1]  = src[3]*src[6];
		tmp[2]  = src[1]*src[7];
		tmp[3]  = src[3]*src[5];
		
		tmp[4]  = src[1]*src[6];
		tmp[5]  = src[2]*src[5];
		tmp[6]  = src[0]*src[7];
		tmp[7]  = src[3]*src[4];
		
		tmp[8]  = src[0]*src[6];
		tmp[9]  = src[2]*src[4];
		tmp[10] = src[0]*src[5];
		tmp[11] = src[1]*src[4];
		
		// calculate second 8 elements (cofactors)
		dst[8]   = tmp[0]*src[13] + tmp[3]*src[14] + tmp[4]*src[15];
		dst[8]  -= tmp[1]*src[13] + tmp[2]*src[14] + tmp[5]*src[15];
		
		dst[9]   = tmp[1]*src[12] + tmp[6]*src[14] + tmp[9]*src[15];
		dst[9]  -= tmp[0]*src[12] + tmp[7]*src[14] + tmp[8]*src[15];
		
		dst[10]  = tmp[2]*src[12] + tmp[7]*src[13] + tmp[10]*src[15];
		dst[10] -= tmp[3]*src[12] + tmp[6]*src[13] + tmp[11]*src[15];
		
		dst[11]  = tmp[5]*src[12] + tmp[8]*src[13] + tmp[11]*src[14];
		dst[11] -= tmp[4]*src[12] + tmp[9]*src[13] + tmp[10]*src[14];
		
		dst[12]  = tmp[2]*src[10] + tmp[5]*src[11] + tmp[1]*src[9];
		dst[12] -= tmp[4]*src[11] + tmp[0]*src[9] + tmp[3]*src[10];
		
		dst[13]  = tmp[8]*src[11] + tmp[0]*src[8] + tmp[7]*src[10];
		dst[13] -= tmp[6]*src[10] + tmp[9]*src[11] + tmp[1]*src[8];
		
		dst[14]  = tmp[6]*src[9] + tmp[11]*src[11] + tmp[3]*src[8];
		dst[14] -= tmp[10]*src[11] + tmp[2]*src[8] + tmp[7]*src[9];
		
		dst[15]  = tmp[10]*src[10] + tmp[4]*src[8] + tmp[9]*src[9];
		dst[15] -= tmp[8]*src[9] + tmp[11]*src[10] + tmp[5]*src[8];
		
		// calculate determinant
		det = src[0]*dst[0] + src[1]*dst[1] + src[2]*dst[2] + src[3]*dst[3];
    
		// calculate matrix inverse
		det = 1/det;
		for (int j = 0; j < 16; j++) {
			dst[j] *= det;
		}
    
		// get solution
		bCpy[0] = b[0];
		bCpy[1] = b[1];
		bCpy[2] = b[2];
		bCpy[3] = b[3];

		b[0] = dst[0]*bCpy[0]  + dst[1]*bCpy[1]  + dst[2]*bCpy[2]  + dst[3]*bCpy[3];
		b[1] = dst[4]*bCpy[0]  + dst[5]*bCpy[1]  + dst[6]*bCpy[2]  + dst[7]*bCpy[3];
		b[2] = dst[8]*bCpy[0]  + dst[9]*bCpy[1]  + dst[10]*bCpy[2] + dst[11]*bCpy[3];
		b[3] = dst[12]*bCpy[0] + dst[13]*bCpy[1] + dst[14]*bCpy[2] + dst[15]*bCpy[3];

	}


	else if (flag == 1) { // GLU LIBRARY-MESA IMPLEMENTATION

		int i;
		
		double inv[16], invOut[16], bCpy[4], det;    
    
		// 1d array
		double* m = &(A[0][0]);
    
		inv[0] = m[5]  * m[10] * m[15] -
		m[5]  * m[11] * m[14] -
		m[9]  * m[6]  * m[15] +
		m[9]  * m[7]  * m[14] +
		m[13] * m[6]  * m[11] -
		m[13] * m[7]  * m[10];
		
		inv[1] = -m[1]  * m[10] * m[15] +
		m[1]  * m[11] * m[14] +
		m[9]  * m[2] * m[15] -
		m[9]  * m[3] * m[14] -
		m[13] * m[2] * m[11] +
		m[13] * m[3] * m[10];
		
		inv[2] = m[1]  * m[6] * m[15] -
		m[1]  * m[7] * m[14] -
		m[5]  * m[2] * m[15] +
		m[5]  * m[3] * m[14] +
		m[13] * m[2] * m[7] -
		m[13] * m[3] * m[6];
		
		inv[3] = -m[1] * m[6] * m[11] +
		m[1] * m[7] * m[10] +
		m[5] * m[2] * m[11] -
		m[5] * m[3] * m[10] -
		m[9] * m[2] * m[7] +
		m[9] * m[3] * m[6];
    
		inv[4] = -m[4]  * m[10] * m[15] +
		m[4]  * m[11] * m[14] +
		m[8]  * m[6]  * m[15] -
		m[8]  * m[7]  * m[14] -
		m[12] * m[6]  * m[11] +
		m[12] * m[7]  * m[10];
		
		inv[5] = m[0]  * m[10] * m[15] -
		m[0]  * m[11] * m[14] -
		m[8]  * m[2] * m[15] +
		m[8]  * m[3] * m[14] +
		m[12] * m[2] * m[11] -
		m[12] * m[3] * m[10];
		
		inv[6] = -m[0]  * m[6] * m[15] +
		m[0]  * m[7] * m[14] +
		m[4]  * m[2] * m[15] -
		m[4]  * m[3] * m[14] -
		m[12] * m[2] * m[7] +
		m[12] * m[3] * m[6];
		
		inv[7] = m[0] * m[6] * m[11] -
		m[0] * m[7] * m[10] -
		m[4] * m[2] * m[11] +
		m[4] * m[3] * m[10] +
		m[8] * m[2] * m[7] -
		m[8] * m[3] * m[6];
		
		inv[8] = m[4]  * m[9] * m[15] -
		m[4]  * m[11] * m[13] -
		m[8]  * m[5] * m[15] +
		m[8]  * m[7] * m[13] +
		m[12] * m[5] * m[11] -
		m[12] * m[7] * m[9];
		
		inv[9] = -m[0]  * m[9] * m[15] +
		m[0]  * m[11] * m[13] +
		m[8]  * m[1] * m[15] -
		m[8]  * m[3] * m[13] -
		m[12] * m[1] * m[11] +
		m[12] * m[3] * m[9];
    
		inv[10] = m[0]  * m[5] * m[15] -
		m[0]  * m[7] * m[13] -
		m[4]  * m[1] * m[15] +
		m[4]  * m[3] * m[13] +
		m[12] * m[1] * m[7] -
		m[12] * m[3] * m[5];
		
		inv[11] = -m[0] * m[5] * m[11] +
		m[0] * m[7] * m[9] +
		m[4] * m[1] * m[11] -
		m[4] * m[3] * m[9] -
		m[8] * m[1] * m[7] +
		m[8] * m[3] * m[5];
		
		inv[12] = -m[4]  * m[9] * m[14] +
		m[4]  * m[10] * m[13] +
		m[8]  * m[5] * m[14] -
		m[8]  * m[6] * m[13] -
		m[12] * m[5] * m[10] +
		m[12] * m[6] * m[9];
    
		inv[13] = m[0]  * m[9] * m[14] -
		m[0]  * m[10] * m[13] -
		m[8]  * m[1] * m[14] +
		m[8]  * m[2] * m[13] +
		m[12] * m[1] * m[10] -
		m[12] * m[2] * m[9];
    
		inv[14] = -m[0]  * m[5] * m[14] +
		m[0]  * m[6] * m[13] +
		m[4]  * m[1] * m[14] -
		m[4]  * m[2] * m[13] -
		m[12] * m[1] * m[6] +
		m[12] * m[2] * m[5];
    
		inv[15] = m[0] * m[5] * m[10] -
		m[0] * m[6] * m[9] -
		m[4] * m[1] * m[10] +
		m[4] * m[2] * m[9] +
		m[8] * m[1] * m[6] -
		m[8] * m[2] * m[5];
   

		det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    
		det = 1.0/det;
    
		for (i = 0; i <16; i++) {
			invOut[i] = inv[i] * det;
		}
    	
		// get solution
		bCpy[0] = b[0];
		bCpy[1] = b[1];
		bCpy[2] = b[2];
		bCpy[3] = b[3];
		
		b[0] = invOut[0]*bCpy[0] + invOut[1]*bCpy[1] + invOut[2]*bCpy[2] + invOut[3]*bCpy[3];
		b[1] = invOut[4]*bCpy[0] + invOut[5]*bCpy[1] + invOut[6]*bCpy[2] + invOut[7]*bCpy[3];
		b[2] = invOut[8]*bCpy[0] + invOut[9]*bCpy[1] + invOut[10]*bCpy[2] + invOut[11]*bCpy[3];
		b[3] = invOut[12]*bCpy[0] + invOut[13]*bCpy[1] + invOut[14]*bCpy[2] + invOut[15]*bCpy[3];
	}


	else if (flag == 2) { // GAUSSIAN-NO PIVOT

		double tmp;
		
		// Normalize first row
		tmp = 1.0/A[0][0];
		A[0][0] = 1.0;
		A[0][1] = A[0][1] * tmp;
		A[0][2] = A[0][2] * tmp;
		A[0][3] = A[0][3] * tmp;
		b[0] = b[0] * tmp;

		// Set column zero to 0.0
		tmp = A[1][0];
		A[1][0] = 0.0;
		A[1][1] = A[1][1] - A[0][1] * tmp;
		A[1][2] = A[1][2] - A[0][2] * tmp;
		A[1][3] = A[1][3] - A[0][3] * tmp;
		b[1] = b[1] - b[0] * tmp;	

		tmp = A[2][0];
		A[2][0] = 0.0;
		A[2][1] = A[2][1] - A[0][1] * tmp;
		A[2][2] = A[2][2] - A[0][2] * tmp;
		A[2][3] = A[2][3] - A[0][3] * tmp;
		b[2] = b[2] - b[0] * tmp;
		
		tmp = A[3][0];
		A[3][0] = 0.0;
		A[3][1] = A[3][1] - A[0][1] * tmp;
		A[3][2] = A[3][2] - A[0][2] * tmp;
		A[3][3] = A[3][3] - A[0][3] * tmp;
		b[3] = b[3] - b[0] * tmp;
		
		// Normalize second row
		tmp = 1.0/A[1][1];
		A[1][1] = 1.0;
		A[1][2] = A[1][2] * tmp;
		A[1][3] = A[1][3] * tmp;
		b[1] = b[1] * tmp;
		
		// Set column one to 0.0
		tmp = A[2][1];
		A[2][1] = 0.0;
		A[2][2] = A[2][2] - A[1][2] * tmp;
		A[2][3] = A[2][3] - A[1][3] * tmp;
		b[2] = b[2] - b[1] * tmp;
		
		tmp = A[3][1];
		A[3][1] = 0.0;
		A[3][2] = A[3][2] - A[1][2] * tmp;
		A[3][3] = A[3][3] - A[1][3] * tmp;
		b[3] = b[3] - b[1] * tmp;
		
		// Normalize third row
		tmp = 1.0/A[2][2];
		A[2][2] = 1.0;
		A[2][3] = A[2][3] * tmp;
		b[2] = b[2] * tmp;
		
		// Set column two to 0.0
		tmp = A[3][2];
		A[3][2] = 0.0;
		A[3][3] = A[3][3] - A[2][3] * tmp;
		b[3] = b[3] - b[2] * tmp;

		// Backward Solve
		b[3] = b[3]/A[3][3];	
		b[2] = b[2] - A[2][3]*b[3];
		b[1] = b[1] - A[1][3]*b[3] - A[1][2]*b[2];
		b[0] = b[0] - A[0][3]*b[3] - A[0][2]*b[2] - A[0][1]*b[1]; 

	}


	
	else { // ORIGINAL GAUSSIAN
    	const int n = 4;
    
		for (int column = 0; column < n-1; ++column) {
			int rowmax = column;
			double colmax = fabs(A[column][column]);
			for (int row = column+1; row < n; ++row) {
				double temp = fabs(A[row][column]);
				if (temp > colmax)  {
					rowmax = row;
					colmax = temp;
				}
			}

			if (rowmax != column) {
				for (int column2 = 0; column2 < n; ++column2) {
					double temp = A[rowmax][column2];
					A[rowmax][column2] = A[column][column2];
					A[column][column2] = temp;
				}
				double temp = b[rowmax];
				b[rowmax] = b[column];
				b[column] = temp;
			}

			Assert(A[column][column] != 0.);
			A[column][column] = 1./A[column][column];

			for (int row = column+1; row < n; ++row)
				A[row][column] *= A[column][column];

			for (int column2 = 0; column2 <= column; ++column2) {
			for (int row = column2+1; row < n; ++row) {
				A[row][column+1] -= A[row][column2]*A[column2][column+1];
			}}
		}

    	Assert(A[n-1][n-1] != 0.);
    	A[n-1][n-1] = 1./A[n-1][n-1];

		for (int column = 0; column < n-1; ++column) {
		for (int row = column+1; row < n; ++row) {
			b[row] -= A[row][column]*b[column];
		}}

		for (int column = n-1; column >= 0; --column) {
			b[column] *= A[column][column];
			for (int row = column-1; row >= 0; --row)
				b[row] -= A[row][column]*b[column];
		}
	} 
}


// Global functions
namespace Transport
{

/*
    solve
*/
void solve(const UINT cell, const UINT angle, const double sigmaTotal,
           const Mat3<double> &localPsiBound, const Mat2<double> &localSource,
           Mat2<double> &localPsi)
{
	Mat2<double> cellSource(g_nVrtxPerCell, g_nGroups, 0.0);
	double matrix[g_nVrtxPerCell][g_nVrtxPerCell] = {0.0};
	double matrix2[g_nVrtxPerCell][g_nVrtxPerCell] = {0.0};
	double solution[g_nVrtxPerCell];
	
    //for(UINT group = 0; group < g_nGroups; group++) {
        //double matrix[g_nVrtxPerCell][g_nVrtxPerCell] = {0.0};
        //double cellSource[g_nVrtxPerCell] = {0.0};
        //double solution[g_nVrtxPerCell];
        
        // form local source term
        calcLocalSource(cell, localSource, cellSource);//, group);
        
        // form streaming-plus-collision portion of matrix
        streamPlusColl(cell, angle, sigmaTotal, matrix);
        
        // form dependencies on incoming (outgoing) faces
        faceDependence(cell, angle, matrix);//, localPsiBound, cellSource, group);
		faceDependence1(cell, angle, localPsiBound, cellSource);
        
	for (UINT group = 0; group < g_nGroups; group++) {
        // solve matrix
        for (UINT vertex = 0; vertex < g_nVrtxPerCell; ++vertex)
            solution[vertex] = cellSource(vertex, group);
		for (UINT i = 0; i < 4; i++) {
		for (UINT j = 0; j < 4; j++) {
			matrix2[i][j] = matrix[i][j];
		}}
        gaussElim4(matrix2, solution);
        
        // put local solution onto global solution
        for (UINT vertex = 0; vertex < g_nVrtxPerCell; ++vertex)
            localPsi(vertex, group) = solution[vertex];
    }
}


/*
    populateLocalPsiBound
    
    Put data from neighboring cells into localPsiBound(fvrtx, face, group).
*/
void populateLocalPsiBound(const UINT angle, const UINT cell, 
                           const PsiData &__restrict psi, const PsiData & __restrict psiBound,
                           Mat3<double> &__restrict localPsiBound)
{
    // Default to 0.0
    for (UINT i = 0; i < localPsiBound.size(); i++)
		localPsiBound[i] = 0.0;
    
    // Populate if incoming flux
    #pragma ivdep
    for (UINT group = 0; group < g_nGroups; group++) {
    for (UINT face = 0; face < g_nFacePerCell; face++) {
        if (g_spTychoMesh->isIncoming(angle, cell, face)) {
            UINT neighborCell = g_spTychoMesh->getAdjCell(cell, face);
            
            // In local mesh
            if (neighborCell != TychoMesh::BOUNDARY_FACE) {
                for (UINT fvrtx = 0; fvrtx < g_nVrtxPerFace; fvrtx++) {
                    UINT neighborVrtx = 
                        g_spTychoMesh->getNeighborVrtx(cell, face, fvrtx);
                    localPsiBound(fvrtx, face, group) = 
                        psi(neighborVrtx, angle, neighborCell, group);
                }
            }
            
            // Not in local mesh
            else if (g_spTychoMesh->getAdjRank(cell, face) != TychoMesh::BAD_RANK) {
                for (UINT fvrtx = 0; fvrtx < g_nVrtxPerFace; fvrtx++) {
                    UINT side = g_spTychoMesh->getSide(cell, face);
                    localPsiBound(fvrtx, face, group) = 
                        psiBound(side, angle, fvrtx, group);
                }
            }
        }
    }}
}


} // End namespace Transport


