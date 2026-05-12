#include "normalmapgenerator.h"
#include <iostream>
#include <tuple>
#include <random>


/// TOOD: constructor
const float DEPTH_STEP = 1.f; // change in dpeth from one level to the next
const float HA_STEP = 0.01f; // change in height along the normal direction

NormalMapGenerator::NormalMapGenerator(QImage* image) :
    height(image->size().height()),
    width(image->size().width()),
    original_image(image)
{
    constructMatrices();
};

bool NormalMapGenerator::onAnySilhouette(QRgb *data, int i, std::vector<std::unique_ptr<NeighboringPixel>>& neighbors){
    // dirichlet boundary condition
    // though this works, the name's a misnomer: it really tells you whether i is below any occlusing object
    // in any direction
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
    int num = width*height;
    L.resize(num, num);
    L.setZero();
    b.resize(num, 2);
    b.setZero();

    QRgb *data = reinterpret_cast<QRgb*>(original_image->bits());

    std::unordered_set<int> boundary_vals{};

    for (int i = 0; i < num; i++){
        /// enforces Dirichlet condition: sum of lapacian entries (i, j)
        /// for all neighbors j in the row i must be equal to d'pq
        std::vector<std::unique_ptr<NeighboringPixel>> neighbors = getValidNeighbors(i, height, width);

        // if (i == 813+(width*380)){
        //     //const auto& [row, col] = convertToRC(i);
        //     std::cout << "Here!" << std::endl;

        // }

        //if (){

        bool onAnySilhouette = false;

        int size = 0;

        // Handle
        for (const auto& n : neighbors){

            int j = n->index;
            direction drct = n->d;
            if (onSilhouette(data, i, j)){
                onAnySilhouette = true;
                        /// add depth step to RHS
                if (drct == NORTH || drct == SOUTH){
                    int sign = 1 + (n->d % 2) * -2;
                    b.row(i) = RowVector2f{0.f, DEPTH_STEP*sign};
                } else {
                    int sign = -1 + (n->d % 2) * 2;
                    b.row(i) = RowVector2f{DEPTH_STEP*sign, 0.f};
                }
                size++;
            }
        }

        // if (size >= 1)
        //     b.row(i) /= size;

        for (const auto& n : neighbors){
            int j = n->index;
            if (onAnySilhouette && !onSilhouette(data, i, j)){
                /// add depth step to RHS
                b.row(j) += b.row(i)/size;
            }
        }

        if (onAnySilhouette){
            // dirichlet constraint
            triplets.push_back(T(i, i, 1.f));
            boundary_vals.insert(i);
            continue;
        }

        for (const auto& n : neighbors){
            int j = n->index;

            if (!boundary_vals.contains(j)){
                triplets.push_back(T(i, j, -1.f));

                // if (belowOccludingObject(data, i, j)){
                //     NeighboringPixel * j_opp{};
                //     if ((j_opp = n->opposite_dir)){
                //         direction j_opposite_dir{j_opp->d};
                //         if (j_opposite_dir == NORTH || j_opposite_dir == SOUTH){
                //             //b.row(i) += RowVector2f{0.f, HA_STEP};
                //         } else {
                //             //b.row(i) += RowVector2f{HA_STEP, 0.f};
                //         }
                //         int j_opposite_index{j_opp->index};
                //         triplets.push_back(T(i, j_opposite_index, -1.f));
                //     }
                // } else {
                //     triplets.push_back(T(i, j, -1.f));
                // }
            }

            triplets.push_back(T(i, i, 1.f));
        }
    }

    L.setFromTriplets(triplets.begin(), triplets.end());
    // std::cout << Eigen::VectorXf(L.row(5))[5] << std::endl;
    // std::cout << Eigen::VectorXf(L.row(6))[6] << std::endl;
    // std::cout << Eigen::VectorXf(L.row(9))[9] << std::endl;
    // std::cout << Eigen::VectorXf(L.row(10))[10] << std::endl;

    // std::cout << "=======================" << std::endl;
    // std::cout << Eigen::VectorXf(b.row(6)) << std::endl;
    //assert(L.isApprox(L.transpose()));
    //assert(isSPD(L));
}

QImage NormalMapGenerator::generate(){
    SimplicialLDLT<Eigen::SparseMatrix<float>> solver;
    solver.compute(L);


    if (solver.info() != Eigen::Success) {
        std::cerr <<
           "solver failed to compute Cholesky factorization."
                  << std::endl;
    }

    MatrixXf n{solver.solve(b)};

    QImage nm(width, height, QImage::Format_ARGB32);
    QRgb *data = reinterpret_cast<QRgb*>(nm.bits());
    eigenMatrixToQImage(n, data);
    // std::cout << qBlue(nm.pixel(0, 0)) << std::endl;
    // std::cout << qBlue(nm.pixel(1, 0)) << std::endl;
    // std::cout << qBlue(nm.pixel(2, 0)) << std::endl;

    return nm;
}

void NormalMapGenerator::eigenMatrixToQImage(MatrixXf mat, QRgb *data){

    assert(mat.rows() == width * height && "invalid row/col comb");
    assert(mat.cols() == 2 && "normal matrix bad size");

    // width = #cols = rowsize
    // height = #rows = colsize

    for (int y = 0; y < height; y++){
        for (int x = 0; x < width; x++){
            // if (y == 1 && x == 6){
            //     std::cout << "here!" << std::endl;
            // }

            int index = y * width + x;
            Vector2f normComponents = mat.row(index);

            float nx = normComponents.x();
            float ny = normComponents.y();
            float i = 1.f-(nx*nx)-(ny*ny);

            float nz = 0.f;
            if (i > 0)
                nz = sqrt(i);


            Vector3f normAsColors{nx*0.5f+0.5f, ny*0.5f+0.5f, nz};

            //assert(i <= 1);

            QRgb color{qRgb(std::clamp(normAsColors.x(), 0.f, 1.f)*255,
                            std::clamp(normAsColors.y(), 0.f, 1.f)*255,
                            std::clamp(normAsColors.z(), 0.f, 1.f)*255)};

            data[index] = color;

        }
    }
}



//void computeImageGradient(QImage img); //(use sobel filter);

