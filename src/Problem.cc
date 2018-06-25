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


#include "Problem.hh"
#include "Comm.hh"
#include <math.h>


// cubeSize assumes using cube meshes in util folder
static const float cubeSize = 100.0;


/*
    hatSource
*/
static
void hatSource(PsiData &source)
{
    for(UINT cell = 0; cell < g_nCells; cell++) {
    for(UINT angle = 0; angle < g_nAngles; angle++) {
    for(UINT vrtx = 0; vrtx < g_nVrtxPerCell; vrtx++) {
        
        UINT node = g_tychoMesh->getCellNode(cell, vrtx);
        float x = g_tychoMesh->getNodeCoord(node, 0) - cubeSize / 2.0;
        float y = g_tychoMesh->getNodeCoord(node, 1) - cubeSize / 2.0;
        float z = g_tychoMesh->getNodeCoord(node, 2) - cubeSize / 2.0;
        
        float xi  = g_quadrature->getXi(angle);
        float eta = g_quadrature->getEta(angle);
        float mu  = g_quadrature->getMu(angle);
        
        float c = sqrt(x*x + y*y + z*z);
        
        for(UINT group = 0; group < g_nGroups; group++) {
            if(c <= 30.0) {
                source(group, vrtx, angle, cell) = 
                    - x / (30.0*c) * xi - y / (30.0*c) * eta - z / (30.0*c) * mu
                    + (g_sigmaT[cell] - g_sigmaS[cell]) * (1.0 - c / 30.0);
            }
            else {
                source(group, vrtx, angle, cell) = 0.0;
            }
        }
    }}}
}


namespace Problem
{

/*
    hatL2Error
*/
float hatL2Error(const PsiData &psi)
{
    float diff = 0.0;
    float norm = 0.0;
    
    
    for(UINT cell = 0; cell < g_nCells; cell++) {
    for(UINT angle = 0; angle < g_nAngles; angle++) {
    for(UINT group = 0; group < g_nGroups; group++) {
        float x = 0.0;
        float y = 0.0;
        float z = 0.0;
        float psiVal = 0.0;
        
        for(UINT vrtx = 0; vrtx < g_nVrtxPerCell; vrtx++) {
            UINT node = g_tychoMesh->getCellNode(cell, vrtx);
            x += g_tychoMesh->getNodeCoord(node, 0) - cubeSize / 2.0;
            y += g_tychoMesh->getNodeCoord(node, 1) - cubeSize / 2.0;
            z += g_tychoMesh->getNodeCoord(node, 2) - cubeSize / 2.0;
            psiVal += psi(group, vrtx, angle, cell);
        }
        
        x = x / g_nVrtxPerCell;
        y = y / g_nVrtxPerCell;
        z = z / g_nVrtxPerCell;
        psiVal = psiVal / g_nVrtxPerCell;
        
        float c = sqrt(x*x + y*y + z*z);
        float psiAct = 0.0;
        if(c <= 30.0) {
            psiAct = 1.0 - c / 30.0;
        }
        
        float localDiff = psiVal - psiAct;
        norm += psiVal * psiVal;
        diff += localDiff * localDiff;
    }}}
    
    
    Comm::gsum(norm);
    Comm::gsum(diff);
    
    return sqrt(diff / norm);
}


/*
    getProblemSource
*/
void getSource(PsiData &source)
{
    hatSource(source);
}


/*
    createCrossSections
*/
void createCrossSections(std::vector<float> &sigmaT, 
                         std::vector<float> &sigmaS,
                         float sigmaT1, float sigmaS1,
                         float sigmaT2, float sigmaS2)
{
    sigmaT.resize(g_nCells);
    sigmaS.resize(g_nCells);

    for (UINT cell = 0; cell < g_nCells; cell++) {
        
        UINT material = g_tychoMesh->getCellMaterial(cell);
        if (material == 1) {
            sigmaT[cell] = sigmaT1;
            sigmaS[cell] = sigmaS1;
        }
        else if (material == 2) {
            sigmaT[cell] = sigmaT2;
            sigmaS[cell] = sigmaS2;
        }
        else {
            Insist(false, "Material index out of bounds.");
        }
    }
}


} // End namespace Problem
