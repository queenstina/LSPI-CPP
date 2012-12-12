#include "stdafx.h"
#include "LspiAgent.h"

#define NUM_ACTIONS 3
#define BASIS_SIZE 10
#define SIGMA_2 1

using namespace arma;
using namespace std;

/**
 * To create an LSPI Agent, a discount factor and a large number of sample data points are required. More sample should result in a better policy.
 * The samples should come from data taken from an agent performing at random, or a previous iteration of an LSPI Agent.
 *
 * Each sample in the vector should be of the format [x, v, a, r, x', v', t]
 * -x is the angle
 * -v is the angular velocity
 * -a is action selected
 * -r is the reward received after executing the action
 * -x' is the angle after executing the action
 * -v' is the angular velocity after executing the action
 * -t is 1 if the state after executing is terminal, 0 otherwise
 */
LspiAgent::LspiAgent(std::vector<array<double, 7>> samples, double disc)
{
	discount = disc;
	w = MatrixOps::vec_zeros(BASIS_SIZE*NUM_ACTIONS);

	// Loop until policy converges
	MatrixOps::vector policy = lstdq(samples);
	while(MatrixOps::mag_diff_vec(w, policy) > epsilon_const)
	{
		w = policy;
		policy = lstdq(samples);
	}

	w = policy;
}

/**
 * After creation, the LSPI Agent's policy is used to generate a functional value at a given angle and velocity. This functional value defines the action
 * the agent intends to take.
 */
int LspiAgent::getAction(double x, double v)
{
	int action = -9999;
	double max = -9999;
	int options[3] = {RF_OPT, NF_OPT, LF_OPT};
	for(int i = 0; i < 3; i++)
	{
		MatrixOps::vector params = basis_function(x, v, options[i]);
		double q = dot(params, w);
		if(q > max)
		{
			action = options[i];
			max = q;
		}
	}

	return action;
}
		
/**
 * Given a set of samples, performs a single update step on the current agent's policy.
 */
MatrixOps::vector LspiAgent::lstdq(std::vector<array<double, 7>> samples)
{
	MatrixOps::matrix B = 0.1*eye(BASIS_SIZE*NUM_ACTIONS, BASIS_SIZE*NUM_ACTIONS);
	MatrixOps::vector b = MatrixOps::vec_zeros(BASIS_SIZE*NUM_ACTIONS);
	for(int i = 0; i < samples.size(); i++)
	{
		// Get the basis functions
		MatrixOps::vector phi = basis_function(samples[i][0], samples[i][1], samples[i][2]);
		int next_action = getAction(samples[i][4], samples[i][5]);
		MatrixOps::vector phi_prime = basis_function(samples[i][4], samples[i][5], next_action);

		// Break the calculation into smaller parts
		MatrixOps::vector temp = phi - discount*phi_prime;
		MatrixOps::matrix num = B*phi*(temp.t()*B);
		double denom = 1.0 + dot((temp.t()*B), phi);
		B = B - num/denom;

		// Update values
		b = b + phi*samples[i][3];
	}
			
	return B*b;
}

/**
 * Returns the policy function weights for the given angle, velocity, and action.
 * These weights can be used to compute the estimated fitness of the given action.
 */
MatrixOps::vector LspiAgent::basis_function(double x, double v, int action)
{
	MatrixOps::vector phi = MatrixOps::vec_zeros(BASIS_SIZE*NUM_ACTIONS);

	// If we're horizontal then the basis function is all 0s
	if (fabs(x) - M_PI/(2.0) >= 0)
	{
		return phi;
	}

	// Now populate the basis function for this state action pair
	// Note that each entry except for the first is a gaussian.
	int i = BASIS_SIZE * (action);
	MatrixOps::vec_set(phi, i, 1.0);
	i += 1;
	double value = M_PI/2.0;
	for(double j = -value; j <= value; j += (value/((BASIS_SIZE-1)/6)))
	{
		for(double k = -1; k <= 1; k += 1)
		{
			double dist = (x - j)*(x - j) + (v - k)*(v - k);
			MatrixOps::vec_set(phi, i, exp(-dist/(2*SIGMA_2)));
			i += 1;
		}
	}

	return phi;
}