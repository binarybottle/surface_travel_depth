/* ********************************************************************
 * MeshAnalyser
 *
 * Copyright 2009 Joachim Giard
 * Université catholique de Louvain, Belgium
 * Apache License, Version 2.0
 *
 * Class containing various methods to apply on 3D meshes. The
 * meshanalyser object takes a vtkPolyData object as input
 *
 * *******************************************************************/


#include "MeshAnalyser.h"
#include <vtkPolyDataReader.h>
#include <vtkPolyDataWriter.h>
#include <vtkHull.h>
#include <vtkPointLocator.h>
#include <vtkCellArray.h>
#include <vtkPointSet.h>
#include <vtkDataSet.h>
#include <vtkPoints.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkQuadricDecimation.h>
#include <map>
#include <vtkPolyDataNormals.h>
#include <vtkDataArray.h>
#include <algorithm>
#include <vtkSphereSource.h>
#include <vtkDelaunay3D.h>
#include <vtkUnstructuredGrid.h>

#include <vtkPolyDataToImageStencil.h>
#include <vtkImageContinuousDilate3D.h>
#include <vtkImageContinuousErode3D.h>
#include <vtkPolyDataConnectivityFilter.h>

#include <vtkSmartPointer.h>

#include <vtkImageGridSource.h>

#include <vtkImageStencil.h>

#include <vtkImageStencilData.h>

#include <vtkMarchingContourFilter.h>

#include <vtkTriangleFilter.h>
#include <vtkImageData.h>

#include <vtkCurvatures.h>

#include <vtkExtractEdges.h>

MeshAnalyser::MeshAnalyser(char* fileName)
{

    vtkPolyDataReader* reader=vtkPolyDataReader::New();
    reader->SetFileName(fileName);
    reader->Update();

    vtkTriangleFilter* tf = vtkTriangleFilter::New();
    tf->SetInput(reader->GetOutput());
    tf->Update();

    this->mesh=vtkPolyData::New();
    this->mesh->DeepCopy(tf->GetOutput());

    reader->Delete();

    Initialize();

}

MeshAnalyser::MeshAnalyser(vtkPolyData* mesh)
{
    vtkTriangleFilter* tf = vtkTriangleFilter::New();
    tf->SetInput(mesh);
    tf->Update();

    this->mesh=vtkPolyData::New();
    this->mesh->DeepCopy(tf->GetOutput());

    Initialize();

}


void MeshAnalyser::Initialize()
{
    this->simpl=vtkPolyData::New();
    this->depth=vtkDoubleArray::New();
    this->euclideanDepth=vtkDoubleArray::New();
    this->pointSurf=vtkDoubleArray::New();
    this->pointSurfSimple=vtkDoubleArray::New();
    this->geoDistRing=vtkDoubleArray::New();
    this->geoDistRingSimple=vtkDoubleArray::New();
    this->curv=vtkDoubleArray::New();
    this->gCurv=vtkDoubleArray::New();
    this->totalSurface=0;
    this->totalSurfaceSimple=0;
    this->inRing=vtkIdList::New();
    this->inRingSimple=vtkIdList::New();
    this->predGeo=vtkIdList::New();
    this->test=vtkDoubleArray::New();
    this->close=vtkIdList::New();
    this->voronoiBin=vtkIdList::New();
    this->closedMesh=vtkPolyData::New();
    this->meshLocator=vtkCellLocator::New();
    this->medialSurface=vtkPolyData::New();

    this->nbPoints=this->mesh->GetNumberOfPoints();

    ComputePointSurface();
    ComputeNormals();
}

MeshAnalyser::~MeshAnalyser()
{
    this->mesh->Delete();
    //	this->simpl->Delete();
    this->depth->Delete();
    this->pointSurf->Delete();
    this->pointSurfSimple->Delete();
    this->geoDistRing->Delete();
    this->geoDistRingSimple->Delete();
    this->curv->Delete();
    this->gCurv->Delete();
    this->inRing->Delete();
    this->inRingSimple->Delete();
    this->predGeo->Delete();
    this->test->Delete();
    this->close->Delete();
    this->voronoiBin->Delete();
    this->closedMesh->Delete();
    this->euclideanDepth->Delete();
    this->meshLocator->Delete();
    //        this->medialSurface->Delete();

    //this->visi->Delete();
}

void MeshAnalyser::SetMesh(char* fileName)
{
    vtkPolyDataReader* reader=vtkPolyDataReader::New();
    reader->SetFileName(fileName);
    reader->Update();

    this->mesh->DeepCopy(reader->GetOutput());
    this->nbPoints=reader->GetOutput()->GetNumberOfPoints();
    reader->Delete();
}

void MeshAnalyser::SetMesh(vtkPolyData* mesh)
{
    this->mesh->DeepCopy(mesh);
    this->nbPoints=mesh->GetNumberOfPoints();
}

void MeshAnalyser::GetPointNeighborsSimple(vtkIdType id, vtkIdList* neighbors)
{
    vtkIdList *cellids = vtkIdList::New();
    this->simpl->GetPointCells(id,cellids);
    neighbors->Reset();
    for (int i = 0; i < cellids->GetNumberOfIds(); i++)
    {
        for(int j = 0; j < this->simpl->GetCell(cellids->GetId(i))->GetNumberOfPoints(); j++)
        {
            if(this->simpl->GetCell(cellids->GetId(i))->GetPointId(j)<this->nbPointsSimple)
            {
                neighbors->InsertUniqueId(this->simpl->GetCell(cellids->GetId(i))->GetPointId(j));
            }
        }
    }
    cellids->Delete();
}

void MeshAnalyser::GetPointNeighbors(vtkIdType id, vtkIdList* neighbors)
{
    vtkIdList *cellids = vtkIdList::New();
    this->mesh->GetPointCells(id,cellids);
    neighbors->Reset();
    for (int i = 0; i < cellids->GetNumberOfIds(); i++)
    {
        for(int j = 0; j < this->mesh->GetCell(cellids->GetId(i))->GetNumberOfPoints(); j++)
        {
            if(this->mesh->GetCell(cellids->GetId(i))->GetPointId(j)<this->nbPoints)
            {
                neighbors->InsertUniqueId(this->mesh->GetCell(cellids->GetId(i))->GetPointId(j));
            }
        }
    }
    cellids->Delete();
}

void MeshAnalyser::GeoDistRing(vtkIdType st, double maxDist, double approx)
{
    vtkDoubleArray* tempDist=vtkDoubleArray::New();

    //number of closest points in this->simpl to relate to each point of this->mesh
    int N=3;
    //minimal ray to effectuate front propagation without passing through this->simpl (to be upgraded)
    double ray=0;
    double point1[3],point2[3], pointt[3],ec;
    vtkIdType cellId;
    int subId;
    double dist2;
    vtkIdList* pointIds = vtkIdList::New();

    //Ids and dist to points from this->simpl close to st
    double closeIds[N];
    double closeDists[N];

    //compute a decimated mesh if not already done
    if(this->approxFactor!=approx)
    {
        Simplify(approx);
        this->approxFactor=approx;

        cout<<endl<<"Decimation done"<<endl;

        //vtkPointLocator* pl=vtkPointLocator::New();
        vtkCellLocator* pl=vtkCellLocator::New();
        pl->SetDataSet(this->simpl);
        pl->BuildLocator();

        this->allDist.clear();

        //temporary list of N closest points
        vtkIdList* Nclo = vtkIdList::New();

        cout<<"computing neighborhood"<<endl;
        for(int i=0;i<this->nbPoints;i++)
        {
            this->mesh->GetPoint(i,point1);
            //pl->FindClosestNPoints(N, point1, Nclo);
            pl->FindClosestPoint(point1, pointt, cellId, subId, dist2);
            this->simpl->GetCellPoints(cellId,pointIds);
            //N=pointIds->GetNumberOfIds();
            this->test->InsertNextValue(pointIds->GetId(2));
            for(int j=0;j<N;j++)
            {
                //this->close->InsertNextId(Nclo->GetId(j));
                this->close->InsertNextId(pointIds->GetId(j));
            }
        }
    }
    else if(this->close->GetNumberOfIds()==0)
    {
        //vtkPointLocator* pl=vtkPointLocator::New();
        vtkCellLocator* pl=vtkCellLocator::New();
        pl->SetDataSet(this->simpl);
        pl->BuildLocator();

        this->allDist.clear();

        //temporary list of N closest points
        vtkIdList* Nclo = vtkIdList::New();

        cout<<"computing neighborhood"<<endl;
        for(int i=0;i<this->nbPoints;i++)
        {
            this->mesh->GetPoint(i,point1);
            //pl->FindClosestNPoints(N, point1, Nclo);
            pl->FindClosestPoint(point1, pointt, cellId, subId, dist2);
            this->simpl->GetCellPoints(cellId,pointIds);
            //N=pointIds->GetNumberOfIds();
            this->test->InsertNextValue(cellId);
            for(int j=0;j<N;j++)
            {
                //this->close->InsertNextId(Nclo->GetId(j));
                this->close->InsertNextId(pointIds->GetId(j));
            }
        }

    }

    this->mesh->GetPoint(st,point1);

    for(int j=0;j<N;j++)
    {
        this->simpl->GetPoint(this->close->GetId(st*N+j),point2);
        ec=vtkMath::Distance2BetweenPoints(point1,point2);
        ec=sqrt(ec);
        closeIds[j]=this->close->GetId(st*N+j);
        closeDists[j]=ec;
        if(ec>ray)ray=ec;
    }

    double md=0;

    //Compute all distances on this->simpl
    if(this->allDist.empty())
    {
        //cout<<"computing distances on this->simpl"<<endl;
        for(int i=0;i<this->nbPointsSimple;i++)
        {
            GeoDistRingSimple(i,maxDist);
            for(int j=0;j<this->nbPointsSimple;j++)
            {
                this->allDist.push_back(this->geoDistRingSimple->GetValue(j));
            }
        }
    }

    //cout<<"distances on this->simpl computed"<<endl;


    int nbInRing;
    float minDist,sd,ed,nd;
    vtkIdType sn,en,curId1,curId2;

    this->geoDistRing->Reset();

    //set all distances to max
    for(int i=0;i<this->nbPoints;i++)
    {
        tempDist->InsertNextValue(maxDist);
    }

    //cout<<"browse and update"<<endl;
    //browse points in this->simpl and update close points of this->mesh if necessary
    for(int i=0;i<this->nbPoints;i++)
    {

        this->mesh->GetPoint(i,point1);

        for(int j=0;j<N;j++)
        {
            curId1=this->close->GetId(i*N+j);
            this->simpl->GetPoint(curId1,point2);	//closest neighbor of the arrival point
            ec=vtkMath::Distance2BetweenPoints(point1,point2);
            ec=sqrt(ec);					//distance between arrivel point and closest simple point

            for(int k=0;k<N;k++)
            {
                curId2=closeIds[k];					//closest neighbor of the starting point
                nd=this->allDist[curId1*this->nbPointsSimple+curId2];
                //nd=10;
                if(nd<maxDist)
                {
                    sd=ec+nd+closeDists[k];
                    //sd=ec;
                    //sd=closeDists[k];
                    //sd=nd;
                    if(tempDist->GetValue(i)>sd)
                    {
                        tempDist->SetValue(i,sd);
                    }
                }
            }
        }
    }

    GeoDistRing(st,ray);
    nbInRing=this->inRing->GetNumberOfIds();

    for(int j=0;j<this->nbPoints;j++)
    {
        if(this->geoDistRing->GetValue(j)>=ray)
        {
            this->geoDistRing->SetValue(j,tempDist->GetValue(j));
        }
    }
    this->geoDistRing->SetValue(st,0);
    tempDist->Delete();
    //cout<<"distance computed"<<endl;
}


void MeshAnalyser::GeoDistRingSimple(vtkIdType stPoint, double maxDist)
{		
    multimap<float,vtkIdType> frontier;

    //temporary indexes
    vtkIdType curId;
    vtkIdType curFrontierId;

    vtkIdList* curNeib=vtkIdList::New();
    int nbNeib;

    this->geoDistRingSimple->Reset();
    this->geoDistRingSimple->Squeeze();

    this->inRingSimple->Reset();
    this->inRingSimple->Squeeze();

    //fill the distance vector with maximum value
    for(int i=0;i<this->nbPointsSimple;i++)
    {
        this->geoDistRingSimple->InsertNextValue(maxDist);
    }

    //in the front set, there is only the starting point
    frontier.insert(pair<float,vtkIdType>(0,stPoint));
    this->inRingSimple->InsertNextId(stPoint);
    int nbFrontierPoints=1;
    this->geoDistRingSimple->SetValue(stPoint,0);

    double point1[3], point2[3];
    double ec=0;
    double upgDist;

    multimap<float,vtkIdType>::iterator iter;

    //iteration screening the neighbors of the front point with the smallest value
    while(!frontier.empty())
    {
        iter=frontier.begin();
        curFrontierId=iter->second;

        this->simpl->GetPoint(curFrontierId,point1);
        curNeib->Reset();
        curNeib->Squeeze();
        GetPointNeighborsSimple(curFrontierId,curNeib);
        nbNeib=curNeib->GetNumberOfIds();
        //neighbors of the front point
        for(int j=0;j<nbNeib;j++)
        {
            curId=curNeib->GetId(j);

            this->simpl->GetPoint(curId,point2);
            ec=vtkMath::Distance2BetweenPoints(point1,point2);
            ec=sqrt(ec);
            upgDist=this->geoDistRingSimple->GetValue(curFrontierId)+ec;
            //distance is upgraded if necessary
            if(upgDist<this->geoDistRingSimple->GetValue(curId))
            {
                this->geoDistRingSimple->SetValue(curId,upgDist);
                //if point is upgraded, it enters in the front
                frontier.insert(pair<float,vtkIdType>(upgDist,curId));
                this->inRingSimple->InsertUniqueId(curId);
            }
        }
        frontier.erase(iter);
    }
    curNeib->Delete();
}


void MeshAnalyser::GeoDistRing(vtkIdType stPoint, double maxDist)
{

    this->geoCenter=stPoint;

    multimap<float,vtkIdType> frontier;

    //temporary indexes
    vtkIdType curId;
    vtkIdType curFrontierId;

    vtkIdList* curNeib=vtkIdList::New();
    int nbNeib;

    this->geoDistRing->Reset();
    this->geoDistRing->Squeeze();

    this->inRing->Reset();
    //this->inRing->Squeeze();

    this->predGeo->Reset();

    //fill the distance vector with maximum value
    for(int i=0;i<this->nbPoints;i++)
    {
        this->geoDistRing->InsertNextValue(maxDist);
        this->predGeo->InsertNextId(-1);
    }

    //in the front set, there is only the starting point
    frontier.insert(pair<float,vtkIdType>(0,stPoint));
    this->inRing->InsertNextId(stPoint);
    int nbFrontierPoints=1;
    this->geoDistRing->SetValue(stPoint,0);

    double point1[3], point2[3];
    double ec=0;
    double upgDist;

    multimap<float,vtkIdType>::iterator iter;

    //iteration screening the neighbors of the front point with the smallest value
    while(!frontier.empty())
    {
        iter=frontier.begin();
        curFrontierId=iter->second;

        this->mesh->GetPoint(curFrontierId,point1);
        curNeib->Reset();
        curNeib->Squeeze();
        GetPointNeighbors(curFrontierId,curNeib);
        nbNeib=curNeib->GetNumberOfIds();
        //neighbors of the front point
        for(int j=0;j<nbNeib;j++)
        {
            curId=curNeib->GetId(j);

            this->mesh->GetPoint(curId,point2);
            ec=vtkMath::Distance2BetweenPoints(point1,point2);
            ec=sqrt(ec);
            upgDist=this->geoDistRing->GetValue(curFrontierId)+ec;
            //distance is upgraded if necessary
            if(upgDist<this->geoDistRing->GetValue(curId))
            {
                this->geoDistRing->SetValue(curId,upgDist);
                //if point is upgraded, it enters in the front
                frontier.insert(pair<float,vtkIdType>(upgDist,curId));
                this->inRing->InsertUniqueId(curId);
                this->predGeo->SetId(curId,curFrontierId);
            }
        }
        frontier.erase(iter);
    }
    curNeib->Delete();
}

void MeshAnalyser::WriteIntoFile(char* fileName, char* prop)
{
    vtkPolyDataWriter* writer=vtkPolyDataWriter::New();
    writer->SetFileName(fileName);

    if(strcmp("geoDist",prop)==0) this->mesh->GetPointData()->SetScalars(this->geoDistRing);
    else if(strcmp("depth",prop)==0) this->mesh->GetPointData()->SetScalars(this->depth);
    else if(strcmp("euclideanDepth",prop)==0) this->mesh->GetPointData()->SetScalars(this->euclideanDepth);
    else if(strcmp("curv",prop)==0) this->mesh->GetPointData()->SetScalars(this->curv);
    else if(strcmp("gCurv",prop)==0) this->mesh->GetPointData()->SetScalars(this->gCurv);
    else if(strcmp("test",prop)==0) this->mesh->GetPointData()->SetScalars(this->test);
    else if(strcmp("surf",prop)==0) this->mesh->GetPointData()->SetScalars(this->pointSurf);
    //convert the voronoi bin indices from Id to double
    else if(strcmp("voronoi",prop)==0)
    {
        vtkDoubleArray* value=vtkDoubleArray::New();

        for(int i=0;i<this->nbPoints;i++)
        {
            value->InsertNextValue(this->voronoiBin->GetId(i));
        }
        this->mesh->GetPointData()->SetScalars(value);
        value->Delete();

    }
    else if(strcmp("1color",prop)==0)
    {
        vtkDoubleArray* value=vtkDoubleArray::New();

        for(int i=0;i<this->nbPoints;i++)
        {
            value->InsertNextValue(1);
        }
        this->mesh->GetPointData()->SetScalars(value);
        value->Delete();

    }
    //If no valid code is used, the index of the point is the scalar
    else
    {
        vtkDoubleArray* noValue=vtkDoubleArray::New();

        for(int i=0;i<this->nbPoints;i++)
        {
            noValue->InsertNextValue(i);
        }
        this->mesh->GetPointData()->SetScalars(noValue);
        noValue->Delete();
    }


    this->mesh->Update();
    if(strcmp("simple",prop)==0) writer->SetInput(this->simpl);
    else if(strcmp("geoDistSimple",prop)==0)
    {
        Simplify(500);
        GeoDistRingSimple(250,1000);
        this->simpl->GetPointData()->SetScalars(this->geoDistRingSimple);
        this->simpl->Update();
        writer->SetInput(this->simpl);
    }
    else if(strcmp("closed",prop)==0) writer->SetInput(this->closedMesh);
    else if(strcmp("medial",prop)==0) writer->SetInput(this->medialSurface);
    else writer->SetInput(this->mesh);
    writer->Update();
    writer->Write();
    writer->Delete();

}

void MeshAnalyser::WriteIntoFile(char* fileName)
{
    vtkPolyDataWriter* writer=vtkPolyDataWriter::New();
    writer->SetFileName(fileName);

    writer->SetInput(this->mesh);
    writer->Update();
    writer->Write();
    writer->Delete();
}


void MeshAnalyser::WriteIntoFile(char* fileName, vtkDataArray* propExt)
{
    vtkPolyDataWriter* writer=vtkPolyDataWriter::New();
    writer->SetFileName(fileName);

    this->mesh->GetPointData()->SetScalars(propExt);
    this->mesh->Update();

    writer->SetInput(this->mesh);
    writer->Update();
    writer->Write();
    writer->Delete();

}

void MeshAnalyser::ComputePointSurface()
{	
    //initialisation
    vtkCellArray* cells=this->mesh->GetPolys();
    int nbPolys = cells->GetNumberOfCells();
    this->totalSurface=0;
    this->pointSurf->Reset();

    for(int i=0;i<this->nbPoints;i++)
    {
        this->pointSurf->InsertNextValue(0);
    }

    int cellIds[3];

    double pos1[3];
    double pos2[3];
    double pos3[3];

    float a;
    float b;
    float c;

    float area;

    for (int i=0;i<nbPolys;i++)
    {
        cellIds[0]=this->mesh->GetCell(i)->GetPointId(0);
        cellIds[1]=this->mesh->GetCell(i)->GetPointId(1);
        cellIds[2]=this->mesh->GetCell(i)->GetPointId(2);

        if(cellIds[0]>this->nbPoints||cellIds[1]>this->nbPoints||cellIds[2]>this->nbPoints)
        {
            cout<<"point surface cell id problem: "<<i<<" "<<cellIds[0]<<" "<<cellIds[1]<<" "<<cellIds[2]<<endl;
        }
        else
        {
            this->mesh->GetPoint(cellIds[0],pos1);
            this->mesh->GetPoint(cellIds[1],pos2);
            this->mesh->GetPoint(cellIds[2],pos3);

            //distances between points of each triangle
            a=sqrt(vtkMath::Distance2BetweenPoints(pos1,pos2));
            b=sqrt(vtkMath::Distance2BetweenPoints(pos1,pos3));
            c=sqrt(vtkMath::Distance2BetweenPoints(pos2,pos3));

            //Herons's formula
            area=0.25*sqrt((a+b+c)*(b+c-a)*(a-b+c)*(a+b-c));

            this->totalSurface+=area;

            //add a third the face area to each point of the triangle
            this->pointSurf->SetValue(cellIds[0],this->pointSurf->GetValue(cellIds[0])+area/3);
            this->pointSurf->SetValue(cellIds[1],this->pointSurf->GetValue(cellIds[1])+area/3);
            this->pointSurf->SetValue(cellIds[2],this->pointSurf->GetValue(cellIds[2])+area/3);
        }
    }
}

void MeshAnalyser::ComputePointSurfaceSimple()
{
    //initialisation
    vtkCellArray* cells=this->simpl->GetPolys();
    int nbPolys = cells->GetNumberOfCells();
    this->totalSurfaceSimple=0;
    this->pointSurfSimple->Reset();

    for(int i=0;i<this->nbPointsSimple;i++)
    {
        this->pointSurfSimple->InsertNextValue(0);
    }

    int cellIds[3];

    double pos1[3];
    double pos2[3];
    double pos3[3];

    float a;
    float b;
    float c;

    float area;

    for (int i=0;i<nbPolys;i++)
    {
        cellIds[0]=this->simpl->GetCell(i)->GetPointId(0);
        cellIds[1]=this->simpl->GetCell(i)->GetPointId(1);
        cellIds[2]=this->simpl->GetCell(i)->GetPointId(2);

        this->simpl->GetPoint(cellIds[0],pos1);
        this->simpl->GetPoint(cellIds[1],pos2);
        this->simpl->GetPoint(cellIds[2],pos3);

        //distances between points of each triangle
        a=sqrt(vtkMath::Distance2BetweenPoints(pos1,pos2));
        b=sqrt(vtkMath::Distance2BetweenPoints(pos1,pos3));
        c=sqrt(vtkMath::Distance2BetweenPoints(pos2,pos3));

        //Herons's formula
        area=0.25*sqrt((a+b+c)*(b+c-a)*(a-b+c)*(a+b-c));

        this->totalSurfaceSimple+=area;

        //add a third the face area to each point of the triangle
        this->pointSurfSimple->SetValue(cellIds[0],this->pointSurfSimple->GetValue(cellIds[0])+area/3);
        this->pointSurfSimple->SetValue(cellIds[1],this->pointSurfSimple->GetValue(cellIds[1])+area/3);
        this->pointSurfSimple->SetValue(cellIds[2],this->pointSurfSimple->GetValue(cellIds[2])+area/3);
    }
}

void MeshAnalyser::ComputeTravelDepthFromClosed(bool norm)
{
    if(this->closedMesh->GetNumberOfPoints()<1)
    {
        ComputeClosedMesh();
    }

    ComputeTravelDepth(norm,this->closedMesh);
}


void MeshAnalyser::ComputeTravelDepth(bool norm)
{
    //convex hull construction
    int recPlan=3;

    vtkHull *hull = vtkHull::New();
    hull->SetInputConnection(this->mesh->GetProducerPort());
    hull->AddRecursiveSpherePlanes(recPlan);
    hull->Update();

    vtkPolyData *pq = vtkPolyData::New();
    pq->DeepCopy(hull->GetOutput());
    pq->Update();

    cout<<"Hull generated"<<endl;

    ComputeTravelDepth(norm, pq);
}

void MeshAnalyser::ComputeTravelDepth(bool norm, vtkPolyData* pq)
{
    int maxBound=5000;

    //Point locator for the closest point of the convex hull
    vtkCellLocator *pl = vtkCellLocator::New();
    pl->SetDataSet(pq);
    pl->BuildLocator();

    vtkDoubleArray* depths=vtkDoubleArray::New();

    vtkIntArray* label=vtkIntArray::New();		//certitude
    vtkIntArray* labelT=vtkIntArray::New();		//temporary certitude

    for (int i=0;i<this->nbPoints;i++)
    {
        label->InsertNextValue(-1);
        labelT->InsertNextValue(-1);
    }

    vtkIdList* frontier=vtkIdList::New();		//points with a certitude<maximal certitude

    double point1[3], point2[3];
    int  subid;
    vtkIdType cellid;

    double ec=0;

    //-----------------------------------------------//
    //allocation of the distance to the              //
    //mesh if the point is visible                   //
    //-----------------------------------------------//

    double threshold=0.30;

    double vector[3];

    vtkCellLocator* cellLocator=vtkCellLocator::New();
    cellLocator->SetDataSet(this->mesh);
    cellLocator->BuildLocator();

    double t, ptline[3], pcoords[3];
    int subId;

    int isInter;

    vtkPoints* nHPoints=vtkPoints::New();		//new reference points
    vtkIdList* nHList=vtkIdList::New();

    int doneLabel=7;				//maximal certitude
    int goodLabel=6;
    double tempSurf=0;
    for(int i = 0; i < this->nbPoints; i++)
    {
        this->mesh->GetPoint(i,point1);
        pl->FindClosestPoint(point1,point2,cellid,subid, ec);
        ec=sqrt(ec);

        //If the point is very close
        if (ec<threshold)
        {
            depths->InsertNextValue(ec);
            label->SetValue(i,doneLabel);
            nHPoints->InsertNextPoint(point1);
            nHList->InsertNextId(i);
            tempSurf+=this->pointSurf->GetValue(i);
        }
        else
        {
            //small shift, because the intersection is the starting point otherwise
            vector[0]=point2[0]-point1[0];
            vector[1]=point2[1]-point1[1];
            vector[2]=point2[2]-point1[2];

            point1[0]+=vector[0]*0.001;
            point1[1]+=vector[1]*0.001;
            point1[2]+=vector[2]*0.001;

            point2[0]+=vector[0]*0.001;
            point2[1]+=vector[1]*0.001;
            point2[2]+=vector[2]*0.001;

            t=1;

            isInter=cellLocator->IntersectWithLine(point1, point2, 0.0001, t, ptline, pcoords, subId);

            //if there is no other point of the surface between the point and the convex hull
            if(isInter==0)
            {
                depths->InsertNextValue(ec);
                label->SetValue(i,doneLabel);
                nHPoints->InsertNextPoint(point1);
                nHList->InsertNextId(i);
                tempSurf+=this->pointSurf->GetValue(i);
            }
            else
            {
                frontier->InsertUniqueId(i);
                depths->InsertNextValue(maxBound);
            }
        }
    }

    pl->Delete();

    vtkPolyData* nHPointSet=vtkPolyData::New();
    nHPointSet->SetPoints(nHPoints);

    int nbNH=nHPoints->GetNumberOfPoints();

    int nbFrontierPoints=frontier->GetNumberOfIds();
    vtkIdList* curNeib=vtkIdList::New();
    int nbNeib;
    vtkIdType curId;
    vtkIdType curFrontierId;

    vtkIdList* oldFront=vtkIdList::New();

    int nbOld;

    double minNeibDist;

    double curDist;

    double n1[3],n2[3];

    vtkIdList* nHListr=vtkIdList::New();
    vtkIdList *result=vtkIdList::New();

    cout<<"Euclidean depth allocated for visible points"<<endl;


    //-----------------------------------------------//
    //allocation of the distance to the              //
    //mesh if the point is  not visible              //
    //-----------------------------------------------//

    int nbLeftPoints;

    vtkIdList* frontierT=vtkIdList::New();

    int curLab;
    int iteration=0;
    int maxIt = 7;

    int subIteration=0;
    int maxSubIt = 7;

    //while there are still uncertain points and while the reference set is not empty
    while(nbFrontierPoints>0 && nbNH>1 && iteration<maxIt)
    {
        iteration++;

        frontierT->DeepCopy(frontier);
        labelT->DeepCopy(label);

        nbLeftPoints=nbFrontierPoints;

        subIteration = 0;

        cout<<"Geodesic propagation "<<iteration<<endl;

        while(nbLeftPoints>0 && subIteration<maxSubIt)
        {
            subIteration++;
            for(int i=0;i<nbLeftPoints;i++)
            {
                curFrontierId=frontierT->GetId(i);
                this->mesh->GetPoint(curFrontierId,point2);

                curNeib->Reset();
                GetPointNeighbors(curFrontierId,curNeib);
                nbNeib=curNeib->GetNumberOfIds();
                minNeibDist=depths->GetValue(curFrontierId);
                curLab=-1;
                //geodesic propagation
                for(int j=0;j<nbNeib;j++)
                {
                    curId=curNeib->GetId(j);

                    if (depths->GetValue(curId)<maxBound)
                    {
                        this->mesh->GetPoint(curId,point1);
                        ec=vtkMath::Distance2BetweenPoints(point1,point2);

                        ec=sqrt(ec);

                        curDist=depths->GetValue(curId)+ec;

                        if(curDist<minNeibDist && label->GetValue(curId)-1>0)
                        {
                            minNeibDist=curDist;
                            curLab=label->GetValue(curId)-1;
                        }

                    }

                }
                if (curLab<0)
                {
                    oldFront->InsertUniqueId(curFrontierId);
                    labelT->SetValue(curFrontierId,labelT->GetValue(curFrontierId)+1);
                    if(labelT->GetValue(curFrontierId)>goodLabel)oldFront->InsertUniqueId(curFrontierId);
                }
                if(minNeibDist<maxBound)
                {
                    depths->SetValue(curFrontierId,minNeibDist);
                    label->SetValue(curFrontierId,curLab);
                    labelT->SetValue(curFrontierId,curLab);
                    oldFront->InsertUniqueId(curFrontierId);
                }

            }

            nbOld= oldFront->GetNumberOfIds();

            for(int i=0;i<nbOld;i++)
            {
                frontierT->DeleteId(oldFront->GetId(i));
            }

            oldFront->Reset();
            labelT->Reset();
            nbLeftPoints=frontierT->GetNumberOfIds();
        }


        vtkPointLocator* tcl=vtkPointLocator::New();
        tcl->SetDataSet(nHPointSet);
        tcl->BuildLocator();

        vtkPoints* tnh=vtkPoints::New();

        nHListr->Reset();

        tempSurf=0;
        //euclidean propagation

        cout<<"Euclidean propagation "<<iteration<<endl;

        for(int i=0;i<nbFrontierPoints;i++)
        {
            curFrontierId=frontier->GetId(i);
            this->mesh->GetPoint(curFrontierId,point1);

            tcl->FindClosestNPoints (2, point1,result);

            if(nHList->GetId(result->GetId(0))!=curFrontierId)
            {
                curId=nHList->GetId(result->GetId(0));
            }
            else curId=nHList->GetId(result->GetId(1));

            this->mesh->GetPoint(curId,point2);

            ec=vtkMath::Distance2BetweenPoints(point1,point2);
            ec=sqrt(ec);

            this->normals->GetTuple(curFrontierId,n1);
            this->normals->GetTuple(curId,n2);

            point1[0]+=n1[0]*0.1*ec;
            point1[1]+=n1[1]*0.1*ec;
            point1[2]+=n1[2]*0.1*ec;

            point2[0]+=n2[0]*0.1*ec;
            point2[1]+=n2[1]*0.1*ec;
            point2[2]+=n2[2]*0.1*ec;

            isInter=cellLocator->IntersectWithLine(point1, point2, 0.0001, t, ptline, pcoords, subId);

            if(isInter==0)
            {

                if(ec+depths->GetValue(curId)<depths->GetValue(curFrontierId))
                {
                    label->SetValue(curFrontierId,label->GetValue(curId)-1);
                    depths->SetValue(curFrontierId,ec+depths->GetValue(curId));
                }
                else if(label->GetValue(curFrontierId)>0)\
                    label->SetValue(curFrontierId,label->GetValue(curFrontierId)+1);
                if(label->GetValue(curFrontierId)>goodLabel)
                {
                    tnh->InsertNextPoint(point1);
                    nHListr->InsertNextId(curFrontierId);
                    tempSurf+=this->pointSurf->GetValue(curFrontierId);
                }
            }
            else
            {
                if(label->GetValue(curFrontierId)>0)label->SetValue(curFrontierId,label->GetValue(curFrontierId)+1);
                if(label->GetValue(curFrontierId)>goodLabel)
                {
                    tnh->InsertNextPoint(point1);
                    nHListr->InsertNextId(curFrontierId);
                    tempSurf+=this->pointSurf->GetValue(curFrontierId);
                }
            }
        }

        tcl->Delete();
        nHPointSet->Reset();
        nHPointSet->SetPoints(tnh);
        nbFrontierPoints=frontier->GetNumberOfIds();
        nbNH=tnh->GetNumberOfPoints();
        nHList->Reset();
        nHList->DeepCopy(nHListr);
        tnh->Delete();
    }

    labelT->Delete();

    frontier->Reset();

    cout<<"Travel depth main computation done"<<endl;


    ////-----------------------------------------------//
    ////last propagation					             //
    ////-----------------------------------------------//

    //detection of uncertain points
    for(int i=0;i<this->nbPoints;i++)
    {
        if(label->GetValue(i)<doneLabel)
        {
            frontier->InsertNextId(i);
            label->SetValue(i,-1);
        }
    }

    nbLeftPoints=frontier->GetNumberOfIds();

    iteration = 0;

    //starting from uncertain points and checking their neighbors
    while(nbLeftPoints>0 && iteration<maxIt)
    {
        iteration ++;

        for(int i=0;i<nbLeftPoints;i++)
        {
            curFrontierId=frontier->GetId(i);
            this->mesh->GetPoint(curFrontierId,point2);

            curNeib->Reset();
            GetPointNeighbors(curFrontierId,curNeib);
            nbNeib=curNeib->GetNumberOfIds();
            minNeibDist=depths->GetValue(curFrontierId);

            for(int j=0;j<nbNeib;j++)
            {
                curId=curNeib->GetId(j);

                if (depths->GetValue(curId)<maxBound)
                {
                    this->mesh->GetPoint(curId,point1);
                    ec=vtkMath::Distance2BetweenPoints(point1,point2);

                    ec=sqrt(ec);

                    curDist=depths->GetValue(curId)+ec;

                    if(curDist<minNeibDist)
                    {
                        minNeibDist=curDist;
                    }
                }

            }

            if(minNeibDist<maxBound)
            {
                if(minNeibDist==depths->GetValue(curFrontierId))
                {
                    label->SetValue(curFrontierId,label->GetValue(curFrontierId)+1);
                }
                else
                {
                    depths->SetValue(curFrontierId,minNeibDist);
                    label->SetValue(curFrontierId,label->GetValue(curId)-1);
                }
            }

            if(label->GetValue(curFrontierId)>goodLabel)
            {
                oldFront->InsertUniqueId(curFrontierId);
            }


        }

        nbOld= oldFront->GetNumberOfIds();

        for(int i=0;i<nbOld;i++)
        {
            frontier->DeleteId(oldFront->GetId(i));
        }

        oldFront->Reset();

        nbLeftPoints=frontier->GetNumberOfIds();
    }

    label->Delete();
    frontier->Delete();

    cout<<"last propagation done"<<endl;


    ////-----------------------------------------------//
    ////normalization       		             //
    ////-----------------------------------------------//

    double MIN=1000;
    double MAX=-1;

    if(norm==true)
    {
        for(int i = 0; i < this->nbPoints; i++)
        {
            if(MAX<depths->GetValue(i)&depths->GetValue(i)<maxBound)MAX=depths->GetValue(i);
            if(MIN>depths->GetValue(i))MIN=depths->GetValue(i);
        }
    }

    double cc;
    double tot=0;
    for(int i = 0; i < this->nbPoints; i++)
    {
        if(norm==true)cc = (depths->GetValue(i)-MIN)/(MAX-MIN);
        else cc=depths->GetValue(i);
        this->depth->InsertNextValue(cc);
        tot+=cc*this->pointSurf->GetValue(i);
    }

    depths->Delete();

    cout<<"Travel depth computed"<<endl;


}

void MeshAnalyser::Simplify(double factor)
{
    //if the last simplification was not exactly the same
    if(this->approxFactor!=factor)
    {
        this->nbPointsSimple=10000000;
        this->simpl->DeepCopy(this->mesh);

        if(factor<=1)
        {
            vtkQuadricDecimation* dec=vtkQuadricDecimation::New();

            dec->SetInput(this->simpl);
            dec->SetTargetReduction(factor);
            dec->Update();

            this->simpl=dec->GetOutput();
            this->nbPointsSimple=this->simpl->GetNumberOfPoints();
        }

        else
        {
            //if the decimation factor is a number of points,
            //the decimation is done until this number of points is reached
            while(this->nbPointsSimple>factor)
            {
                vtkQuadricDecimation* dec=vtkQuadricDecimation::New();
                dec->SetInput(this->simpl);
                dec->SetTargetReduction(0.5);
                dec->Update();

                this->simpl=dec->GetOutput();
                if(this->simpl->GetNumberOfPoints()==this->nbPointsSimple)break;
                this->nbPointsSimple=this->simpl->GetNumberOfPoints();
                cout<<this->nbPointsSimple<<" ";
            }
        }
        this->approxFactor=factor;
    }
}

void MeshAnalyser::ComputeEuclideanDepth(bool norm)
{
    //convex hull construction
    int recPlan=3;

    vtkHull *hull = vtkHull::New();
    hull->SetInputConnection(this->mesh->GetProducerPort());
    hull->AddRecursiveSpherePlanes(recPlan);
    hull->Update();

    vtkPolyData *pq = vtkPolyData::New();
    pq->DeepCopy(hull->GetOutput());
    pq->Update();

    cout<<"Hull generated"<<endl;

    ComputeEuclideanDepth(norm, pq);
}

void MeshAnalyser::ComputeEuclideanDepth(bool norm, vtkPolyData *refMesh)
{

    double maxBound = 5000;

    //Point locator for the closest point of the convex hull
    vtkCellLocator *pl = vtkCellLocator::New();
    pl->SetDataSet(refMesh);
    pl->BuildLocator();

    double point1[3], point2[3];
    int  subid;
    vtkIdType cellid;

    double ec=0;

    vtkDoubleArray* depths = vtkDoubleArray::New();

    //-----------------------------------------------//
    //allocation of the distance to the              //
    //mesh if the point is visible                   //
    //-----------------------------------------------//


    vtkCellLocator* cellLocator=vtkCellLocator::New();
    cellLocator->SetDataSet(this->mesh);
    cellLocator->BuildLocator();

    this->euclideanDepth->Reset();

    for(int i = 0; i < this->nbPoints; i++)
    {
        this->mesh->GetPoint(i,point1);
        pl->FindClosestPoint(point1,point2,cellid,subid, ec);
        ec=sqrt(ec);

        depths->InsertNextValue(ec);

    }

    double MIN=1000;
    double MAX=-1;

    if(norm==true)
    {
        for(int i = 0; i < this->nbPoints; i++)
        {
            if(MAX<depths->GetValue(i)&depths->GetValue(i)<maxBound)MAX=depths->GetValue(i);
            if(MIN>depths->GetValue(i))MIN=depths->GetValue(i);
        }
    }

    double cc;
    for(int i = 0; i < this->nbPoints; i++)
    {
        if(norm==true)cc = (depths->GetValue(i)-MIN)/(MAX-MIN);
        else cc=depths->GetValue(i);
        this->euclideanDepth->InsertNextValue(cc);
    }

    depths->Delete();

    pl->Delete();

}

void MeshAnalyser::ComputeEuclideanDepthFromClosed(bool norm)
{
    if(this->closedMesh->GetNumberOfPoints()<1)
    {
        ComputeClosedMesh();
    }

    ComputeEuclideanDepth(norm,this->closedMesh);
}


void MeshAnalyser::ComputeNormals()
{
    vtkPolyDataNormals *pdn = vtkPolyDataNormals::New();
    pdn->SetInput(this->mesh);
    pdn->SetFeatureAngle(90);
    pdn->SplittingOff();
    pdn->Update();

    vtkPolyData* no=pdn->GetOutput();
    no->Update();
    this->normals=no->GetPointData()->GetNormals();
}

void MeshAnalyser::ComputeBothCurvatures(double ray)
{

    //initialisation
    vtkPolyData* upNorm=vtkPolyData::New();
    vtkPoints* upNormPoints=vtkPoints::New();

    double point1[3], point2[3], norm[3];
    double eps=0.01;

    //computation of the mesh modification by projection of the points
    //in the direction of the normal.
    for(int i=0;i<this->nbPoints;i++)
    {
        this->mesh->GetPoint(i,point1);
        this->normals->GetTuple(i,norm);

        point2[0]=point1[0]+norm[0]*eps;
        point2[1]=point1[1]+norm[1]*eps;
        point2[2]=point1[2]+norm[2]*eps;

        upNormPoints->InsertNextPoint(point2);
    }

    upNorm->DeepCopy(this->mesh);
    upNorm->SetPoints(upNormPoints);
    upNorm->Update();

    upNormPoints->Delete();

    //computation of the laplacian smoothed mesh
    vtkSmoothPolyDataFilter* smoothed = vtkSmoothPolyDataFilter::New();
    smoothed->SetInput(this->mesh);
    smoothed->SetRelaxationFactor(0.7);
    smoothed->SetNumberOfIterations(200);
    smoothed->FeatureEdgeSmoothingOff();
    smoothed->Update();

    int nbInRing;

    //using meshanalyser to compute the areas affected to each point
    MeshAnalyser* mat=new MeshAnalyser(upNorm);

    vtkDoubleArray* upPs=mat->GetPointSurface();

    upNorm->Delete();

    MeshAnalyser* mas=new MeshAnalyser(smoothed->GetOutput());

    vtkDoubleArray* sPs=mas->GetPointSurface();

    smoothed->Delete();

    double surf,normSurf, siSurf;

    //point locator for the smoothing of the curvature field
    vtkPointLocator* pl=vtkPointLocator::New();
    pl->SetDataSet(this->mesh);
    pl->BuildLocator();
    vtkIdList* Nclo = vtkIdList::New();

    vtkIdType curId;

    double maxCurv=-10;
    double minCurv=1000;
    double maxgCurv=-10;
    double mingCurv=1000;

    for(int i=0;i<this->nbPoints;i++)
    {
        this->mesh->GetPoint(i,point1);
        pl->FindPointsWithinRadius(ray,point1,Nclo);
        //GeoDistRing(i,ray);
        //nbInRing=this->inRing->GetNumberOfIds();
        nbInRing=Nclo->GetNumberOfIds();

        surf=0;
        normSurf=0;
        siSurf=0;

        for(int j=0;j<nbInRing;j++)
        {
            //curId=this->inRing->GetId(j);
            curId=Nclo->GetId(j);

            surf+=this->pointSurf->GetValue(curId);
            normSurf+=upPs->GetValue(curId);
            siSurf+=sPs->GetValue(curId);
        }
        if(normSurf/surf<minCurv)minCurv=normSurf/surf;
        if(normSurf/surf>maxCurv)maxCurv=normSurf/surf;
        if(siSurf/surf<mingCurv)mingCurv=siSurf/surf;
        if(siSurf/surf>maxgCurv)maxgCurv=siSurf/surf;

        this->curv->InsertNextValue(normSurf/surf);
        this->gCurv->InsertNextValue(siSurf/surf);

    }

    delete mat;
    delete mas;

    double curCurv;

    ofstream myfile("curvNorm.txt");
    myfile.clear();

    for(int i=0;i<this->nbPoints;i++)
    {
        curCurv=this->curv->GetValue(i);
        this->curv->SetValue(i,(maxCurv-curCurv)/(maxCurv-minCurv)*(-2)+1);
        myfile<<(maxCurv-curCurv)/(maxCurv-minCurv)*(-2)+1<<endl;
        curCurv=this->gCurv->GetValue(i);
        this->gCurv->SetValue(i,(maxgCurv-curCurv)/(maxgCurv-mingCurv)*2-1);
    }
    myfile.close();

}

void MeshAnalyser::ComputeCurvature(double res)
{

    double pt1[3],pt2[3],N[3]={0,0,0}, normv;

    vtkSmoothPolyDataFilter* smoothed = vtkSmoothPolyDataFilter::New();
    smoothed->SetInput(this->mesh);
    smoothed->SetRelaxationFactor(res);
    smoothed->SetNumberOfIterations(20);
    smoothed->FeatureEdgeSmoothingOff();
    smoothed->BoundarySmoothingOff();
    smoothed->Update();

    double maxCurv=-10;
    double minCurv=1000;	this->curv->Reset();

    double pt1pt2[3];

    double curCurv;

    for( int i = 0; i < this->nbPoints; i++)
    {
        this->mesh->GetPoint(i,pt1);
        this->normals->GetTuple(i,N);
        smoothed->GetOutput()->GetPoint(i,pt2);
        for(int w=0; w<3;w++)
        {
            pt1pt2[w]=pt2[w]-pt1[w];
        }
        //if(vtkMath::Norm(pt1pt2)==0)normv=1;
        //else normv=vtkMath::Norm(pt1pt2);
        normv=vtkMath::Norm(pt1pt2);
        curCurv=(1+vtkMath::Dot(N,pt1pt2))/2.0;
        //curCurv=normv;
        this->curv->InsertNextTuple1(curCurv);
        if(curCurv<minCurv)minCurv=curCurv;
        if(curCurv>maxCurv)maxCurv=curCurv;
        if(isnan(this->curv->GetTuple1(i)))cout<<vtkMath::Norm(pt1pt2)<<" "<<vtkMath::Norm(N)<<endl;
    }


    ofstream myfile("curvLapl.txt");
    myfile.clear();

    for(int i=0;i<this->nbPoints;i++)
    {
        curCurv=this->curv->GetValue(i);
        this->curv->SetValue(i,(maxCurv-curCurv)/(maxCurv-minCurv)*(-2)+1);
        myfile<<(maxCurv-curCurv)/(maxCurv-minCurv)*(-2)+1<<endl;
    }

    myfile.close();


    smoothed->Delete();

}


void MeshAnalyser::ComputeVoronoi(vtkIdList* centroidsList, double maxDist)
{
    //initialisation
    int nbCentroids	=centroidsList->GetNumberOfIds();
    vtkDoubleArray* shortestDists=vtkDoubleArray::New();

    int nbApprox=500;

    for(int i=0; i<this->nbPoints;i++)
    {
        shortestDists->InsertNextValue(maxDist);
        this->voronoiBin->InsertNextId(-1);
    }

    vtkIdType curCentroid;

    double pointc[3], point[3];
    double ec;
    //compute the geodesic distance from centroids to all points
    for(int i=0;i<nbCentroids;i++)
    {
        curCentroid=centroidsList->GetId(i);
        //~ GeoDistRing(curCentroid,maxDist);
        this->mesh->GetPoint(curCentroid,pointc);
        for(int j=0;j<this->nbPoints;j++)
        {
            this->mesh->GetPoint(j,point);

            ec=vtkMath::Distance2BetweenPoints(pointc,point);
            ec=sqrt(ec);

            //~ if(shortestDists->GetValue(j)>this->geoDistRing->GetValue(j))
            if(shortestDists->GetValue(j)>ec)
            {
                //~ shortestDists->SetValue(j,this->geoDistRing->GetValue(j));
                shortestDists->SetValue(j,ec);
                this->voronoiBin->SetId(j,curCentroid);
            }
        }
    }
    shortestDists->Delete();
    cout<<"Voronoi computed"<<endl;
}

void MeshAnalyser::ComputeHistogram(char* prop, int nbBins)
{
    vtkDoubleArray* data=vtkDoubleArray::New();

    if(strcmp("geoDist",prop)==0) data=this->geoDistRing;
    else if(strcmp("depth",prop)==0) data=this->depth;
    else if(strcmp("curv",prop)==0) data=this->curv;
    else if(strcmp("gCurv",prop)==0) data=this->gCurv;
    else if(strcmp("test",prop)==0) data=this->test;
    else if(strcmp("surf",prop)==0) data=this->pointSurf;

    double min=10000000;
    double max=-100000000;

    for(int i=0;i<this->nbPoints;i++)
    {
        if(min>data->GetValue(i))min=data->GetValue(i);
        if(max<data->GetValue(i))max=data->GetValue(i);
    }

    double hist[nbBins];

    for(int j=0;j<nbBins;j++)
    {
        hist[j]=0;
    }

    int curBin;
    double s=(max-min)/(double)nbBins;


    for(int i=0;i<this->nbPoints;i++)
    {
        curBin=(int)floor((data->GetValue(i)-min)/s);
        hist[curBin]=hist[curBin]+this->pointSurf->GetValue(i);
    }
    cout<<"histogram of "<<prop<<endl;
    cout<<"min: "<<min<<"; max: "<<max<<"; step: "<<s<<endl;

    for(int j=0;j<nbBins;j++)
    {
        cout<<hist[j]<<" ";
    }
    cout<<endl;

}

void MeshAnalyser::ComputeClosedMesh()
{


    vtkSmartPointer<vtkImageData> whiteImage = vtkSmartPointer<vtkImageData>::New();
    double bounds[6];
    this->mesh->GetBounds(bounds);
    double spacing[3]; // desired volume spacing
    spacing[0] = 2;
    spacing[1] = 2;
    spacing[2] = 2;
    whiteImage->SetSpacing(spacing);
    double sk = 5;
    double sec = 1.5;

    for(int i = 0; i < 3 ; i++)
    {
        bounds[2*i] -= sec*sk;
        bounds[2*i+1] += sec*sk;
    }


    // compute dimensions
    int dim[3];
    for (int i = 0; i < 3; i++)
    {
        dim[i] = static_cast<int>(ceil((bounds[i * 2 + 1] - bounds[i * 2]) / spacing[i]));
    }
    whiteImage->SetDimensions(dim);
    whiteImage->SetExtent(0, dim[0] - 1, 0, dim[1] - 1, 0, dim[2] - 1);

    double origin[3];
    // NOTE: I am not sure whether or not we had to add some offset!
    origin[0] = bounds[0];// + spacing[0] / 2;
    origin[1] = bounds[2];// + spacing[1] / 2;
    origin[2] = bounds[4];// + spacing[2] / 2;
    whiteImage->SetOrigin(origin);

    whiteImage->SetScalarTypeToUnsignedChar();
    whiteImage->AllocateScalars();

    // fill the image with foreground voxels:
    unsigned char inval = 255;
    unsigned char outval = 0;
    vtkIdType count = whiteImage->GetNumberOfPoints();
    for (vtkIdType i = 0; i < count; ++i)
    {
        whiteImage->GetPointData()->GetScalars()->SetTuple1(i, inval);
    }

    // polygonal data --> image stencil:
    vtkSmartPointer<vtkPolyDataToImageStencil> pol2stenc = vtkSmartPointer<vtkPolyDataToImageStencil>::New();
    pol2stenc->SetInput(this->mesh);
    pol2stenc->SetOutputOrigin(origin);
    pol2stenc->SetOutputSpacing(spacing);
    pol2stenc->SetOutputWholeExtent(whiteImage->GetExtent());
    pol2stenc->Update();

    // cut the corresponding white image and set the background:
    vtkSmartPointer<vtkImageStencil> imgstenc = vtkSmartPointer<vtkImageStencil>::New();
    imgstenc->SetInput(whiteImage);
    imgstenc->SetStencil(pol2stenc->GetOutput());
    imgstenc->ReverseStencilOff();
    imgstenc->SetBackgroundValue(outval);
    imgstenc->Update();

    //Dilatation
    vtkImageContinuousDilate3D *dilate = vtkImageContinuousDilate3D::New();
    dilate->SetInputConnection(imgstenc->GetOutputPort());
    //sk is the size of the dilation kernel in each direction


    dilate->SetKernelSize(sk,sk,sk);
    dilate->UpdateWholeExtent();


    //Erosion
    vtkImageContinuousErode3D *erode = vtkImageContinuousErode3D::New();
    erode->SetInputConnection(dilate->GetOutputPort());
    erode->SetKernelSize(sk,sk,sk);
    erode->UpdateWholeExtent();

    vtkMarchingContourFilter* imc3=vtkMarchingContourFilter::New();
    imc3->SetInputConnection(erode->GetOutputPort());
    imc3->SetValue(0,100);
    imc3->UpdateWholeExtent();

    this->closedMesh->DeepCopy(imc3->GetOutput());

    cout<<"Closed mesh computed"<<endl;

}

void MeshAnalyser::ComputeMedialSurfaces()
{
    //    Simplify(40000);

    //    cout<<"medial: mesh simplified"<<endl;

    //    vtkPolyDataNormals *pdn = vtkPolyDataNormals::New();
    //    pdn->SetInput(this->simpl);
    //    pdn->SetFeatureAngle(90);
    //    pdn->Update();

    //    vtkPolyData* no=pdn->GetOutput();
    //    no->Update();
    //    vtkDataArray* normal = no->GetPointData()->GetNormals();

    //    cout<<"medial: normals computed"<<endl;

    //    vtkPoints* medPoints = vtkPoints::New();


    //    vtkPolyData* evMesh = vtkPolyData::New();
    //    evMesh->DeepCopy(this->simpl);
    //    evMesh->Update();

    //    vtkPoints* evPoints = evMesh->GetPoints();

    //    vtkIdList* toProcess = vtkIdList::New();

    //    double evFactor = 0.001;
    //    double eps = 0.0001;
    //    int maxNbSteps = 100;


    //    int nbToProcess = this->simpl->GetNumberOfPoints();

    //    for (int i = 0 ; i < nbToProcess ; i++ )
    //    {
    //        toProcess->InsertNextId(i);
    //    }


    //    double curPoint[3], disPoint[3], midPoint[3];
    //    double nor[3];
    //    vtkIdType curId;



    ////    cout<<"evMesh"<<endl;
    ////    vtkIndent indent;
    ////    evMesh->PrintSelf(cout, indent);
    ////    cout<<endl;

    //    vtkCellLocator* cl = vtkCellLocator::New();
    //    cl->SetDataSet(evMesh);
    //    cl->BuildLocator();
    //    cl->Update();

    //    double t, ptline[3], pcoords[3];
    //    int subId;
    //    int isInter;


    //    cout<<"medial: loop about to start"<<endl;

    //    for(int nbs = 0 ; nbs < maxNbSteps ; nbs ++)
    //    {
    //        cout<<"medial: step "<<nbs<<endl;

    //        cl->SetDataSet(evMesh);
    //        cl->BuildLocator();
    //        cl->Update();

    //        nbToProcess = toProcess->GetNumberOfIds();

    //        for (int i = 0 ; i < nbToProcess ; i++ )
    //        {
    //            curId = toProcess->GetId(i);
    //            evPoints->GetPoint(curId,curPoint);
    //            normal->GetTuple(curId,nor);

    //            for(int k = 0; k < 3 ; k++)
    //            {
    //                curPoint[k] += eps;
    //                disPoint[k] = curPoint[k] + evFactor*nor[k] - eps;
    //            }

    //            isInter = cl->IntersectWithLine(curPoint, disPoint, 0.0001, t, ptline, pcoords, subId);
    //            if(isInter != 0)
    //            {
    //                for(int k = 0; k < 3; k++)
    //                {
    //                    midPoint[k] = curPoint[k] - eps + t*disPoint[k];
    //                }
    //                medPoints->InsertNextPoint(midPoint);
    //                toProcess->DeleteId(curId);
    //            }

    //            evPoints->SetPoint(curId, disPoint[0], disPoint[1], disPoint[2]);
    //        }

    //        evMesh->SetPoints(evPoints);
    //        evMesh->Update();
    //    }

    //    vtkPolyData* midPD = vtkPolyData::New();
    //    midPD->SetPoints(medPoints);
    //    midPD->Update();

    ////    cout<<"midPD"<<endl;
    ////    midPD->PrintSelf(cout, indent);
    ////    cout<<endl;

    //    cout<<"medial: mesh generated"<<endl;

    //    vtkDelaunay3D* delFin = vtkDelaunay3D::New();
    //    delFin->SetInput(midPD);
    //    delFin->SetAlpha(4);
    //    delFin->Update();

    //    cout<<"medial: delaunay computed"<<endl;


    //    this->medialSurface->SetPoints(delFin->GetOutput()->GetPoints());
    //    this->medialSurface->SetPolys(delFin->GetOutput()->GetCells());


    //    if(this->euclideanDepth->GetSize()< 1)
    //    {
    //        ComputeEuclideanDepthFromClosed(true);
    //    }
    //    cout<<"ok"<<endl;

    //    vtkPoints* deepPoints = vtkPoints::New();

    //    double threshold = 0.2;
    double thr = -0.75;

    //    vtkIdList* corrMeshId = vtkIdList::New();

    //    for(int i = 0 ; i < this->nbPoints ; i++)
    //    {
    //        if(this->euclideanDepth->GetValue(i) > threshold)
    //        {
    //            deepPoints->InsertNextPoint(this->mesh->GetPoint(i));
    //            corrMeshId->InsertNextId(i);
    //        }
    //    }
    //    cout<<"ok"<<endl;

    //    int nbDeep = deepPoints->GetNumberOfPoints();

    //    double norm [3];
    //    double pos [3];

    ComputeNormals();

    //    for(int i = 0; i<nbDeep ; i++)
    //    {

    //        this->normals->GetTuple(corrMeshId->GetId(i),norm);
    //        deepPoints->GetPoint(i,pos);

    //        pos[0]+=10*norm[0];
    //        pos[1]+=10*norm[1];
    //        pos[2]+=10*norm[2];

    //        deepPoints->SetPoint(i,pos);

    //    }

    //    vtkPolyData* deepPD = vtkPolyData::New();
    //    deepPD->SetPoints(deepPoints);
    //    deepPD->Update();
    //    cout<<"ok"<<endl;


    //    vtkUnstructuredGrid* deepPD = vtkUnstructuredGrid::New();
    //    deepPD->SetPoints(deepPoints);
    //    deepPD->Update();
    //    cout<<"ok"<<endl;


    //    vtkDelaunay3D* del = vtkDelaunay3D::New();
    //    del->SetInput(this->mesh);
    //    del->SetAlpha(2);
    //    del->Update();


    //    vtkExtractEdges* ee = vtkExtractEdges::New();
    //    ee->SetInput(del->GetOutput());
    //    ee->Update();

    //    int nbLines = ee->GetOutput()->GetNumberOfLines();

    vtkIdType npts;
    vtkIdType* pts;
    vtkIdType c1, c2;

    double point1[3];
    double point2[3];
    double vec[3];
    double meanPoint[3];
    double res;
    double n1[3], n2[3];
    double ec;

    vtkPoints* midPoints = vtkPoints::New();

    vtkIdList *cellids = vtkIdList::New();

    vtkIdList* neighbors = vtkIdList::New();

    int nbn;

    vtkPointLocator* pl=vtkPointLocator::New();
    pl->SetDataSet(this->mesh);
    pl->BuildLocator();
    //    vtkIdList* Nclo = vtkIdList::New();


    for(int i = 0 ; i<this->nbPoints ; i++)
    {
        c1 = i;
        this->mesh->GetPoint(c1, point1);
        this->normals->GetTuple(c1,n1);

        pl->FindPointsWithinRadius(5,point1,neighbors);
        //        ee->GetOutput()->GetPointCells(i,cellids);
        //        for (int i = 0; i < cellids->GetNumberOfIds(); i++)
        //        {
        //                for(int j = 0; j < ee->GetOutput()->GetCell(cellids->GetId(i))->GetNumberOfPoints(); j++)
        //                {
        //                        if( ee->GetOutput()->GetCell(cellids->GetId(i))->GetPointId(j) < this->nbPoints )
        //                        {
        //                                neighbors->InsertUniqueId(ee->GetOutput()->GetCell(cellids->GetId(i))->GetPointId(j));
        //                        }
        //                }
        //        }

        nbn = neighbors->GetNumberOfIds();


        for(int j = 0; j < nbn ; j++)
        {

            c2 = neighbors->GetId(j);

            this->mesh->GetPoint(c2, point2);
            this->normals->GetTuple(c2,n2);

            res = vtkMath::Dot(n1,n2);

            if(res<thr)
            {
                for(int k = 0; k < 3 ; k++)
                {
                    meanPoint[k] = (point2[k] + point1[k])/2;
                }
                midPoints->InsertNextPoint(meanPoint);
            }
        }
    }

    cellids->Delete();


    //    for(int i = 0; i < nbLines ; i++)
    //    {
    //        ee->GetOutput()->GetLines()->GetCell(i, npts, pts);

    //        if(npts !=2)
    //        {
    //            cout<<"error, wrong usage of a vtk function"<<endl;
    ////            return;
    //        }

    //        c1 = corrMeshId->GetId(pts[0]);
    //        c2 = corrMeshId->GetId(pts[1]);

    //        this->mesh->GetPoint(c1, point1);
    //        this->mesh->GetPoint(c2, point2);

    ////        IsIntersecting(point1,point2);
    //        for(int k = 0; k < 3 ; k++)
    //        {
    //            vec[k] = point2[k] - point1[k];
    //            meanPoint[k] = (point2[k] + point1[k])/2;
    //        }

    //        this->normals->GetTuple(c1,n);

    //        ec=vtkMath::Distance2BetweenPoints(point1,point2);
    //        ec=sqrt(ec);

    //        res = vtkMath::Dot(vec,n)/ec;

    //        if(fabs(res)>thr)
    //        {
    //            midPoints->InsertNextPoint(meanPoint);
    //        }


    //    }

    vtkPolyData* midPD = vtkPolyData::New();
    midPD->SetPoints(midPoints);
    midPD->Update();

    vtkDelaunay3D* delFin = vtkDelaunay3D::New();
    delFin->SetInput(midPD);
    delFin->SetAlpha(2);
    delFin->Update();

    //    vtkPolyDataConnectivityFilter* con = vtkPolyDataConnectivityFilter::New();
    //    con->SetInput(delFin->GetOutput());
    //    con->Update();

    //    this->medialSurface->DeepCopy(con->GetOutput());


    this->medialSurface->SetPoints(delFin->GetOutput()->GetPoints());
    this->medialSurface->SetPolys(delFin->GetOutput()->GetCells());


    vtkPolyDataNormals *pdn = vtkPolyDataNormals::New();
    pdn->SetInput(this->medialSurface);
    pdn->SetFeatureAngle(90);
    pdn->Update();

    vtkSmoothPolyDataFilter* spdf = vtkSmoothPolyDataFilter::New();
    spdf->SetInput(pdn->GetOutput());
    spdf->SetRelaxationFactor(0.7);
    spdf->SetNumberOfIterations(20);
    spdf->FeatureEdgeSmoothingOff();
    spdf->BoundarySmoothingOff();
    spdf->Update();

    this->medialSurface=spdf->GetOutput();
    this->medialSurface->Update();



    cout<<"ok"<<endl;



}

double MeshAnalyser::IsIntersecting(double point1[3], double point2[3])
{
    bool inter;

    if(this->meshLocator->GetDataSet() != this->mesh)
    {
        this->meshLocator->SetDataSet(this->mesh);
        this->meshLocator->BuildLocator();
    }

    double t, ptline[3], pcoords[3];
    int subId;

    int isInter=this->meshLocator->IntersectWithLine(point1, point2, 0.0001, t, ptline, pcoords, subId);

    if(isInter==0)
    {
        inter = false;
    }
    else
    {
        inter = true;
    }


    return t;

}

