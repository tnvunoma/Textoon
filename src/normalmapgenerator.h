#pragma once

#include <QImage>
#include <QVector>
#include <Eigen/Dense>
#include <Eigen/SparseCholesky>

#include "scribblecontext.h"
#include <set>
#include <unordered_set>
#include "util/vector_utils.h"
#include <algorithm>

using namespace Eigen;
using namespace std;

struct DepthInequality{
    QPoint start_p;
    QPoint end_p;
};

struct DepthInequalityGraph {
    QVector<DepthInequality> de;
    std::set<Vector2f> path;
};

enum direction {
    NORTH,
    SOUTH,
    WEST,
    EAST
};

struct NeighboringPixel {
    int index;
    direction d;
    NeighboringPixel* opposite_dir;

    bool operator==(const NeighboringPixel& other) const {
        return index == other.index &&
               d == other.d &&
               opposite_dir == other.opposite_dir;
    }

    NeighboringPixel(int idx, direction dir) : index(idx), d(dir) {}
};

// struct NeighboringPixelHash {
//     std::size_t operator()(const NeighboringPixel& p) const {
//         std::size_t h1 = std::hash<int>{}(p.index);
//         std::size_t h2 = std::hash<int>{}(static_cast<int>(p.d));
//         std::size_t h3 = std::hash<void*>{}(static_cast<void*>(p.opposite_dir));

//         return h1 ^ (h2 << 1) ^ (h3 << 2);
//     }
// };
/*
pixel utilities

*/
inline bool inBounds(int index, int increment, int row_size, int size ){
    int newIndex = index+increment;
    bool rowBounds = newIndex < size && newIndex >= 0; // are we exceeding row bounds?
    bool colBounds = (abs(increment) != 1 || newIndex/row_size == index/row_size); // column consistency - are we exceeding column bounds?
    // alt condition: = newIndex % row_size > col or newIndex % row_size < 0

    return rowBounds && colBounds;
}

inline std::vector<std::unique_ptr<NeighboringPixel>> getValidNeighbors(int index, int rows, int cols){
    /// TODO: fix pointer...
    ///
    std::vector<std::unique_ptr<NeighboringPixel>> neighbors;
    NeighboringPixel *north = nullptr, *south = nullptr, *east = nullptr, *west = nullptr;
    if (inBounds(index, -cols, cols, rows*cols)){
        neighbors.push_back(make_unique<NeighboringPixel>(index-cols, NORTH));
    }
    if (inBounds(index, cols, cols, rows*cols)){
        neighbors.push_back(make_unique<NeighboringPixel>(index+cols, SOUTH));
        south = neighbors.back().get();
    }

    if (inBounds(index, -1, cols, rows*cols)){
        neighbors.push_back(make_unique<NeighboringPixel>(index-1, WEST));
        west = neighbors.back().get();
    }

    if (inBounds(index, 1, cols, rows*cols)){
        neighbors.push_back(make_unique<NeighboringPixel>(index+1, EAST));
        east = neighbors.back().get();
    }

    if (north && south) {
        north->opposite_dir = south;
        south->opposite_dir = north;
    }
    if (west && east) { west->opposite_dir = east; east->opposite_dir = west; }

    // NSWE order, not minding exclusions due to bounds failures
    return neighbors;

}

// inline unordered_set<int> getValidNeighbors(int index, int rows, int cols){
//     unordered_set<int> neighbors;
//     if (inBounds(index, -cols, cols, rows*cols)){
//         neighbors.insert(index-cols);
//     }

//     if (inBounds(index, cols, cols, rows*cols)){
//         neighbors.insert(index+cols);
//     }

//     if (inBounds(index, 1, cols, rows*cols)){
//         neighbors.insert(index+1);
//     }

//     if (inBounds(index, -1, cols, rows*cols)){
//         neighbors.insert(index-1);
//     }

//     // NSWE order, not minding exclusions due to bounds failures
//     return neighbors;
// }


/*
 * Get normal field
 *
 * Solve for the normal field for a given image
 * Steps:
    * contours: set up Dirichlet-Neumann boundary constraints (in other words: use a layer map to indicate ordering of
        * segmentations)
        * Result: homogenous Laplacian, by which you should verify => normal field for given boundary values
    * intermediate values: use the image gradient values and Lambertian surface calcualtion calculate the x, y, and z values
        * of normals directly
        * Result: your normal field
 * Impl:
 *
 *
 * MatrixXf constructLaplacian()
 *
 * QImage computeImageGradient(QImage img) (use sobel filter)
 *
 * void computeBoundaryValues()
 * void computeIntermediateValues()
 *
*/

/*
 * (for another class) Mimic texture rounding
 *
 * Then, solve for the virtual 3D surface of that image, given the normal field
    * Compute the derivative of the normal field (aka:
 *
*/
class NormalMapGenerator
{
public:

    QImage* original_image;

    QImage layer_map; // can compute from grayscale
    SparseMatrix<float> L; // laplacian for depth inequalities
    MatrixXf b;
    int height, width;

    NormalMapGenerator(QImage* image);

    QImage generate();


private:
    std::pair<int, int> convertToRC(int index);
    void eigenMatrixToQImage(MatrixXf mat, QRgb *data);
    void constructMatrices();
    bool onAnySilhouette(QRgb *data, int i, std::vector<std::unique_ptr<NeighboringPixel>>& neighbors);
    bool onSilhouette(QRgb *data, int i, int j);
    bool belowOccludingObject(QRgb *data, int i, int j);

};
