#include "MeshAnalyser.h"
#include "BrainMeshing.h"

#include "vtkMetaImageReader.h"

#include <vtkWindowedSincPolyDataFilter.h>
#include <vtkUnstructuredGrid.h>

#include <vtkDelaunay3D.h>
#include <vtkDecimatePro.h>

#include <vtkImageDifference.h>

#include <vtkXMLPolyDataReader.h>

#include <vtkTransform.h>
#include <vtkImageReader.h>

#include "FsSurfaceReader.h"

#include <iostream>
#include <fstream>
#include <stdio.h>

int main(int argc, char** argv)
{
    time_t start= time(NULL);

    FsSurfaceReader* fsr1 = new FsSurfaceReader(argv[1]);

    // the reader for the vtp files
/*    vtkXMLPolyDataReader* reader = vtkXMLPolyDataReader::New();
    reader->SetFileName(argv[1]);
    reader->Update();*/

    MeshAnalyser* ma = new MeshAnalyser(fsr1->GetVTKData());

    ma->ComputeInflatedMesh();

    //may be replaced by ComputeTravelDepthFromInflated
    //which is slower but does not allow to cross the surface
    ma->ComputeEuclideanDepthFromInflated(true);

    ma->WriteIntoFile((char*)"closed.vtk",(char*)"closed");
    ma->WriteIntoFile((char*)"depth.vtk",(char*)"euclideanDepth");

    cout<<"Elapsed time (meshTest): "<<time(NULL)-start<<" s"<<endl;
    return 0;



}



