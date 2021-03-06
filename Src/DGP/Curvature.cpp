#include "Curvature.h"

namespace MagicDGP
{
    Curvature::Curvature()
    {
    }

    Curvature::~Curvature()
    {
    }

    void Curvature::CalGaussianCurvature(const Mesh3D* pMesh, std::vector<double>& curvList)
    {
        int vertNum = pMesh->GetVertexNumber();
        curvList.clear();
        curvList.resize(vertNum);
        double twoPI = 2 * 3.14159265;
        for (int vid = 0; vid < vertNum; vid++)
        {
            const Vertex3D* pVert = pMesh->GetVertex(vid);
            MagicMath::Vector3 pos = pVert->GetPosition();
            const Edge3D* pEdge = pVert->GetEdge();
            double angleSum = 0;
            do
            {
                const Face3D* pFace = pEdge->GetFace();
                if (pFace != NULL)
                {
                    MagicMath::Vector3 pos1 = pEdge->GetVertex()->GetPosition();
                    MagicMath::Vector3 pos2 = pEdge->GetNext()->GetVertex()->GetPosition();
                    MagicMath::Vector3 dir1 = pos1 - pos;
                    dir1.Normalise();
                    MagicMath::Vector3 dir2 = pos2 - pos;
                    dir2.Normalise();
                    double cosAngle = dir1 * dir2;
                    cosAngle = cosAngle > 1 ? 1 : (cosAngle < -1 ? -1 : cosAngle);
                    angleSum += acos(cosAngle);
                }
                pEdge = pEdge->GetPair()->GetNext();
            } while (pEdge != NULL && pEdge != pVert->GetEdge());
            curvList.at(vid) = twoPI - angleSum;
        }
    }

    void Curvature::CalMeanCurvature(const Mesh3D* pMesh, std::vector<double>& curvList)
    {
        double epsilon = 1.0e-5;
        int vertNum = pMesh->GetVertexNumber();
        curvList.clear();
        curvList.resize(vertNum);
        for (int vid = 0; vid < vertNum; vid++)
        {
            const MagicDGP::Vertex3D* pVert = pMesh->GetVertex(vid);
            if (pVert->GetBoundaryType() == BT_Boundary)
            {
                curvList.at(vid) = 0;
                continue;
            }
            const Edge3D* pEdge = pVert->GetEdge();
            MagicMath::Vector3 avgPos(0, 0, 0);
            double wSum = 0;
            double areaSum = 0;
            do 
            {
                areaSum += pEdge->GetFace()->GetArea();

                double wTemp = 0;
                double sinV, cosV;

                MagicMath::Vector3 dir0 = pEdge->GetVertex()->GetPosition() - pEdge->GetNext()->GetVertex()->GetPosition();
                dir0.Normalise();
                MagicMath::Vector3 dir1 = pEdge->GetPre()->GetVertex()->GetPosition() - pEdge->GetNext()->GetVertex()->GetPosition();
                dir1.Normalise();
                cosV = dir0 * dir1;
                cosV = (cosV > 1) ? 1 : ((cosV < -1) ? -1 : cosV); 
                sinV = sqrt(1 - cosV * cosV);
                sinV = (sinV < epsilon) ? epsilon : sinV;
                wTemp += cosV / sinV;

                dir0 = pEdge->GetPair()->GetVertex()->GetPosition() - pEdge->GetPair()->GetNext()->GetVertex()->GetPosition();
                dir0.Normalise();
                dir1 = pEdge->GetPair()->GetPre()->GetVertex()->GetPosition() - pEdge->GetPair()->GetNext()->GetVertex()->GetPosition();
                dir1.Normalise();
                cosV = dir0 * dir1;
                cosV = (cosV > 1) ? 1 : ((cosV < -1) ? -1 : cosV); 
                sinV = sqrt(1 - cosV * cosV);
                sinV = (sinV < epsilon) ? epsilon : sinV;
                wTemp += cosV / sinV;

                wSum += wTemp;
                avgPos += pEdge->GetVertex()->GetPosition() * wTemp;

                pEdge = pEdge->GetPair()->GetNext();
            } while(pEdge != NULL && pEdge != pVert->GetEdge());
            avgPos = avgPos / wSum;
            MagicMath::Vector3 HVector = pVert->GetPosition() - avgPos;
            double uh = HVector.Length();
            uh = uh / areaSum;
            if (HVector * pVert->GetNormal() < 0)
            {
                uh = uh * -1;
            }
            curvList.at(vid) = uh;
        }
    }
}