#include "Clustering.h"
#include "../Common/LogSystem.h"
#include "Eigen/Eigenvalues"
#include "flann/flann.h"

namespace MagicML
{
    Clustering::Clustering()
    {
    }

    Clustering::~Clustering()
    {
    }

    void Clustering::OrchardBoumanClustering(const std::vector<double>& inputData, int dim, int k, std::vector<int>& clusterRes)
    {
        std::vector<std::vector<double> > eigenVectorList(k);
        std::vector<double> eigenValueList(k);
        std::vector<std::vector<double> > meanVectorList(k);
        std::vector<std::vector<int> > clusterIndexList(k);
        int dataCount = inputData.size() / dim;
        std::vector<int> clusterStart(dataCount);
        for (int did = 0; did < dataCount; did++)
        {
            clusterStart.at(did) = did;
        }
        clusterIndexList.at(0) = clusterStart;
        int splitIndex = 0;
        std::vector<double> eigenVector;
        double eigenValue;
        std::vector<double> meanVector;
        CalEigenVector(inputData, dim, clusterIndexList.at(splitIndex), eigenVector, eigenValue, meanVector);
        eigenValueList.at(splitIndex) = eigenValue;
        eigenVectorList.at(splitIndex) = eigenVector;
        meanVectorList.at(splitIndex) = meanVector;
        for (int cid = 1; cid < k; cid++)
        {
            std::vector<int> clusterA, clusterB;
            SplitSetByEigenVector(inputData, dim, clusterIndexList.at(splitIndex), eigenVectorList.at(splitIndex), 
                meanVectorList.at(splitIndex), clusterA, clusterB);
            
            //update clusterIndexList
            clusterIndexList.at(splitIndex) = clusterA;
            clusterIndexList.at(cid) = clusterB;

            if (cid == k - 1)
            {
                break;
            }

            //update eigenVectorList, eigenValueList, meanVectorList
            CalEigenVector(inputData, dim, clusterA, eigenVector, eigenValue, meanVector);
            eigenValueList.at(splitIndex) = eigenValue;
            eigenVectorList.at(splitIndex) = eigenVector;
            meanVectorList.at(splitIndex) = meanVector;
            CalEigenVector(inputData, dim, clusterB, eigenVector, eigenValue, meanVector);
            eigenValueList.at(cid) = eigenValue;
            eigenVectorList.at(cid) = eigenVector;
            meanVectorList.at(cid) = meanVector;

            //update splitIndex
            double largestEigenValue = eigenValueList.at(0);
            splitIndex = 0;
            for (int sid = 0; sid <= cid; sid++)
            {
                if (eigenValueList.at(sid) > largestEigenValue)
                {
                    largestEigenValue = eigenValueList.at(sid);
                    splitIndex = sid;
                }
            }
        }
        //copy result
        clusterRes.clear();
        clusterRes.resize(dataCount);
        for (int cid = 0; cid < k; cid++)
        {
            std::vector<int>& clusterIndex = clusterIndexList.at(cid);
            int clusterSize = clusterIndex.size();
            for (int i = 0; i < clusterSize; i++)
            {
                clusterRes.at(clusterIndex.at(i)) = cid;
            }
        }
    }

    void Clustering::CalEigenVector(const std::vector<double>& inputData, int dim, const std::vector<int>& inputIndex, 
            std::vector<double>& eigenVector, double& eigenValue, std::vector<double>& meanVector)
    {
        int dataCount = inputIndex.size();
        meanVector = std::vector<double>(dim, 0.0);
        for (int sid = 0; sid < dataCount; sid++)
        {
            int baseIndex = inputIndex.at(sid) * dim;
            for (int did = 0; did < dim; did++)
            {
                meanVector.at(did) += inputData.at(baseIndex + did);
            }
        }
        for (int did = 0; did < dim; did++)
        {
            meanVector.at(did) /= dataCount;
        }
        Eigen::MatrixXd RMat(dim, dim);
        for (int rid = 0; rid < dim; rid++)
        {
            for (int cid = 0; cid < dim; cid++)
            {
                RMat(rid, cid) = 0.0;
            }
        }
        for (int sid = 0; sid < dataCount; sid++)
        {
            int baseIndex = inputIndex.at(sid) * dim;
            for (int rid= 0; rid < dim; rid++)
            {
                for (int cid = 0; cid < dim; cid++)
                {
                    RMat(rid, cid) += (inputData.at(baseIndex + rid) * inputData.at(baseIndex + cid));
                }
            }
        }
        for (int rid = 0; rid < dim; rid++)
        {
            for (int cid = 0; cid < dim; cid++)
            {
                RMat(rid, cid) -= (meanVector.at(rid) * meanVector.at(cid) * dataCount);
            }
        }
        Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(RMat);
        eigenValue = es.eigenvalues()(dim - 1);
        Eigen::VectorXd maxVec = es.eigenvectors().col(dim - 1);
        eigenVector = std::vector<double>(dim);
        for (int did = 0; did < dim; did++)
        {
            eigenVector.at(did) = maxVec(did);
        }
    }

    void Clustering::SplitSetByEigenVector(const std::vector<double>& inputData, int dim, const std::vector<int>& inputIndex,
            const std::vector<double>& eigenVector, const std::vector<double>& meanVector,
            std::vector<int>& clusterA, std::vector<int>& clusterB)
    {
        clusterA.clear();
        clusterB.clear();
        int dataCount = inputIndex.size();
        double meanValue = 0.0;
        for (int did = 0; did < dim; did++)
        {
            meanValue += (eigenVector.at(did) * meanVector.at(did));
        }
        for (int sid = 0; sid < dataCount; sid++)
        {
            int baseIndex = inputIndex.at(sid) * dim;
            double dataProjectValue = 0.0;
            for (int did = 0; did < dim; did++)
            {
                dataProjectValue += (inputData.at(baseIndex + did) * eigenVector.at(did));
            }
            if (dataProjectValue <= meanValue)
            {
                clusterA.push_back(inputIndex.at(sid));
            }
            else
            {
                clusterB.push_back(inputIndex.at(sid));
            }
        }
    }

    void Clustering::MeanshiftValue(const std::vector<double>& sourceData, int dim, double h, 
                                    const std::vector<double>& inputData, std::vector<double>& resData)
    {
        //double radius = h * 7 * 2.3; //according to h
        double radius = h * h;
        //calculate near neighbors by radius
        double* pSrcData = new double[sourceData.size()];
        for (int i = 0; i < sourceData.size(); i++)
        {
            pSrcData[i] = sourceData.at(i);
        }
        int dataCount = sourceData.size() / dim;
        int queryCount = inputData.size() / dim;
        flann::Matrix<double> dataSet(pSrcData, dataCount, dim);
        flann::Index<L2<double> > index(dataSet, flann::KDTreeIndexParams());
        index.buildIndex();
        //Mean shift every query
        resData.clear();
        resData.resize(sourceData.size());
        double* pQueryData = new double[dim];
        double* pMeanData = new double[dim];
        //double epsilon = h * 1.0e-7;
        for (int qid = 0; qid < queryCount; qid++)
        {
            int baseIndex = qid * dim;
            for (int did = 0; did < dim; did++)
            {
                pQueryData[did] = inputData.at(baseIndex + did);
            }
            for (int iterId = 0; iterId < 5; iterId++)
            {
                //DebugLog << "pQuery" << iterId << " : " << pQueryData[0] << " " << pQueryData[1] << " " << pQueryData[2] << std::endl;
                std::vector<std::vector<int> > nearIndex;
                std::vector<std::vector<double> > nearDist;
                flann::Matrix<double> querySet(pQueryData, 1, dim);
                index.radiusSearch(querySet, nearIndex, nearDist, radius, flann::SearchParams(-1));
                int nearCount = nearIndex.at(0).size();
                if (nearIndex.size () != 1 || nearDist.size() != 1) 
                {
                    DebugLog << "error: nearIndex.size () != 1 || nearDist.size() != 1" << std::endl;
                }
                if (nearCount == 0)
                {
                    DebugLog << "nearCount == 0, iterId: " << iterId << std::endl;
                    break;
                }
                /*else 
                {
                    DebugLog << "iterId: " << iterId << " count: " << nearCount << std::endl;
                }*/
                double coefSum = 0.0;
                for (int did = 0; did < dim; did++)
                {
                    pMeanData[did] = 0.0;
                }
                for (int nid = 0; nid < nearCount; nid++)
                {
                    double coef = GaussianValue(nearDist.at(0).at(nid), h);
                    //DebugLog << "nearDist" << nid << ": " << nearDist.at(0).at(nid) << " index: " << nearIndex.at(0).at(nid) << std::endl;
                    coefSum += coef;
                    int baseIndex = nearIndex.at(0).at(nid) * dim;
                    for (int did = 0; did < dim; did++)
                    {
                        pMeanData[did] += coef * sourceData.at(baseIndex + did);
                    }
                }
                for (int did = 0; did < dim; did++)
                {
                    pMeanData[did] /= coefSum;
                }
                //check if pMeanData is near pQueryData enough

                double* pTemp = pMeanData;
                pMeanData = pQueryData;
                pQueryData = pTemp;
            }
            //copy result
            for (int did = 0; did < dim; did++)
            {
                resData.at(baseIndex + did) = pQueryData[did];
            }
        }
        delete []pQueryData;
        pQueryData = NULL;
        delete []pMeanData;
        pMeanData = NULL;
        delete []pSrcData;
        pSrcData = NULL;
    }

    double Clustering::GaussianValue(double dist, double h)
    {
        //return 1.0 / exp(dist / h);
        return 1.0;
    }
}
