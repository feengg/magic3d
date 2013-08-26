#pragma once
#include "OpenNI.h"

namespace MagicApp
{
    enum ScanMode
    {
        CLOSE,
        SCANNER,
        RECORDER
    };

    class MediaStream
    {
    public:
        MediaStream();
        ~MediaStream();

    public:
        void Init();
        void StartScanner();
        void StopScanner();
        void StartRecorder();
        void StopRecorder();
        void StartRecordScanner();
        void StopRecordScanner();

        int GetResolutionX();
        int GetResolutionY();
        void WaitStream(openni::VideoFrameRef* pColorFrame, openni::VideoFrameRef* pDepthFrame);
        bool IsRecording();

    private:
        openni::Device mDevice;
        openni::VideoStream mDepthStream, mColorStream;
        ScanMode mScanMode;
        openni::Recorder mRecorder;
        bool mIsRecording;
    };


}