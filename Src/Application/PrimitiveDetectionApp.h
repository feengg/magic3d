#pragma once
#include "../Common/AppBase.h"
#include "../Tool/ViewTool.h"
#include "PrimitiveDetectionAppUI.h"
#include "../DGP/Mesh3D.h"

namespace MagicApp
{
    class PrimitiveDetectionApp : public MagicCore::AppBase
    {
    public:
        PrimitiveDetectionApp();
        ~PrimitiveDetectionApp();

        virtual bool Enter(void);
        virtual bool Update(float timeElapsed);
        virtual bool Exit(void);
        virtual bool MouseMoved( const OIS::MouseEvent &arg );
        virtual bool MousePressed( const OIS::MouseEvent &arg, OIS::MouseButtonID id );

        void SetMesh3D(MagicDGP::Mesh3D* pMesh);

    private:
        void SetupScene(void);
        void ShutdownScene(void);

    private:
        PrimitiveDetectionAppUI mUI;
        MagicTool::ViewTool mViewTool;
        MagicDGP::Mesh3D* mpMesh;
    };

}