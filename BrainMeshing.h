/* ********************************************************************
 * BrainMeshing
 *
 * Copyright 2009 Joachim Giard
 * Université catholique de Louvain, Belgium
 * Apache License, Version 2.0
 *
 * Class containing methods to construct a 3D meshes from
 * a 3D image with various parameters.
 *
 * *******************************************************************/

#ifndef GAUSSIAN_BRAIN_MESHING_H_
#define GAUSSIAN_BRAIN_MESHING_H_

#include <vtkPolyData.h>
#include <vtkImageData.h>

class BrainMeshing
{
public:
    //constructor and destructor
    BrainMeshing(vtkImageData* inputImage);
    ~BrainMeshing();

    vtkPolyData* computeInflatedPolyData();

    void SetShrinkFactor(double sf);
    void SetProbeRadius(double pr);
    void SetThreshold(double th);


private:
    vtkImageData* m_inputImage;

    float m_shrinkFactor;
    float m_probeRadius;
    float m_threshold;

};


#endif
