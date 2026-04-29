#pragma once
#include <Eigen>

const float EPSILON = 1e-4;
//const float SMOL_EPSILON = 1e-8;
const float BIG_EPSILON = 1e-3;

// verifies if the matrix is symmetric positive definite
inline bool isSPD(Eigen::MatrixXf mat){
    if (mat.transpose() == mat){
        Eigen::SelfAdjointEigenSolver<Eigen::MatrixXf> solver(mat);

        Eigen::VectorXf eigenvalues = solver.eigenvalues();

        for (int x = 0; x < eigenvalues.size(); x++){
            if (eigenvalues[x] <= 0.f)
                return false;
        }

        return true;
    }
    return false;
}

inline float computeCot(Eigen::Vector3f x, Eigen::Vector3f y, Eigen::Vector3f z){
    Eigen::Vector3f u = (x - y);
    Eigen::Vector3f v = (x - z);
    return u.dot(v) / u.cross(v).norm();

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
