/* 
 * Copyright 2015 FLR Team. Distributed under the GPL 2 or later
 * Maintainer: Finlay Scott, JRC
 */

// For timing functions
#include <time.h>

#include "../inst/include/solver.h"

double euclid_norm(std::vector<double> x){
    double xsum = std::inner_product(x.begin(), x.end(), x.begin(), 0.0); // must be 0.0 else automatically casts to int (argh)
    xsum = sqrt(xsum);
    return xsum;
}

// We should offer the option of doing iterations in chunks - i.e. if 5000 iters, do 1000 at a time else Jacobian becomes massive and we hit memory problems
// And we need to make sure that solving for multiple targets (e.g. if two fleets and we have two fmults) works
// So we pass in the number of iterations we want to solve, and how many simultaneous targets there in that iteration, i.e. the dimension of the problem
// As each iteration is indpendent, we solve each iteration of simultaneous targets separately
// find x: f(x) = 0
// x1 = x0 - f(x0) / f'(x0)
// w = f(x0) / f'(x0)
// We want w.
// Rearrange w:
// f'(x0) w = f(x0)
// We use LU solve, give it f'(x0) and f(x0) to get w
// x1 = x0 - w
// Need to add max_limit

/*! \brief A simple Newton-Raphson optimiser
 *
 * The Newton-Raphons optimiser uses the Jacobian matrix calculated by CppAD.
 * As all iterations of the simulations can be run simultaneously the Jacobian can be treated in discrete chunks along the diagonal.
 * Also, the Jacobian matrix can be very sparse. 
 * This implementation therefore solves each 'chunk' independently to save inverting a massive matrix.
 * The size of each chunk is given by the number of simulation targets (the parameter nsim_targets).
 * Iterations continue either all the chunks are solved to within the desired tolerance or the maximum number of iterations has been hit.
 * \param indep The initial values of the independent values.
 * \param fun The CppAD function object.
 * \param niter The number of iterations in the simulations.
 * \param nsim_targets The number of targets to solve for in each iteration (determines of the size of the Jacobian chunks).
 * \param indep_min The minimum value of the independent variable (default is 0).
 * \param indep_max The maximum value of the independent variable (default is 1000).
 * \param max_iters The maximum number of solver iterations (not FLR iterations).
 * \param tolerance The tolerance of the solutions.
 */
std::vector<int> newton_raphson(std::vector<double>& indep, CppAD::ADFun<double>& fun, const int niter, const int nsim_targets, const double indep_min, const double indep_max, const int max_iters, const double tolerance){
    Rprintf("In newton raphson\n");
    // Check that product of niter and nsim_targets = length of indep (otherwise something has gone wrong)
    if (indep.size() != (niter * nsim_targets)){
        Rcpp::stop("In newton_raphson: length of indep does not equal product of niter and nsim_targets\n");
    }

    double logdet = 0.0; // Not sure what this actually does but is used in the CppAD LUsolve function
    std::vector<double> y(niter * nsim_targets, 1000.0);
    std::vector<double> delta_indep(niter * nsim_targets, 0.0); // For updating indep in final step
    std::vector<double> jac(niter * nsim_targets * niter * nsim_targets);
    std::vector<double> iter_jac(nsim_targets * nsim_targets);
    std::vector<double> iter_y(nsim_targets);
    std::vector<int> iter_solved(niter, 0); // If 0, that iter has not been solved
    int jac_element = 0; // an index for Jacobian used for subsetting the Jacobian for each iter

    // Reasons for stopping
    //  1 - Solved within tolerance
    // -1 - Itertion limit reached;
    // -2 - Min limit reached;
    // -3 - Max limit reached
    std::vector<int> success_code (niter, -1); 
    int nr_count = 0;
    // Keep looping until all sim_targets have been solved, or number of iterations (NR iterations, not FLR iterations) has been hit
    
    while((std::accumulate(iter_solved.begin(), iter_solved.end(), 0) < niter) & (nr_count < max_iters)){ 
        ++nr_count;
        Rprintf("nr_count: %i\n", nr_count);
        // Get y = f(x0)
        //Rprintf("Forward\n");
        y = fun.Forward(0, indep); 
        //Rprintf("indep: %f\n", indep[0]);
        //Rprintf("y: %f\n", y[0]);
        // Get f'(x0) -  gets Jacobian for all simultaneous targets
        //Rprintf("Getting SparseJacobian\n");
        jac = fun.SparseJacobian(indep);
        //Rprintf("Got it\n");
        // Get w (f(x0) / f'(x0)) for each iteration if necessary
        // Loop over simultaneous targets, solving if necessary
        for (int iter_count = 0; iter_count < niter; ++iter_count){
            //Rprintf("iter_count: %i\n", iter_count);
            // Only solve if that iter has not been solved
            if(iter_solved[iter_count] == 0){
                //Rprintf("Iter %i not yet solved\n", iter_count);
                // Subsetting y and Jacobian for that iter only
                //Rprintf("Subsetting\n");
                for(int jac_count_row = 0; jac_count_row < nsim_targets; ++jac_count_row){
                    iter_y[jac_count_row] = y[iter_count * nsim_targets + jac_count_row];
                    // Fill up mini Jacobian for that iteration 
                    for(int jac_count_col = 0; jac_count_col < nsim_targets; ++jac_count_col){
                        jac_element = (iter_count * niter * nsim_targets * nsim_targets) + (iter_count * nsim_targets) + jac_count_row + (jac_count_col * niter * nsim_targets);
                        iter_jac[jac_count_row + (jac_count_col * nsim_targets)] = jac[jac_element];
                    }
                }
                //Rprintf("Done subsetting\n");
                // Solve to get w = f(x0) / f'(x0)
                // Puts resulting w (delta_indep) into iter_y
                //Rprintf("LU Solving\n");
                //Rprintf("nsim_targets: %i\n", nsim_targets);
                //Rprintf("iter_jac: %f\n", iter_jac[0]);
                //Rprintf("iter_y: %f\n", iter_y[0]);
                CppAD::LuSolve(nsim_targets, nsim_targets, iter_jac, iter_y, iter_y, logdet); 
                //Rprintf("Done LU Solving\n");
                // Has iter now been solved? If so, set the flag to 1
                if (euclid_norm(iter_y) < tolerance){
                    iter_solved[iter_count] = 1;
                    success_code[iter_count] = 1;
                }
                // put iter_y into delta_indep - needs for loop
                for(int jac_count = 0; jac_count < nsim_targets; ++jac_count){
                    delta_indep[iter_count * nsim_targets + jac_count] = iter_y[jac_count];
                }
            }
        }
        // Update x = x - w
        // Ideally should only update the iterations that have not hit the tolerance
        std::transform(indep.begin(),indep.end(),delta_indep.begin(),indep.begin(),std::minus<double>());

        // indep cannot be less than minimum value or greater than maximum value
        for (auto minmax_counter = 0; minmax_counter < indep.size(); ++minmax_counter){
            // Have we breached min limit?
            if (indep[minmax_counter] < indep_min){
                indep[minmax_counter] = indep_min;
                // Which iter is this
                success_code[minmax_counter / nsim_targets] = -2;
            }
            // Have we breached max limit?
            if (indep[minmax_counter] > indep_max){
                indep[minmax_counter] = indep_max;
                success_code[minmax_counter / nsim_targets] = -3;
            }
        } 
    }
    return success_code;
}
