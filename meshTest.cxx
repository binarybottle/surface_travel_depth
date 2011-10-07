/* ********************************************************************
 * meshTest
 *
 * Copyright 2009 Joachim Giard
 * Université catholique de Louvain, Belgium
 * Apache License, Version 2.0
 * 
 * *******************************************************************/

#include "MeshAnalyser.cxx"
#include <vtkDelaunay3D.h>
#include <vtkUnstructuredGrid.h>
#include <vtkCurvatures.h>
#include <vtkSphereSource.h>

#include <iostream>
#include <fstream>
#include <stdio.h>

int main(int argc, char** argv)
{
	time_t start= time(NULL);

	MeshAnalyser* ma=new MeshAnalyser(argv[1]);

	//~ int nbp=ma->GetNumberOfPoints();

	ma->ComputeTravelDepth(true);
	
	ma->WriteIntoFile((char*)"test.vtk",(char*)"depth");
	
	cout<<"Elapsed time (meshTest): "<<time(NULL)-start<<" s"<<endl;
	
	return 0;
}


