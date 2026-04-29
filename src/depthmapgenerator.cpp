// #include "depthmapgenerator.h"

// /// TOOD: constructor

// NormalMapGenerator::NormalMapGenerator(QImage* image) :
//     dims(image->size()),
//     original_image(image)
// {
//     constructLaplacian();
// };


// bool onSilhouette(){
//     // dirichlet boundary condition
//     return false;
// }

// bool belowOcclusion(){
//     // neumann boundary condition
//     return false;
// }


// /*
//  ScribbleInfo(int, int);
//  SegmentInfo(int, int);

//  map <int, scribbles>
//  map <int, segments>

//  colorize
// */

// void NormalMapGenerator::constructLaplacian(const unordered_set<int>& boundary_vals) {
//     typedef Eigen::Triplet<double> T; // why is this double & not float again?

//     int width = dims.width();
//     int height = dims.height();

//     int n = width*height;
//     L.resize(n, n);
//     L.setZero();

//     std::vector<T> triplets;
//     for (int i = 0; i < n; i++){
//         unordered_set<int> neighbors = getValidNeighbors(i, height, width);
//         int neighbor_count = neighbors.size();

//         float weight = 1.f;

//         if (!boundary_vals.contains(i)){
//             triplets.push_back(T(i, i, neighbor_count));
//         } else {
//             triplets.push_back(T(i, i, 1.f));
//             continue;
//         }

//         for (int j : neighbors){
//             /// Dirichlet condition: sum of lapacian entries (i, j)
//             /// for all neighbors j in the row i must be equal to d'pq
//             ///     aka: if dp > dq
//             ///
//             //if (!boundary_vals.contains(i) && !boundary_vals.contains(j)){
//             triplets.push_back(T(i, j, weight));
//             //}
//         }
//     }
//     L.setFromTriplets(triplets.begin(), triplets.end());

//     assert(isSPD(L));
// }

// QImage NormalMapGenerator::generate(QImage img, QImage layer_map){
//     int width = dims.width();
//     int height = dims.height();

//     QImage nm(width, height, QImage::Format_RGB32);
//     QRgb *data = reinterpret_cast<QRgb*>(nm.bits());

//     SimplicialLDLT<Eigen::SparseMatrix<float>> solver;
//     solver.compute(L);
//     assert(solver.info() == Eigen::Success &&
//            "solver failed to compute Cholesky factorization.");

//     MatrixXf x{solver.solve(MatrixXf::Zero(width*height, 3))};

//     for (){
//         x.row() = ;
//     }

//     if (belowOcclusion()){

//     }

//     eigenMatrixToQImage(x, data);
//     return nm;
// }

// void NormalMapGenerator::eigenMatrixToQImage(MatrixXf mat, QRgb *data){
//     int width = dims.width();
//     int height = dims.height();

//     assert(mat.rows() == width * height && "invalid row/col comb");

//     // width = #cols = rowsize
//     // height = #rows = colsize

//     for (int x = 0; x < width; x++){
//         for (int y = 0; y < height; y++){

//             int index = x * width + y;
//             Vector3f normColor = (0.5*mat.row(index).normalized()).array()+0.5f;
//             data[index] = qRgb(std::clamp(normColor.x(), 0.f, 1.f),
//                                std::clamp(normColor.y(), 0.f, 1.f),
//                                std::clamp(normColor.z(), 0.f, 1.f));
//         }
//     }
// }

// //void computeImageGradient(QImage img); //(use sobel filter);

