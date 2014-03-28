#include "Clustering.h"
#include "../Common/LogSystem.h"
#include "Eigen/Eigenvalues"

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

}