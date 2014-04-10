#include "Deformation.h"
#include "../DGP/Vector2.h"
#include "../DGP/Vector3.h"
#include "../Common/LogSystem.h"
#include "../Common/ToolKit.h"

namespace MagicDIP
{
    Deformation::Deformation()
    {
    }

    Deformation::~Deformation()
    {
    }

    cv::Mat Deformation::DeformByMovingLeastSquares(const cv::Mat& inputImg, 
            const std::vector<int>& originIndex, const std::vector<int>& targetIndex)
    {
        double startTime = MagicCore::ToolKit::GetTime();
        int imgW = inputImg.cols;
        int imgH = inputImg.rows;
        cv::Size imgSize(imgW, imgH);
        cv::Mat resImg(imgSize, CV_8UC3);
        int markNum = originIndex.size() / 2;
        std::vector<double> wList(markNum);
        std::vector<MagicDGP::Vector2> pHatList(markNum);
        std::vector<MagicDGP::Vector2> qHatList(markNum);
        MagicDGP::Vector2 pStar, qStar;
        std::vector<MagicDGP::Vector2> pList(markNum);
        for (int mid = 0; mid < markNum; mid++)
        {
            pList.at(mid) = MagicDGP::Vector2(originIndex.at(mid * 2), originIndex.at(mid * 2 + 1));
        }
        std::vector<MagicDGP::Vector2> qList(markNum);
        for (int mid = 0; mid < markNum; mid++)
        {
            qList.at(mid) = MagicDGP::Vector2(targetIndex.at(mid * 2), targetIndex.at(mid * 2 + 1));
        }
        std::vector<std::vector<double> > aMatList(markNum);
        std::vector<bool> visitFlag(imgW * imgH, 0);
        DebugLog << "Prepare time: " << MagicCore::ToolKit::GetTime() - startTime << std::endl;
        for (int hid = 0; hid < imgH; hid++)
        {
            for (int wid = 0; wid < imgW; wid++)
            {
                MagicDGP::Vector2 pos(wid, hid);
                //calculate w
                bool isMarkVertex = false;
                double wSum = 0;
                for (int mid = 0; mid < markNum; mid++)
                {
                    double dTemp = (pos - pList.at(mid)).LengthSquared(); //variable
                    if (dTemp < 1.0e-15)
                    {
                        isMarkVertex = true;
                        break;
                    }
                    wList.at(mid) = 1.0 / dTemp;
                    wSum += wList.at(mid);
                }
                //
                if (isMarkVertex)
                {
                    const unsigned char* pPixel = inputImg.ptr(hid, wid);
                    unsigned char* pResPixel = resImg.ptr(hid, wid);
                    pResPixel[0] = pPixel[0];
                    pResPixel[1] = pPixel[1];
                    pResPixel[2] = pPixel[2];
                    visitFlag.at(hid * imgW + wid) = 1;
                }
                else
                {
                    //Calculate pStar qStar
                    pStar = MagicDGP::Vector2(0.0, 0.0);
                    qStar = MagicDGP::Vector2(0.0, 0.0);
                    for (int mid = 0; mid < markNum; mid++)
                    {
                        pStar += (pList.at(mid) * wList.at(mid));
                        qStar += (qList.at(mid) * wList.at(mid));
                    }
                    pStar /= wSum;
                    qStar /= wSum;

                    //Calculate pHat qHat
                    for (int mid = 0; mid < markNum; mid++)
                    {
                        pHatList.at(mid) = pList.at(mid) - pStar;
                        qHatList.at(mid) = qList.at(mid) - qStar;
                    }
                    
                    //Calculate A
                    MagicDGP::Vector2 col0 = pos - pStar;
                    MagicDGP::Vector2 col1(col0[1], -col0[0]);
                    for (int mid = 0; mid < markNum; mid++)
                    {
                        std::vector<double> aMat(4);
                        MagicDGP::Vector2 row1(pHatList.at(mid)[1], -pHatList.at(mid)[0]);
                        aMat.at(0) = pHatList.at(mid) * col0 * wList.at(mid);
                        aMat.at(1) = pHatList.at(mid) * col1 * wList.at(mid);
                        aMat.at(2) = row1 * col0 * wList.at(mid);
                        aMat.at(3) = row1 * col1 * wList.at(mid);
                        aMatList.at(mid) = aMat;
                    }

                    //Calculate fr(v)
                    MagicDGP::Vector2 fVec(0, 0);
                    for (int mid = 0; mid < markNum; mid++)
                    {
                        fVec[0] += (qHatList.at(mid)[0] * aMatList.at(mid).at(0) + qHatList.at(mid)[1] * aMatList.at(mid).at(2));
                        fVec[1] += (qHatList.at(mid)[0] * aMatList.at(mid).at(1) + qHatList.at(mid)[1] * aMatList.at(mid).at(3));
                    }

                    //Calculate target position
                    fVec.Normalise();
                    MagicDGP::Vector2 targetPos = fVec * ((pos - pStar).Length()) + qStar;
                    int targetW = targetPos[0];
                    int targetH = targetPos[1];
                    if (targetH >= 0 && targetH < imgH && targetW >= 0 && targetW < imgW)
                    {
                        const unsigned char* pPixel = inputImg.ptr(hid, wid);
                        unsigned char* pResPixel = resImg.ptr(targetH, targetW);
                        pResPixel[0] = pPixel[0];
                        pResPixel[1] = pPixel[1];
                        pResPixel[2] = pPixel[2];
                        visitFlag.at(targetH * imgW + targetW) = 1;
                    }
                }
            }
        }
        DebugLog << "Deform time: " << MagicCore::ToolKit::GetTime() - startTime << std::endl;
        startTime = MagicCore::ToolKit::GetTime();
        //fill hole
        for (int hid = 0; hid < imgH; hid++)
        {
            int baseIndex = hid * imgW;
            for (int wid = 0; wid < imgW; wid++)
            {
                if (!visitFlag.at(baseIndex + wid))
                {
                    double wSum = 0;
                    MagicDGP::Vector3 avgColor(0, 0, 0);
                    for (int wRight = wid + 1; wRight < imgW; wRight++)
                    {
                        if (visitFlag.at(baseIndex + wRight))
                        {
                            double wTemp = 1.0 / (wRight - wid);
                            wSum += wTemp;
                            unsigned char* pPixel = resImg.ptr(hid, wRight);
                            avgColor[0] += wTemp * pPixel[0];
                            avgColor[1] += wTemp * pPixel[1];
                            avgColor[2] += wTemp * pPixel[2];
                            break;
                        }
                    }
                    for (int wLeft = wid - 1; wLeft >= 0; wLeft--)
                    {
                        if (visitFlag.at(baseIndex + wLeft))
                        {
                            double wTemp = 1.0 / (wid - wLeft);
                            wSum += wTemp;
                            unsigned char* pPixel = resImg.ptr(hid, wLeft);
                            avgColor[0] += wTemp * pPixel[0];
                            avgColor[1] += wTemp * pPixel[1];
                            avgColor[2] += wTemp * pPixel[2];
                            break;
                        }
                    }
                    for (int hUp = hid - 1; hUp >= 0; hUp--)
                    {
                        if (visitFlag.at(hUp * imgW + wid))
                        {
                            double wTemp = 1.0 / (hid - hUp);
                            unsigned char* pPixel = resImg.ptr(hUp, wid);
                            wSum += wTemp;
                            avgColor[0] += wTemp * pPixel[0];
                            avgColor[1] += wTemp * pPixel[1];
                            avgColor[2] += wTemp * pPixel[2];
                            break;
                        }
                    }
                    for (int hDown = hid + 1; hDown < imgH; hDown++)
                    {
                        if (visitFlag.at(hDown * imgW + wid))
                        {
                            double wTemp = 1.0 / (hDown - hid);
                            unsigned char* pPixel = resImg.ptr(hDown, wid);
                            wSum += wTemp;
                            avgColor[0] += wTemp * pPixel[0];
                            avgColor[1] += wTemp * pPixel[1];
                            avgColor[2] += wTemp * pPixel[2];
                            break;
                        }
                    }
                    if (wSum > 1.0e-15)
                    {
                        avgColor /= wSum;
                    }
                    unsigned char* pFillPixel = resImg.ptr(hid, wid);
                    pFillPixel[0] = avgColor[0];
                    pFillPixel[1] = avgColor[1];
                    pFillPixel[2] = avgColor[2];
                }
            }
        }
        DebugLog << "Fill hole time: " << MagicCore::ToolKit::GetTime() - startTime << std::endl;
        return resImg;
    }
}
