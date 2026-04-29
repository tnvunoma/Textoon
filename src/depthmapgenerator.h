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
inline bool outOfBounds(int index, int increment, int row_size, int size ){
    int newIndex = index+increment;
    bool rowBounds = (index > size) || (index < 0); // are we exceeding row bounds?
    bool colBounds = newIndex/row_size != index/row_size; // column consistency - are we exceeding column bounds?
    // alt condition: = newIndex % row_size > col or newIndex % row_size < 0
    return rowBounds && colBounds;
}

inline unordered_set<int> getValidNeighbors(int index, int row, int col){
    unordered_set<int> neighbors;
    if (!outOfBounds(index, -col, col, row*col)){
        neighbors.insert(index-col);
    }

    if (!outOfBounds(index, col, col, row*col)){
        neighbors.insert(index+col);
    }

    if (!outOfBounds(index, 1, col, row*col)){
        neighbors.insert(index+1);
    }

    if (!outOfBounds(index, -1, col, row*col)){
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

    QImage* original_image; // image gradient - generated with sobel filter (could use opencv)

    QImage I; // image gradient - generated with sobel filter (could use opencv)
    SparseMatrix<float> L; // laplacian for depth inequalities
    QSize dims;

    NormalMapGenerator(QImage* image);

    QImage generate(QImage img, QImage layer_map);

    void constructLaplacian();
    //QImage computeImageGradient(QImage img); // use sobel filter?;
    // void computeBoundaryValues();
    // void computeIntermediateValues();

private:
    void eigenMatrixToQImage(MatrixXf mat, QRgb *data);

};
