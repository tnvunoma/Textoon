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

inline unordered_set<int> getValidNeighbors(int index, int rows, int cols){
    unordered_set<int> neighbors;
    if (inBounds(index, -cols, cols, rows*cols)){
        neighbors.insert(index-cols);
    }

    if (inBounds(index, cols, cols, rows*cols)){
        neighbors.insert(index+cols);
    }

    if (inBounds(index, 1, cols, rows*cols)){
        neighbors.insert(index+1);
    }

    if (inBounds(index, -1, cols, rows*cols)){
        neighbors.insert(index-1);
    }

    // NSWE order, not minding exclusions due to bounds failures
    return neighbors;
}


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

    bool isBoundary(QRgb *data, int i, unordered_set<int> neighbors);
    void constructMatrices();
    //QImage computeImageGradient(QImage img); // use sobel filter?;
    // void computeBoundaryValues();
    // void computeIntermediateValues();

private:
    void eigenMatrixToQImage(MatrixXf mat, QRgb *data);

};
