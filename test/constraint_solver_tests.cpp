#include <gtest/gtest.h>

#include "gauss_seidel_sle_solver.h"
#include "matrix.h"
#include "sparse_matrix.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace {

class ExposedGaussSeidel : public atg_scs::GaussSeidelSleSolver {
public:
    using GaussSeidelSleSolver::solveIteration;
};

} // namespace

namespace {

bool sameBits(double a, double b) {
    uint64_t left = 0;
    uint64_t right = 0;
    std::memcpy(&left, &a, sizeof(left));
    std::memcpy(&right, &b, sizeof(right));
    return left == right;
}

void referenceLimitedIteration(
        atg_scs::Matrix &left,
        atg_scs::Matrix &right,
        atg_scs::Matrix &limits,
        atg_scs::Matrix &k)
{
    const int n = k.getHeight();
    for (int i = 0; i < n; ++i) {
        double s0 = 0.0;
        double s1 = 0.0;
        for (int j = 0; j < i; ++j)
            s0 += left.get(j, i) * k.get(0, j);
        for (int j = i + 1; j < n; ++j)
            s1 += left.get(j, i) * k.get(0, j);

        const double kNext = (1.0 / left.get(i, i)) * (right.get(0, i) - s0 - s1);
        const double x = std::fmax(limits.get(0, i), std::fmin(limits.get(1, i), kNext));
        k.set(0, i, x);
    }
}

void fillDominantSystem(int n, atg_scs::Matrix &left, atg_scs::Matrix &right, atg_scs::Matrix &limits, atg_scs::Matrix &k) {
    left.initialize(n, n, 0.0);
    right.initialize(1, n, 0.0);
    limits.initialize(2, n, 0.0);
    k.initialize(1, n, 0.0);
    for (int row = 0; row < n; ++row) {
        for (int column = 0; column < n; ++column) {
            const double value = (row == column)
                ? static_cast<double>(n) + 2.0
                : 0.05 * std::sin(0.3 * row + 0.7 * column);
            left.set(column, row, value);
        }
        right.set(0, row, std::cos(0.2 * row));
        limits.set(0, row, -4.0);
        limits.set(1, row, 4.0);
        k.set(0, row, 0.1 * row);
    }
}

} // namespace

TEST(ConstraintSolverTests, GaussSeidelMatchesScalarIteration) {
    const int sizes[] = {1, 2, 3, 7, 8, 31};
    for (int n : sizes) {
        atg_scs::Matrix left;
        atg_scs::Matrix right;
        atg_scs::Matrix limits;
        atg_scs::Matrix actual;
        atg_scs::Matrix expected;
        fillDominantSystem(n, left, right, limits, actual);
        expected.set(&actual);

        ExposedGaussSeidel solver;
        for (int iteration = 0; iteration < 5; ++iteration) {
            solver.solveIteration(left, right, limits, &actual, &actual);
            referenceLimitedIteration(left, right, limits, expected);
            for (int row = 0; row < n; ++row)
                EXPECT_TRUE(sameBits(actual.get(0, row), expected.get(0, row)))
                    << "n=" << n << " iteration=" << iteration << " row=" << row;
        }

        left.destroy();
        right.destroy();
        limits.destroy();
        actual.destroy();
        expected.destroy();
    }
}

TEST(ConstraintSolverTests, MultiplyTransposeMatchesExpandedDot) {
    const int heights[] = {1, 2, 5};
    for (int height : heights) {
        atg_scs::SparseMatrix<3> left;
        atg_scs::SparseMatrix<3> right;
        left.initialize(6, height);
        right.initialize(6, height);
        for (int row = 0; row < height; ++row) {
            // Entry order is column order. A later entry for an earlier block
            // adds those products first and will not match a left-to-right dot.
            left.setBlock(row, 0, 0);
            left.setBlock(row, 1, 1);
            right.setBlock(row, 0, static_cast<uint8_t>(row % 2));
            right.setBlock(row, 1, static_cast<uint8_t>(1 - (row % 2)));
            for (int slice = 0; slice < 3; ++slice) {
                left.set(row, 0, slice, 0.25 * (row + 1) + 0.1 * slice);
                left.set(row, 1, slice, -0.15 * (row + 1) + slice);
                right.set(row, 0, slice, 0.4 * (row + 2) - slice);
                right.set(row, 1, slice, 0.05 * row + 0.2 * slice);
            }
        }

        atg_scs::Matrix actual;
        left.multiplyTranspose(right, &actual);

        atg_scs::Matrix leftDense;
        atg_scs::Matrix rightDense;
        left.expand(&leftDense);
        right.expand(&rightDense);
        ASSERT_EQ(actual.getWidth(), height);
        ASSERT_EQ(actual.getHeight(), height);
        for (int row = 0; row < height; ++row) {
            for (int column = 0; column < height; ++column) {
                double dot = 0.0;
                for (int k = 0; k < leftDense.getWidth(); ++k)
                    dot += leftDense.get(k, row) * rightDense.get(k, column);
                EXPECT_TRUE(sameBits(actual.get(column, row), dot))
                    << "height=" << height << " row=" << row << " column=" << column;
            }
        }

        actual.destroy();
        leftDense.destroy();
        rightDense.destroy();
        left.destroy();
        right.destroy();
    }
}
