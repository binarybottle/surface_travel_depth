#include "MeshAnalyser.cxx"
#include "BrainMeshing.cxx"

#include "vtkMetaImageReader.h"

#include <vtkWindowedSincPolyDataFilter.h>
#include <vtkUnstructuredGrid.h>

#include <vtkDelaunay3D.h>
#include <vtkDecimatePro.h>


#include <iostream>
#include <fstream>
#include <stdio.h>

int main(int argc, char** argv)
{
    time_t start= time(NULL);

    vtkMetaImageReader* mir = vtkMetaImageReader::New();
    mir->SetFileName(argv[1]);
    mir->Update();

    cout<<"image loaded"<<endl;

    BrainMeshing* bm = new BrainMeshing(mir->GetOutput());
    bm->SetProbeRadius(0);
    bm->SetShrinkFactor(2);
    bm->SetThreshold(70);

    MeshAnalyser* ma = new MeshAnalyser(bm->computeInflatedPolyData());
    cout<<"mesh computed"<<endl;

    BrainMeshing* bmr = new BrainMeshing(mir->GetOutput());
    bmr->SetProbeRadius(5);
    bmr->SetShrinkFactor(3);
    bmr->SetThreshold(60);
    cout<<"reference mesh computed"<<endl;

    MeshAnalyser* mar = new MeshAnalyser(bmr->computeInflatedPolyData());
    mar->WriteIntoFile((char*)"testHull.vtk");

    ma->ComputeTravelDepth(true, bmr->computeInflatedPolyData());

    cout<<"depth computed"<<endl;

    ma->WriteIntoFile((char*)"test.vtk",(char*)"depth");

    cout<<"Written in file"<<endl;

    cout<<"Elapsed time (meshTest): "<<time(NULL)-start<<" s"<<endl;
    return 0;



}



