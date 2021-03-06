#include "stdafx.h"
#include "PrimitiveDetectionApp.h"
#include "Tool/LogSystem.h"
#include "../Common/ResourceManager.h"
#include "../Common/RenderSystem.h"
#include "../Common/ToolKit.h"
#include "../DGP/Parser.h"
#include "../DGP/Curvature.h"
#include "../DGP/Consolidation.h"
#include "../DGP/Sampling.h"
#include "../DGP/PickPointTool.h"
#include "Math/HomoMatrix4.h"
#include "../Common/ThreadPool.h"

namespace
{
    class NormalDeviationTask : public MagicCore::ITask
    {
    public:
        NormalDeviationTask(int taskId, int taskCount, MagicDGP::Mesh3D* pMesh, std::vector<double>& normDeviation) : 
            mTaskId(taskId),
            mTaskCount(taskCount),
            mpMesh(pMesh),
            mNormDeviation(normDeviation)
        {
        }

        virtual void Run()
        {
            DebugLog << "NormalDeviationTask::Run" << std::endl;
            int vertNum = mpMesh->GetVertexNumber();
            for (int vid = mTaskId; vid < vertNum; vid += mTaskCount)
            {
                std::vector<int> neighborList;
                neighborList.reserve(10);
                MagicDGP::Vertex3D* pVert = mpMesh->GetVertex(vid);
                MagicDGP::Edge3D* pEdge = pVert->GetEdge();
                do
                {
                    if (pEdge == NULL)
                    {
                        break;
                    }
                    neighborList.push_back(pEdge->GetVertex()->GetId());
                    pEdge = pEdge->GetPair()->GetNext();
                } while (pEdge != pVert->GetEdge());

                MagicMath::Vector3 normal = mpMesh->GetVertex(vid)->GetNormal();
                double nDev = 0;
                for (std::vector<int>::iterator neigItr = neighborList.begin(); neigItr != neighborList.end(); ++neigItr)
                {
                    double cosA = normal * (mpMesh->GetVertex(*neigItr)->GetNormal());
                    cosA = cosA > 1 ? 1 : (cosA < -1 ? -1 : cosA);
                    nDev += acos(cosA);
                }
                if (neighborList.size() > 0)
                {
                    nDev /= neighborList.size();
                }
                mNormDeviation.at(vid) = nDev;
            }
        }

        ~NormalDeviationTask()
        {
        }

    private:
        int mTaskId;
        int mTaskCount;
        MagicDGP::Mesh3D* mpMesh;
        std::vector<double>& mNormDeviation;
    };

}

namespace MagicApp
{
    PrimitiveDetectionApp::PrimitiveDetectionApp() : 
        mpMesh(NULL),
        mIsPickingMode(false)
    {
    }

    PrimitiveDetectionApp::~PrimitiveDetectionApp()
    {
        if (mpMesh != NULL)
        {
            delete mpMesh;
            mpMesh = NULL;
        }
    }

    bool PrimitiveDetectionApp::Enter(void)
    {
        InfoLog << "Enter PrimitiveDetectionApp" << std::endl;
        mUI.Setup();
        SetupScene();
        return true;
    }

    bool PrimitiveDetectionApp::Update(float timeElapsed)
    {
        return true;
    }

    bool PrimitiveDetectionApp::Exit(void)
    {
        ShutdownScene();
        mUI.Shutdown();
        return true;
    }

    bool PrimitiveDetectionApp::MouseMoved( const OIS::MouseEvent &arg )
    {
        if (mIsPickingMode == false)
        {
            MagicDGP::ViewTool::MouseMode mm;
            if (arg.state.buttonDown(OIS::MB_Left))
            {
                mm = MagicDGP::ViewTool::MM_Left_Down;
            }
            else if (arg.state.buttonDown(OIS::MB_Middle))
            {
                mm = MagicDGP::ViewTool::MM_Middle_Down;
            }
            else if (arg.state.buttonDown(OIS::MB_Right))
            {
                mm = MagicDGP::ViewTool::MM_Right_Down;
            }
            else
            {
                mm = MagicDGP::ViewTool::MM_None;
            }
            mViewTool.MouseMoved(arg.state.X.abs, arg.state.Y.abs, mm);
        }

        return true;
    }

    bool PrimitiveDetectionApp::MousePressed( const OIS::MouseEvent &arg, OIS::MouseButtonID id )
    {
        if (mIsPickingMode == false)
        {
            mViewTool.MousePressed(arg.state.X.abs, arg.state.Y.abs);
        }

        return true;
    }

    bool PrimitiveDetectionApp::MouseReleased( const OIS::MouseEvent &arg, OIS::MouseButtonID id )
    {
        if (mIsPickingMode)
        {
            if (mpMesh != NULL)
            {
                MagicMath::Vector2 mousePos(arg.state.X.abs * 2.0 / MagicCore::RenderSystem::GetSingleton()->GetRenderWindow()->getWidth() - 1.0, 
                    1.0 - arg.state.Y.abs * 2.0 / MagicCore::RenderSystem::GetSingleton()->GetRenderWindow()->getHeight());
                int pickIndex = MagicDGP::PickPointTool::PickMeshVertexByPoint(mpMesh, mousePos, true);
                if (pickIndex != -1)
                {
                    PrimitiveSelection(pickIndex);
                    mpMesh->GetVertex(pickIndex)->SetColor(MagicMath::Vector3(0, 0, 0));
                    MagicCore::RenderSystem::GetSingleton()->RenderMesh3D("Mesh3D", "MyCookTorrance", mpMesh);
                }
            }
            
        }
        return true;
    }

    bool PrimitiveDetectionApp::KeyPressed( const OIS::KeyEvent &arg )
    {
        if (arg.key == OIS::KC_V && mpMesh !=NULL)
        {
            MagicCore::RenderSystem::GetSingleton()->GetMainCamera()->setPolygonMode(Ogre::PolygonMode::PM_POINTS);
        }
        else if (arg.key == OIS::KC_E && mpMesh !=NULL)
        {
            MagicCore::RenderSystem::GetSingleton()->GetMainCamera()->setPolygonMode(Ogre::PolygonMode::PM_WIREFRAME);
        }
        else if (arg.key == OIS::KC_F && mpMesh !=NULL)
        {
            MagicCore::RenderSystem::GetSingleton()->GetMainCamera()->setPolygonMode(Ogre::PolygonMode::PM_SOLID);
        }
        else if (arg.key == OIS::KC_G && mpMesh != NULL)
        {
            CalMeshCurvature();
        }
        else if (arg.key == OIS::KC_S && mpMesh != NULL)
        {
            FilterMesh3D();
        }
        else if (arg.key == OIS::KC_N && mpMesh != NULL)
        {
            CalNormalDeviation();
            //CalNormalDeviationByMT();
        }
        else if (arg.key == OIS::KC_A && mpMesh != NULL)
        {
            SampleVertex();
        }
        else if (arg.key == OIS::KC_P && mpMesh != NULL)
        {
            mIsPickingMode = !mIsPickingMode;
        }
        return true;
    }

    void PrimitiveDetectionApp::SetupScene(void)
    {
        InfoLog << "PrimitiveDetectionApp::SetupScene" << std::endl;
        Ogre::SceneManager* pSceneMgr = MagicCore::RenderSystem::GetSingleton()->GetSceneManager();
        pSceneMgr->setAmbientLight(Ogre::ColourValue(0.1, 0.1, 0.1));
        Ogre::Light*  sl = pSceneMgr->createLight("SimpleLight");
        sl->setPosition(10, 10, 20);
        sl->setDiffuseColour(0.8, 0.8, 0.8);
        sl->setSpecularColour(0.5, 0.5, 0.5);
    }
     
    void PrimitiveDetectionApp::ShutdownScene(void)
    {
        InfoLog << "PrimitiveDetectionApp::ShutdownScene" << std::endl;
        Ogre::SceneManager* pSceneMgr = MagicCore::RenderSystem::GetSingleton()->GetSceneManager();
        pSceneMgr->setAmbientLight(Ogre::ColourValue::Black);
        pSceneMgr->destroyLight("SimpleLight");
        if (mpMesh != NULL)
        {
            delete mpMesh;
            mpMesh = NULL;
        }
        MagicCore::RenderSystem::GetSingleton()->HideRenderingObject("Mesh3D");
        MagicCore::RenderSystem::GetSingleton()->HideRenderingObject("Primitive");
        if (MagicCore::RenderSystem::GetSingleton()->GetSceneManager()->hasSceneNode("ModelNode"))
        {
            MagicCore::RenderSystem::GetSingleton()->GetSceneManager()->getSceneNode("ModelNode")->resetToInitialState();
        } 
    }

    bool PrimitiveDetectionApp::ImportMesh3D()
    {
        std::string fileName;
        char filterName[] = "OBJ Files(*.obj)\0*.obj\0STL Files(*.stl)\0*.stl\0";
        if (MagicCore::ToolKit::FileOpenDlg(fileName, filterName))
        {
            MagicDGP::Mesh3D* pMesh = MagicDGP::Parser::ParseMesh3D(fileName);
            if (pMesh != NULL)
            {
                pMesh->UnifyPosition(2.0);
                pMesh->UpdateNormal();
                if (mpMesh != NULL)
                {
                    delete mpMesh;
                }
                mpMesh = pMesh;
                MagicCore::RenderSystem::GetSingleton()->RenderMesh3D("Mesh3D", "MyCookTorrance", mpMesh);
                MagicCore::RenderSystem::GetSingleton()->HideRenderingObject("Primitive");
                return true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    void PrimitiveDetectionApp::RansacPrimitiveDetection()
    {
        std::vector<int> res;
        MagicDGP::PrimitiveDetection::Primitive2DDetectionEnhance(mpMesh, res);
        //MagicDGP::PrimitiveDetection::Primitive2DDetectionByScore(mpMesh, res);
        int vertNum = mpMesh->GetVertexNumber();
        for (int i = 0; i < vertNum; i++)
        {
            float cv = res.at(i) * 0.2f;
            MagicMath::Vector3 color = MagicCore::ToolKit::ColorCoding(cv);
            mpMesh->GetVertex(i)->SetColor(color);
        }
        MagicCore::RenderSystem::GetSingleton()->RenderMesh3D("Mesh3D", "MyCookTorrance", mpMesh);
    }

    void PrimitiveDetectionApp::PrimitiveSelection(int sampleId)
    {
        int vertNum = mpMesh->GetVertexNumber();
        std::vector<int> res;
        //MagicDGP::ShapeCandidate* pCand = MagicDGP::PrimitiveDetection::Primitive2DSelectionByVertex(mpMesh, sampleId, res);
        MagicDGP::ShapeCandidate* pCand = MagicDGP::PrimitiveDetection::Primitive2DSelectionByVertexSampling(mpMesh, sampleId, res);
        if (pCand != NULL)
        {
            for (int i = 0; i < vertNum; i++)
            {
                if (res.at(i) == MagicDGP::PrimitiveType::None)
                {
                    mpMesh->GetVertex(i)->SetColor(MagicMath::Vector3(0.9, 0.9, 0.9));
                }
                else
                {
                    float cv = res.at(i) * 0.2f;
                    MagicMath::Vector3 color = MagicCore::ToolKit::ColorCoding(cv);
                    mpMesh->GetVertex(i)->SetColor(color);
                }
            }
            MagicCore::RenderSystem::GetSingleton()->RenderMesh3D("Mesh3D", "MyCookTorrance", mpMesh);
            DrawPrimitive(pCand);
            if (pCand != NULL)
            {
                delete pCand;
            }
        }  
    }

    void PrimitiveDetectionApp::DrawPrimitive(MagicDGP::ShapeCandidate* pCand)
    {
        if (pCand != NULL)
        {
            MagicDGP::PrimitiveType candType = pCand->GetType();
            if (candType == MagicDGP::PrimitiveType::Plane)
            {
                MagicDGP::PlaneCandidate* planeCand = dynamic_cast<MagicDGP::PlaneCandidate* >(pCand);
                MagicMath::Vector3 centerPos = planeCand->mCenter;
                MagicMath::Vector3 normal = planeCand->mNormal;
                MagicMath::HomoMatrix4 rotateMat;
                rotateMat.GenerateVectorToVectorRotation(MagicMath::Vector3(0, 0, 1), normal);
                MagicMath::HomoMatrix4 translateMat;
                translateMat.GenerateTranslation(centerPos);
                MagicMath::HomoMatrix4 totalMat = translateMat * rotateMat;
                MagicDGP::Mesh3D* planeMesh = MagicDGP::Parser::ParseMesh3D("../../Media/Model/plane.obj");
                if (planeMesh != NULL)
                {
                    int vertNum = planeMesh->GetVertexNumber();
                    for (int vid = 0; vid < vertNum; vid++)
                    {
                        MagicDGP::Vertex3D* pVert = planeMesh->GetVertex(vid);
                        MagicMath::Vector3 pos = pVert->GetPosition();
                        pos = totalMat.TransformPoint(pos);
                        pVert->SetPosition(pos);
                    }
                    planeMesh->UpdateNormal();
                    MagicCore::RenderSystem::GetSingleton()->RenderBlendMesh3D("Primitive", "MyCookTorranceBlend", planeMesh, 0.6);
                }
            }
            else if (candType == MagicDGP::PrimitiveType::Sphere)
            {
                MagicDGP::SphereCandidate* sphereCand = dynamic_cast<MagicDGP::SphereCandidate* >(pCand);
                MagicMath::Vector3 centerPos = sphereCand->mCenter;
                double radius = sphereCand->mRadius;
                MagicMath::HomoMatrix4 translateMat;
                translateMat.GenerateTranslation(centerPos);
                MagicDGP::Mesh3D* sphereMesh = MagicDGP::Parser::ParseMesh3D("../../Media/Model/sphere.obj");
                if (sphereCand != NULL)
                {
                    int vertNum = sphereMesh->GetVertexNumber();
                    for (int vid = 0; vid < vertNum; vid++)
                    {
                        MagicDGP::Vertex3D* pVert = sphereMesh->GetVertex(vid);
                        MagicMath::Vector3 pos = pVert->GetPosition();
                        pos *= radius;
                        pos = translateMat.TransformPoint(pos);
                        pVert->SetPosition(pos);
                    }
                    sphereMesh->UpdateNormal();
                    MagicCore::RenderSystem::GetSingleton()->RenderBlendMesh3D("Primitive", "MyCookTorranceBlend", sphereMesh, 0.8);
                }
            }
            else if (candType == MagicDGP::PrimitiveType::Cylinder)
            {
                MagicDGP::CylinderCandidate* cylinderCand = dynamic_cast<MagicDGP::CylinderCandidate* >(pCand);
                //cylinderCand->Rectify(mpMesh);
                MagicMath::Vector3 centerPos = cylinderCand->mCenter;
                MagicMath::Vector3 direction = cylinderCand->mDir;
                double radius = cylinderCand->mRadius;
                MagicMath::HomoMatrix4 rotateMat;
                rotateMat.GenerateVectorToVectorRotation(MagicMath::Vector3(0, 0, 1), direction);
                MagicMath::HomoMatrix4 translateMat;
                translateMat.GenerateTranslation(centerPos);
                MagicMath::HomoMatrix4 totalMat = translateMat * rotateMat;
                MagicDGP::Mesh3D* cylinderMesh = MagicDGP::Parser::ParseMesh3D("../../Media/Model/cylinder.obj");
                if (cylinderMesh != NULL)
                {
                    int vertNum = cylinderMesh->GetVertexNumber();
                    for (int vid = 0; vid < vertNum; vid++)
                    {
                        MagicDGP::Vertex3D* pVert = cylinderMesh->GetVertex(vid);
                        MagicMath::Vector3 pos = pVert->GetPosition();
                        pos *= radius;
                        pos = totalMat.TransformPoint(pos);
                        pVert->SetPosition(pos);
                    }
                    cylinderMesh->UpdateNormal();
                    MagicCore::RenderSystem::GetSingleton()->RenderBlendMesh3D("Primitive", "MyCookTorranceBlend", cylinderMesh, 0.6);
                }
            }
            else if (candType == MagicDGP::PrimitiveType::Cone)
            {
                MagicDGP::ConeCandidate* coneCand = dynamic_cast<MagicDGP::ConeCandidate* >(pCand);
                MagicMath::Vector3 apex = coneCand->mApex;
                MagicMath::Vector3 direction = coneCand->mDir;
                double angle = coneCand->mAngle;
                double radius = tan(angle);
                MagicMath::HomoMatrix4 rotateMat;
                MagicMath::Vector3 zAxis(0, 0, 1);
                if ((zAxis.CrossProduct(direction)).LengthSquared() < 1.0e-15 &&
                    zAxis * direction < 0)
                {
                    MagicMath::HomoMatrix4 rotateTemp0, rotateTemp1;
                    rotateTemp0.GenerateVectorToVectorRotation(zAxis, MagicMath::Vector3(0, 1, 0));
                    rotateTemp1.GenerateVectorToVectorRotation(MagicMath::Vector3(0, 1, 0), direction);
                    rotateMat = rotateTemp1 * rotateTemp0;
                }
                else
                {
                    rotateMat.GenerateVectorToVectorRotation(MagicMath::Vector3(0, 0, 1), direction);
                }
                MagicMath::HomoMatrix4 translateMat;
                translateMat.GenerateTranslation(apex);
                MagicMath::HomoMatrix4 totalMat = translateMat * rotateMat;
                MagicDGP::Mesh3D* coneMesh = MagicDGP::Parser::ParseMesh3D("../../Media/Model/cone.obj");
                if (coneMesh != NULL)
                {
                    int vertNum = coneMesh->GetVertexNumber();
                    for (int vid = 0; vid < vertNum; vid++)
                    {
                        MagicDGP::Vertex3D* pVert = coneMesh->GetVertex(vid);
                        MagicMath::Vector3 pos = pVert->GetPosition();
                        pos[0] = pos[0] * radius;
                        pos[1] = pos[1] * radius;
                        pos = totalMat.TransformPoint(pos);
                        pVert->SetPosition(pos);
                    }
                    coneMesh->UpdateNormal();
                    MagicCore::RenderSystem::GetSingleton()->RenderBlendMesh3D("Primitive", "MyCookTorranceBlend", coneMesh, 0.6);
                }
            }
            else
            {
                DebugLog << "No Type" << std::endl;
            }
        }
        else
        {
            MagicCore::RenderSystem::GetSingleton()->HideRenderingObject("Primitive");
        }
    }

    void PrimitiveDetectionApp::CalMeshCurvature()
    {
        mpMesh->CalculateBBox();
        MagicMath::Vector3 bboxMin, bboxMax;
        mpMesh->GetBBox(bboxMin, bboxMax);
        double bboxSize = (bboxMax - bboxMin).Length();
        double bboxArea = bboxSize * bboxSize;
        mpMesh->CalculateFaceArea();

        int filterNum = 3;
        int vertNum = mpMesh->GetVertexNumber();
        std::vector<double> gaussianCurvList(vertNum, 0);
        for (int filterIdx = 0; filterIdx < filterNum; filterIdx++)
        {
            MagicDGP::Consolidation::SimpleMeshSmooth(mpMesh);
            std::vector<double> localGaussianCurvList;
            MagicDGP::Curvature::CalGaussianCurvature(mpMesh, localGaussianCurvList);
            for (int vid = 0; vid < vertNum; vid++)
            {
                gaussianCurvList.at(vid) += localGaussianCurvList.at(vid);
            }
        }
        for (int vid = 0; vid < vertNum; vid++)
        {
            gaussianCurvList.at(vid) /= filterNum;
        }
        //std::vector<double> gaussianCurvList;
        //MagicDGP::Curvature::CalGaussianCurvature(mpMesh, gaussianCurvList);
        //int vertNum = mpMesh->GetVertexNumber();

        for (int vid = 0; vid < vertNum; vid++)
        {
            MagicDGP::Vertex3D* pVert = mpMesh->GetVertex(vid);
            MagicDGP::Edge3D* pEdge = pVert->GetEdge();
            double area = 0;
            do
            {
                MagicDGP::Face3D* pFace = pEdge->GetFace();
                if (pFace != NULL)
                {
                    area += pFace->GetArea();
                }
                pEdge = pEdge->GetPair()->GetNext();
            } while (pEdge != NULL && pEdge != pVert->GetEdge());
            area /= bboxArea;
            float cv = gaussianCurvList.at(vid);
            if (area > 1.0e-10)
            {
                cv /= area;    
            }
            cv = 0.6 + cv * 0.05;
            //if (cv > 0.7)
            //{
            //    cv = 0;
            //}
            //else
            //{
            //    cv = 0.6;
            //}
            DebugLog << "Gaussian: " << cv << std::endl;
            MagicMath::Vector3 color = MagicCore::ToolKit::ColorCoding(cv);
            mpMesh->GetVertex(vid)->SetColor(color);
        }
        MagicCore::RenderSystem::GetSingleton()->RenderMesh3D("Mesh3D", "MyCookTorrance", mpMesh);
    }

    void PrimitiveDetectionApp::CalScaleGradient(std::vector<double>& scaleField, std::vector<MagicMath::Vector3>& gradientField, 
            const MagicDGP::Mesh3D* pMesh)
    {
        std::vector<MagicMath::Vector3> faceGradient;
        int faceNum = pMesh->GetFaceNumber();
		for (int fid = 0; fid < faceNum; fid++)
		{
            const MagicDGP::Edge3D* pEdge = pMesh->GetFace(fid)->GetEdge();
            const MagicDGP::Vertex3D* v1 = pEdge->GetVertex();
            const MagicDGP::Vertex3D* v2 = pEdge->GetNext()->GetVertex();
            const MagicDGP::Vertex3D* v3 = pEdge->GetPre()->GetVertex();

			MagicMath::Vector3 orignal, newV1, newV2, newV3, newY, newX, dirG;
            //double a;
			//a = computeCoeff(v1, v2, v3);
            //
            MagicMath::Vector3 vec1 = v1->GetPosition() - v2->GetPosition();
            MagicMath::Vector3 vec2 = v3->GetPosition() - v2->GetPosition();
            double a = (vec1[0] * vec2[0] + vec1[1] * vec2[1] + vec2[2] * vec1[2]) / 
			(vec1[0] * vec1[0] + vec1[1] *vec1[1] + vec1[2] * vec1[2]);
            //
            orignal = v1->GetPosition() * a + v2->GetPosition() * (1 - a);
			newY = (v3->GetPosition() - (v1->GetPosition() * a + v2->GetPosition() * (1 - a)));
			newX = (v1->GetPosition() - v2->GetPosition());
			newX.Normalise();
			newY.Normalise();

			newV3 = MagicMath::Vector3(0, (v3->GetPosition() - orignal).Length(), 0);
			newV2 = MagicMath::Vector3(-(v2->GetPosition() - orignal).Length(), 0, 0);
			newV1 = MagicMath::Vector3((v1->GetPosition() - orignal).Length(), 0, 0);

            dirG = MagicMath::Vector3((scaleField.at(v1->GetId()) - scaleField.at(v2->GetId())) / 
				(newV1[0] - newV2[0]), (scaleField.at(v3->GetId()) - scaleField.at(v1->GetId()) +
				newV1[0] * (scaleField.at(v1->GetId()) - scaleField.at(v2->GetId())) /
				( newV1[0] - newV2[0])) / newV3[1], 0);

			double gV = dirG.Length();
			MagicMath::Vector3 gDir = newX * dirG[0] + newY * dirG[1];
			gDir.Normalise();
			faceGradient.push_back(gDir * gV);	
		}
		gradientField.clear();
        int vertNum = pMesh->GetVertexNumber();
        gradientField.resize(vertNum);
		for (int vid = 0; vid < vertNum; vid++)
		{
            const MagicDGP::Vertex3D* pVert = pMesh->GetVertex(vid);
            const MagicDGP::Edge3D* pEdge = pVert->GetEdge();
			MagicMath::Vector3 vertG(0.f, 0.f, 0.f);
			int neighborNum = 0;
			do 
			{
                if (pEdge->GetPair()->GetFace() == NULL)
                {
                    break;
                }
                vertG = vertG + faceGradient.at(pEdge->GetPair()->GetFace()->GetId());
                pEdge = pEdge->GetPair()->GetNext();
				neighborNum++;
            } while (pEdge != pVert->GetEdge());
			if (neighborNum > 0)
			{
				vertG = vertG / float(neighborNum);
			}
            gradientField.at(vid) = vertG;
        }
    }

    void PrimitiveDetectionApp::CalNormalDeviation()
    {
        float timeStart = MagicCore::ToolKit::GetTime();
        //mpMesh->CalculateFaceArea();
        //mpMesh->UpdateBoundaryFlag();
        //int vertNum = mpMesh->GetVertexNumber();
        //std::vector<double> meanCurv;
        //MagicDGP::Curvature::CalMeanCurvature(mpMesh, meanCurv);
        //std::vector<MagicMath::Vector3> curvGrad;
        //CalScaleGradient(meanCurv, curvGrad, mpMesh);
        //std::vector<double> curvGradValue(vertNum);
        //for (int vid = 0; vid < vertNum; vid++)
        //{
        //    curvGradValue.at(vid) = curvGrad.at(vid).Length();
        //}
        //std::vector<MagicMath::Vector3> curvGradGrad;
        //CalScaleGradient(curvGradValue, curvGradGrad, mpMesh);
        //for (int vid = 0; vid < vertNum; vid++)
        //{
        //    MagicMath::Vector3 gradDir = curvGrad.at(vid);
        //    gradDir.Normalise();
        //    double value = gradDir * curvGradGrad.at(vid);
        //    MagicMath::Vector3 color = MagicCore::ToolKit::ColorCoding(value / 10000 + 0.7);
        //    //DebugLog << "mean curv: " << value / 10000 << std::endl;
        //    mpMesh->GetVertex(vid)->SetColor(color);
        //}
        int vertNum = mpMesh->GetVertexNumber();
        std::vector<double> norDev(vertNum);
        for (int vid = 0; vid < vertNum; vid++)
        {
            std::vector<int> neighborList;
            neighborList.reserve(10);
            MagicDGP::Vertex3D* pVert = mpMesh->GetVertex(vid);
            MagicDGP::Edge3D* pEdge = pVert->GetEdge();
            do
            {
                if (pEdge == NULL)
                {
                    break;
                }
                neighborList.push_back(pEdge->GetVertex()->GetId());
                pEdge = pEdge->GetPair()->GetNext();
            } while (pEdge != pVert->GetEdge());

            MagicMath::Vector3 normal = mpMesh->GetVertex(vid)->GetNormal();
            double nDev = 0;
            for (std::vector<int>::iterator neigItr = neighborList.begin(); neigItr != neighborList.end(); ++neigItr)
            {
                //nDev += (normal - (mpMesh->GetVertex(*neigItr)->GetNormal())).Length();
                double cosA = normal * (mpMesh->GetVertex(*neigItr)->GetNormal());
                cosA = cosA > 1 ? 1 : (cosA < -1 ? -1 : cosA);
                nDev += acos(cosA);
            }
            if (neighborList.size() > 0)
            {
                nDev /= neighborList.size();
            }
            norDev.at(vid) = nDev;
            nDev = nDev * 10 + 0.2;
            //DebugLog << "nDev: " << nDev << std::endl;
            MagicMath::Vector3 color = MagicCore::ToolKit::ColorCoding(nDev);
            mpMesh->GetVertex(vid)->SetColor(color);
        }
        //std::vector<MagicMath::Vector3> norGrad;
        //CalScaleGradient(norDev, norGrad, mpMesh);
        //static int smoothNum = 1;
        ////smoothNum = smoothNum % 2;
        //for (int sid = 0; sid < smoothNum; sid++)
        //{
        //    std::vector<MagicMath::Vector3> smoothNorGrad(vertNum);
        //    for (int vid = 0; vid < vertNum; vid++)
        //    {
        //        MagicDGP::Vertex3D* pVert = mpMesh->GetVertex(vid);
        //        MagicDGP::Edge3D* pEdge = pVert->GetEdge();
        //        MagicMath::Vector3 avgGrad(0, 0, 0);
        //        int neigNum = 0;
        //        do
        //        {
        //            if (pEdge == NULL)
        //            {
        //                break;
        //            }
        //            avgGrad += norGrad.at(pEdge->GetVertex()->GetId());
        //            neigNum++;
        //            pEdge = pEdge->GetPair()->GetNext();
        //        } while (pEdge != pVert->GetEdge());
        //        if (neigNum > 0)
        //        {
        //            avgGrad /= neigNum;
        //            smoothNorGrad.at(vid) = avgGrad;
        //        }
        //        else
        //        {
        //            smoothNorGrad.at(vid) = norGrad.at(vid);
        //        }
        //    }
        //    norGrad = smoothNorGrad;
        //}
        ////smoothNum++;
        ////smooth
        //for (int vid  = 0; vid  < vertNum; vid ++)
        //{
        //    double v = norGrad.at(vid).Length() / 10 + 0.2;
        //    /*if (v < 0.8)
        //    {
        //        v = 0.25;
        //    }*/
        //    MagicMath::Vector3 color = MagicCore::ToolKit::ColorCoding(v);
        //    //DebugLog << "norGrad: " << norGrad.at(vid).Length() / 10 << std::endl;
        //    mpMesh->GetVertex(vid)->SetColor(color);
        //}

        //double scale = 5;
        ////std::map<double, int> scoreMap;
        //for (int vid = 0; vid < vertNum; vid++)
        //{
        //    MagicDGP::Vertex3D* pVert = mpMesh->GetVertex(vid);
        //    MagicDGP::Edge3D* pEdge = pVert->GetEdge();
        //    double devGrad = 0;
        //    int neigNum = 0;
        //    do
        //    {
        //        if (pEdge == NULL)
        //        {
        //            break;
        //        }
        //        devGrad += fabs(norDev.at(vid) - norDev.at(pEdge->GetVertex()->GetId()));
        //        neigNum++;
        //        pEdge = pEdge->GetPair()->GetNext();
        //    } while (pEdge != pVert->GetEdge());
        //    if (neigNum > 0)
        //    {
        //        devGrad /= neigNum;
        //    }
        //    devGrad = devGrad * scale + 0.2;
        //    /*if (devGrad > 0.4)
        //    {
        //        devGrad = 1.2;
        //    }
        //    else
        //    {
        //        devGrad = 0.4;
        //    }*/
        //    //scoreMap[devGrad] = vid;
        //    MagicMath::Vector3 color = MagicCore::ToolKit::ColorCoding(devGrad);
        //    mpMesh->GetVertex(vid)->SetColor(color);
        //}

        /*int validNum = vertNum / 2;
        int validIndex = 0;
        MagicMath::Vector3 color(77.0 / 255.0, 0, 153.0 / 255.0);
        for (std::map<double, int>::iterator itr = scoreMap.begin(); itr != scoreMap.end(); ++itr)
        {
            mpMesh->GetVertex(itr->second)->SetColor(color);
            validIndex++;
            if (validIndex == validNum)
            {
                break;
            }
        }*/
        DebugLog << "CalNormalDeviation: total time: " << MagicCore::ToolKit::GetTime() - timeStart << std::endl;
        MagicCore::RenderSystem::GetSingleton()->RenderMesh3D("Mesh3D", "MyCookTorrance", mpMesh);
    }

    void PrimitiveDetectionApp::CalNormalDeviationByMT()
    {
        float timeStart = MagicCore::ToolKit::GetTime();
        int vertNum = mpMesh->GetVertexNumber();
        std::vector<double> norDev(vertNum);
        int threadCount = MagicCore::GetNumberOfProcessors();
        DebugLog << "Processor number: " << threadCount << std::endl;
        MagicCore::ThreadPool threadPool(threadCount);
        for (int tid = 0; tid < threadCount; tid++)
        {
            NormalDeviationTask* pTask = new NormalDeviationTask(tid, threadCount, mpMesh, norDev);
            threadPool.InsertTask(pTask);
        }
        threadPool.WaitUntilAllDone();
        DebugLog << "Finish multi-thread normal deviation" << std::endl;
        double scale = 10;
        for (int vid = 0; vid < vertNum; vid++)
        {
            MagicDGP::Vertex3D* pVert = mpMesh->GetVertex(vid);
            MagicDGP::Edge3D* pEdge = pVert->GetEdge();
            double devGrad = 0;
            int neigNum = 0;
            do
            {
                if (pEdge == NULL)
                {
                    break;
                }
                devGrad += fabs(norDev.at(vid) - norDev.at(pEdge->GetVertex()->GetId()));
                neigNum++;
                pEdge = pEdge->GetPair()->GetNext();
            } while (pEdge != pVert->GetEdge());
            if (neigNum > 0)
            {
                devGrad /= neigNum;
            }
            devGrad = devGrad * scale + 0.2;
            MagicMath::Vector3 color = MagicCore::ToolKit::ColorCoding(devGrad);
            mpMesh->GetVertex(vid)->SetColor(color);
        }
        DebugLog << "CalNormalDeviationByMT: total time: " << MagicCore::ToolKit::GetTime() - timeStart << std::endl;
        MagicCore::RenderSystem::GetSingleton()->RenderMesh3D("Mesh3D", "MyCookTorrance", mpMesh);
    }

    void PrimitiveDetectionApp::SampleVertex()
    {
        if (mpMesh != NULL)
        {
            std::vector<int> sampleIndex;
            MagicDGP::Sampling::MeshVertexUniformSampling(mpMesh, 200, sampleIndex);
            MagicMath::Vector3 color = MagicCore::ToolKit::ColorCoding(0.2);
            for (int sid = 0; sid < sampleIndex.size(); sid++)
            {
                mpMesh->GetVertex(sampleIndex.at(sid))->SetColor(color);
            }
            MagicCore::RenderSystem::GetSingleton()->RenderMesh3D("Mesh3D", "MyCookTorrance", mpMesh);
        }
    }

    void PrimitiveDetectionApp::FilterMesh3D()
    {
        MagicDGP::Consolidation::SimpleMeshSmooth(mpMesh);
        MagicCore::RenderSystem::GetSingleton()->RenderMesh3D("Mesh3D", "MyCookTorrance", mpMesh);
    }

}

