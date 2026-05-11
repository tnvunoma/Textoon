#pragma once
#include <Eigen/Dense>
#include <Eigen/SparseCholesky>

const float EPSILON = 1e-4;
//const float SMOL_EPSILON = 1e-8;
const float BIG_EPSILON = 1e-3;

// verifies if the matrix is symmetric positive definite
inline bool isSPD(Eigen::SparseMatrix<float> mat){
    if (mat.isApprox(mat.transpose())){
        Eigen::SelfAdjointEigenSolver<Eigen::SparseMatrix<float>> solver(mat);

        Eigen::VectorXf eigenvalues = solver.eigenvalues();

        for (int x = 0; x < eigenvalues.size(); x++){
            if (eigenvalues[x] <= 0.f)
                return false;
        }

        return true;
    }
    return false;
}

inline int getFromVector(Eigen::Vector3i v, int index){
    return v[((index % 3) + 3) % 3];
}

inline bool stableEquals(float a, float b){
    return abs(a - b) <= EPSILON;
}

inline bool stableGEQ(float a, float b){
    return a - b <= EPSILON;
}

inline bool stableLEQ(float a, float b){
    return a - b >= -EPSILON;
}
