#include "normalmapgenerator.h"
#include "iostream"
/// TOOD: constructor
const float DEPTH_STEP = 0.5f;

NormalMapGenerator::NormalMapGenerator(QImage* image) :
    height(image->size().height()),
    width(image->size().width()),
    original_image(image)
{

    constructMatrices();
};


bool onSilhouette(){
    // dirichlet boundary condition
    return false;
}

bool belowOcclusion(){
    // neumann boundary condition
    return false;
}



bool NormalMapGenerator::isBoundary(QRgb *data, int i, unordered_set<int> neighbors){
    for (int j : neighbors){
        if (data[j] != data[i]){
            return true;
        }
    }
    return false;
}

void NormalMapGenerator::constructMatrices() {
    typedef Eigen::Triplet<float> T;


    std::vector<T> triplets;
    int n = width*height;
    L.resize(n, n);
    L.setZero();
    b.resize(n, 2);
    b.setZero();

    QRgb *data = reinterpret_cast<QRgb*>(original_image->bits());

    for (int i = 0; i < n; i++){
        /// enforces Dirichlet condition: sum of lapacian entries (i, j)
        /// for all neighbors j in the row i must be equal to d'pq
        unordered_set<int> neighbors = getValidNeighbors(i, height, width);

        if (isBoundary(data, i, neighbors)){
            triplets.push_back(T(i, i, 1.f));
            b.row(i) = RowVector2f{DEPTH_STEP, DEPTH_STEP};
        } else {
            for (int j : neighbors){
                auto j_neighbors = getValidNeighbors(j, height, width);

                if (isBoundary(data, j, j_neighbors)) {
                    // x_j is fixed, so move it to RHS
                    b.row(i) += RowVector2f{DEPTH_STEP/neighbors.size(), DEPTH_STEP/neighbors.size()};
                } else {
                    triplets.push_back(T(i, j, -1.f));
                }

                triplets.push_back(T(i, i, 1.f));
            }
        }
    }
    L.setFromTriplets(triplets.begin(), triplets.end());
    //assert(isSPD(L));
}

QImage NormalMapGenerator::generate(){
    SimplicialLDLT<Eigen::SparseMatrix<float>> solver;
    solver.compute(L);
    assert(solver.info() == Eigen::Success &&
           "solver failed to compute Cholesky factorization.");

    MatrixXf n{solver.solve(b)};

    QImage nm(width, height, QImage::Format_RGB32);
    QRgb *data = reinterpret_cast<QRgb*>(nm.bits());
    eigenMatrixToQImage(n, data);
    return nm;
}

void NormalMapGenerator::eigenMatrixToQImage(MatrixXf mat, QRgb *data){

    assert(mat.rows() == width * height && "invalid row/col comb");
    assert(mat.cols() == 2 && "normal matrix bad size");

    // width = #cols = rowsize
    // height = #rows = colsize

    for (int y = 0; y < height; y++){
        for (int x = 0; x < width; x++){
            int index = y * width + x;
            Vector2f normComponents = (0.5*mat.row(index)).array()+0.5f;
            float nx = normComponents.x();
            float ny = normComponents.y();
            float i = 1-(nx*nx)-(ny*ny);
            float nz = 0;
            if (i > 0)
                nz = sqrt(1-(nx*nx)-(ny*ny));
            int r = std::clamp(nx, 0.f, 1.f)*255;
            data[index] = qRgb(std::clamp(nx, 0.f, 1.f)*255,
                               std::clamp(ny, 0.f, 1.f)*255,
                               std::clamp(nz, 0.f, 1.f)*255);
        }
    }
}



//void computeImageGradient(QImage img); //(use sobel filter);

