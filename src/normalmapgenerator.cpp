#include "normalmapgenerator.h"
#include <iostream>
#include <tuple>
/// TOOD: constructor
const float DEPTH_STEP = 0.5f; // change in dpeth from one level to the next
const float HA_STEP = 0.0f; // change in height along the normal direction

NormalMapGenerator::NormalMapGenerator(QImage* image) :
    height(image->size().height()),
    width(image->size().width()),
    original_image(image)
{
    constructMatrices();
};

bool NormalMapGenerator::onAnySilhouette(QRgb *data, int i, std::vector<std::unique_ptr<NeighboringPixel>>& neighbors){
    // dirichlet boundary condition
    for (const auto& n : neighbors){
        int j = n->index;
        if (data[i] < data[j]){
            return true;
        }
    }
    return false;
}

bool NormalMapGenerator::onSilhouette(QRgb *data, int i, int j){
    // dirichlet boundary condition

    return data[i] > data[j];
}

bool NormalMapGenerator::belowOccludingObject(QRgb *data, int i, int j){
    // neumann boundary condition
    // QRgb iColor = data[i];
    // QRgb jColor = data[j];

    return data[i] < data[j];
}

std::pair<int, int> NormalMapGenerator::convertToRC(int index){
    //assert(inBounds(index));
    int r = index / width;
    int c = index % width;
    return std::pair<int, int>(r, c);
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
        std::vector<std::unique_ptr<NeighboringPixel>> neighbors = getValidNeighbors(i, height, width);

        // if (i == 813+(width*380)){
        //     //const auto& [row, col] = convertToRC(i);
        //     std::cout << "Here!" << std::endl;

        // }
        if (onAnySilhouette(data, i, neighbors)){
            /// TODO: account for direction of boundary?

            /// add depth step to RHS while enforcing constraint
            triplets.push_back(T(i, i, 1.f));
            //b.row(i) = RowVector2f{DEPTH_STEP, DEPTH_STEP};
            // for (const auto& n : neighbors){
            //     int j = n->index;
            //     direction drct = n->d;

            //     if (onSilhouette(data, i, j)){
            //         /// add depth step to RHS
            //         //int sign = -1 + (n->d % 2) * 2;
            //         if (drct == NORTH || drct == SOUTH){
            //             b.row(i) += RowVector2f{DEPTH_STEP, 0.f};
            //         } else {
            //             b.row(i) += RowVector2f{0.f, DEPTH_STEP};
            //         }
            //     }
            // }
        }
        else {
            for (const auto& n : neighbors){
                int j = n->index;
                direction drct = n->d;

                // adding contributions...
                if (onSilhouette(data, i, j)){
                    /// add depth step to RHS
                    int sign = -1 + (n->d % 2) * 2;
                    if (drct == NORTH || drct == SOUTH){
                        b.row(i) += RowVector2f{DEPTH_STEP*sign, 0.f};
                    } else {
                        b.row(i) += RowVector2f{0.f, DEPTH_STEP*sign};
                    }
                }
                // else if (belowOccludingObject(data, i, j)){
                //     /// we're below a boundary, but suppose there was a ghost pixel there;
                //     /// we wnat the normals to be smooth and consistent with the neighbor in the opposite direciton and the ghost pixel.
                //     /// to do this, we can simply add the opposite direction neighbor in the laplacian and a height-step weight to the RHS

                //     if (n->opposite_dir){
                //         NeighboringPixel * j_opposite{n->opposite_dir};
                //         direction j_opposite_dir{j_opposite->d};

                //         int sign = -1 + (j_opposite_dir % 2) * 2;

                //         if (j_opposite_dir == NORTH || j_opposite_dir == SOUTH){
                //             b.row(i) += RowVector2f{0.f, HA_STEP*sign};

                //         } else {
                //             b.row(i) += RowVector2f{HA_STEP*sign, 0.f};
                //         }

                //         int j_opposite_index{j_opposite->index};
                //         triplets.push_back(T(i, j_opposite_index, -1.f));
                //     }
                // }
                else {
                    /// add standard laplacian contributions
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

    QImage nm(width, height, QImage::Format_ARGB32);
    QRgb *data = reinterpret_cast<QRgb*>(nm.bits());
    eigenMatrixToQImage(n, data);
    std::cout << qBlue(nm.pixel(0, 0)) << std::endl;
    std::cout << qBlue(nm.pixel(1, 0)) << std::endl;
    std::cout << qBlue(nm.pixel(2, 0)) << std::endl;

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
                nz = sqrt(i);
            assert(i <= 1);

            QRgb color{qRgb(std::clamp(nx, 0.f, 1.f)*255,
                            std::clamp(ny, 0.f, 1.f)*255,
                            std::clamp(nz, 0.f, 1.f)*255)};

            data[index] = color;

        }
    }
}



//void computeImageGradient(QImage img); //(use sobel filter);

